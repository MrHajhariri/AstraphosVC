#define _POSIX_C_SOURCE 200809L

#include "commits/avc_commit.h"

#include "hashing/avc_hash.h"
#include "utils/avc_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct tree_node {
    char *name;
    int is_tree;
    uint32_t mode;
    avc_oid oid;
    struct tree_node **children;
    int child_count;
    int child_capacity;
} tree_node;

static tree_node *tree_node_create(const char *name) {
    tree_node *node = (tree_node *)calloc(1, sizeof(*node));
    if (node == NULL) return NULL;
    node->name = strdup(name);
    if (node->name == NULL) {
        free(node);
        return NULL;
    }
    return node;
}

static void tree_node_free(tree_node *node) {
    if (node == NULL) return;
    free(node->name);
    for (int i = 0; i < node->child_count; i++) {
        tree_node_free(node->children[i]);
    }
    free(node->children);
    free(node);
}

static int tree_node_add_child(tree_node *parent, tree_node *child) {
    if (parent->child_count == parent->child_capacity) {
        int next = parent->child_capacity == 0 ? 8 : parent->child_capacity * 2;
        tree_node **c = (tree_node **)realloc(
            parent->children, (size_t)next * sizeof(tree_node *));
        if (c == NULL) return -1;
        parent->children = c;
        parent->child_capacity = next;
    }
    parent->children[parent->child_count++] = child;
    return 0;
}

static int tree_node_sort_cmp(const void *a, const void *b) {
    const tree_node *na = *(const tree_node **)a;
    const tree_node *nb = *(const tree_node **)b;
    if (na->is_tree != nb->is_tree) {
        return na->is_tree ? -1 : 1;
    }
    return strcmp(na->name, nb->name);
}

static void tree_node_sort(tree_node *node) {
    qsort(node->children, (size_t)node->child_count,
          sizeof(tree_node *), tree_node_sort_cmp);
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]->is_tree) {
            tree_node_sort(node->children[i]);
        }
    }
}

static char *next_slash(char *p) {
    for (; *p != '\0'; p++) {
        if (*p == '/') return p;
    }
    return NULL;
}

static tree_node *tree_node_find_child(tree_node *node, const char *name) {
    for (int i = 0; i < node->child_count; i++) {
        if (strcmp(node->children[i]->name, name) == 0) {
            return node->children[i];
        }
    }
    return NULL;
}

static avc_status build_tree_recursive(avc_odb *odb, tree_node *node,
                                       avc_oid out, avc_error *error) {
    if (!node->is_tree) {
        return AVC_OK;
    }

    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]->is_tree) {
            avc_status status = build_tree_recursive(
                odb, node->children[i], node->children[i]->oid, error);
            if (status != AVC_OK) return status;
        }
    }

    tree_node_sort(node);

    size_t total = 0;
    for (int i = 0; i < node->child_count; i++) {
        total += strlen(node->children[i]->name) + 7 + 20;
    }

    unsigned char *buf = (unsigned char *)malloc(total);
    if (buf == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory building tree");
        return AVC_ERR_NO_MEMORY;
    }

    unsigned char *p = buf;
    for (int i = 0; i < node->child_count; i++) {
        tree_node *child = node->children[i];
        const char *mode_str = child->is_tree ? "40000" : "100644";
        int entry_len = snprintf((char *)p, total - (size_t)(p - buf),
                                 "%s %s", mode_str, child->name);
        p += entry_len;
        *p++ = '\0';
        memcpy(p, child->oid, 20);
        p += 20;
    }

    size_t payload_size = (size_t)(p - buf);
    avc_status status = avc_odb_write(odb, AVC_OBJECT_TREE, buf,
                                       payload_size, out, error);
    free(buf);
    return status;
}

avc_status avc_commit_build_tree(avc_odb *odb, avc_index *index,
                                 avc_oid tree_oid, avc_error *error) {
    tree_node *root = tree_node_create("");
    if (root == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory building tree");
        return AVC_ERR_NO_MEMORY;
    }
    root->is_tree = 1;

    for (size_t i = 0; i < index->count; i++) {
        avc_index_entry *entry = &index->entries[i];

        char *path_copy = strdup(entry->path);
        if (path_copy == NULL) {
            tree_node_free(root);
            avc_error_set(error, AVC_ERR_NO_MEMORY,
                          "out of memory building tree");
            return AVC_ERR_NO_MEMORY;
        }

        tree_node *current = root;
        char *pos = path_copy;
        char *slash;

        while ((slash = next_slash(pos)) != NULL) {
            *slash = '\0';
            tree_node *child = tree_node_find_child(current, pos);
            if (child == NULL) {
                child = tree_node_create(pos);
                if (child == NULL) {
                    free(path_copy);
                    tree_node_free(root);
                    avc_error_set(error, AVC_ERR_NO_MEMORY,
                                  "out of memory building tree");
                    return AVC_ERR_NO_MEMORY;
                }
                child->is_tree = 1;
                tree_node_add_child(current, child);
            }
            current = child;
            pos = slash + 1;
        }

        tree_node *child = tree_node_find_child(current, pos);
        if (child == NULL) {
            child = tree_node_create(pos);
            if (child == NULL) {
                free(path_copy);
                tree_node_free(root);
                avc_error_set(error, AVC_ERR_NO_MEMORY,
                              "out of memory building tree");
                return AVC_ERR_NO_MEMORY;
            }
            child->is_tree = 0;
            child->mode = entry->mode;
            memcpy(child->oid, entry->oid, 20);
            tree_node_add_child(current, child);
        }

        free(path_copy);
    }

    avc_status status = build_tree_recursive(odb, root, tree_oid, error);
    tree_node_free(root);
    return status;
}

avc_status avc_commit_create(avc_odb *odb, const avc_oid tree_oid,
                             const avc_oid *parent_oids, int parent_count,
                             const avc_signature *author,
                             const avc_signature *committer,
                             const char *message, avc_oid out,
                             avc_error *error) {
    size_t cap = 4096;
    char *buf = (char *)malloc(cap);
    if (buf == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory creating commit");
        return AVC_ERR_NO_MEMORY;
    }
    size_t len = 0;

    char hex[AVC_OID_HEX_SIZE];

    avc_oid_hex(tree_oid, hex);
    len += (size_t)snprintf(buf + len, cap - len, "tree %s\n", hex);

    for (int i = 0; i < parent_count; i++) {
        avc_oid_hex(parent_oids[i], hex);
        len += (size_t)snprintf(buf + len, cap - len, "parent %s\n", hex);
    }

    char tz_str[16];
    int tz_abs = author->tz_offset;
    snprintf(tz_str, sizeof(tz_str), "%+03d%02d",
             tz_abs / 60, tz_abs % 60);

    len += (size_t)snprintf(buf + len, cap - len,
                            "author %s <%s> %ld %s\n",
                            author->name, author->email,
                            (long)author->timestamp, tz_str);

    tz_abs = committer->tz_offset;
    snprintf(tz_str, sizeof(tz_str), "%+03d%02d",
             tz_abs / 60, tz_abs % 60);

    len += (size_t)snprintf(buf + len, cap - len,
                            "committer %s <%s> %ld %s\n",
                            committer->name, committer->email,
                            (long)committer->timestamp, tz_str);

    len += (size_t)snprintf(buf + len, cap - len, "\n%s\n", message);

    avc_status status = avc_odb_write(odb, AVC_OBJECT_COMMIT, buf, len,
                                       out, error);
    free(buf);
    return status;
}

static int read_line(const unsigned char *data, size_t size, size_t *pos,
                     char *line, size_t line_size) {
    if (*pos >= size) return -1;
    size_t start = *pos;
    while (*pos < size && data[*pos] != '\n') {
        (*pos)++;
    }
    size_t line_len = *pos - start;
    if (line_len >= line_size) {
        line_len = line_size - 1;
    }
    memcpy(line, data + start, line_len);
    line[line_len] = '\0';
    if (*pos < size) {
        (*pos)++;
    }
    return (int)line_len;
}

avc_status avc_commit_parse(const unsigned char *payload, size_t size,
                            avc_oid *tree_oid,
                            avc_oid *parent_oids, int *parent_count,
                            char *author, size_t author_size,
                            char *committer, size_t committer_size,
                            char *message, size_t message_size,
                            avc_error *error) {
    (void)committer_size;
    size_t pos = 0;
    int parsed_parents = 0;
    int found_body = 0;

    if (parent_oids != NULL && parent_count != NULL) {
        *parent_count = 0;
    }

    while (pos < size) {
        char line[4096];
        if (read_line(payload, size, &pos, line, sizeof(line)) < 0) {
            break;
        }

        if (line[0] == '\0') {
            found_body = 1;
            break;
        }

        if (strncmp(line, "tree ", 5) == 0 && tree_oid != NULL) {
            if (avc_oid_parse(line + 5, *tree_oid) != 0) {
                avc_error_set(error, AVC_ERR_PARSE,
                              "invalid tree OID in commit");
                return AVC_ERR_PARSE;
            }
        } else if (strncmp(line, "parent ", 7) == 0) {
            if (parent_oids != NULL && parent_count != NULL &&
                *parent_count < 16) {
                if (avc_oid_parse(line + 7,
                                  parent_oids[*parent_count]) != 0) {
                    avc_error_set(error, AVC_ERR_PARSE,
                                  "invalid parent OID in commit");
                    return AVC_ERR_PARSE;
                }
                (*parent_count)++;
            }
            parsed_parents++;
        } else if (strncmp(line, "author ", 7) == 0 && author != NULL) {
            snprintf(author, author_size, "%s", line + 7);
        } else if (strncmp(line, "committer ", 10) == 0 &&
                   committer != NULL) {
            snprintf(committer, author_size, "%s", line + 10);
        }
    }

    if (!found_body) {
        avc_error_set(error, AVC_ERR_PARSE,
                      "commit message body not found");
        return AVC_ERR_PARSE;
    }

    size_t msg_start = pos;
    size_t msg_len = size - msg_start;
    if (message != NULL && message_size > 0) {
        size_t copy = msg_len < message_size - 1 ? msg_len : message_size - 1;
        memcpy(message, payload + msg_start, copy);
        message[copy] = '\0';
    }

    return AVC_OK;
}

avc_status avc_commit_log(avc_odb *odb, const avc_oid head_oid,
                          int max_count, avc_error *error) {
    avc_oid current;
    memcpy(current, head_oid, 20);

    int count = 0;
    while (count < max_count || max_count <= 0) {
        void *payload = NULL;
        size_t payload_size = 0;
        avc_object_type type;

        avc_error_clear(error);
        avc_status status = avc_odb_read(odb, current, &type, &payload,
                                          &payload_size, error);
        if (status != AVC_OK) {
            return status;
        }
        if (type != AVC_OBJECT_COMMIT) {
            free(payload);
            break;
        }

        char hex[AVC_OID_HEX_SIZE];
        avc_oid_hex(current, hex);
        printf("commit %s\n", hex);

        char author[256] = "";
        char msg[4096] = "";
        avc_oid tree_id, parent_ids[16];
        int parent_cnt = 0;

        avc_commit_parse((const unsigned char *)payload, payload_size,
                         &tree_id, parent_ids, &parent_cnt,
                         author, sizeof(author),
                         NULL, 0, msg, sizeof(msg), error);
        free(payload);

        printf("Author: %s\n", author);

        char *nl = strchr(msg, '\n');
        if (nl != NULL) *nl = '\0';
        printf("    %s\n\n", msg);

        count++;

        if (parent_cnt == 0) {
            break;
        }

        memcpy(current, parent_ids[0], 20);
    }

    return AVC_OK;
}
