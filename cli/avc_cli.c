#define _POSIX_C_SOURCE 200809L

#include "cli/avc_cli.h"

#include "api/astraphosvc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void print_help(void) {
    puts("AstraphosVC - modern distributed version control");
    puts("");
    puts("Usage:");
    puts("  astraphosvc <command> [options]");
    puts("");
    puts("Implemented commands:");
    puts("  init [path]             Initialize an AstraphosVC repository");
    puts("  add <path>              Stage file content");
    puts("  status                  Show working tree status");
    puts("  commit -m <message>     Record changes to the repository");
    puts("  branch [name]           List branches or create a new branch");
    puts("  checkout <branch>       Switch to a branch");
    puts("  merge <branch>          Merge a branch into the current branch");
    puts("  log                     Show commit history");
    puts("  version                 Print version information");
    puts("  help                    Print this help text");
    puts("");
    puts("See docs/cli-reference.md for planned commands.");
}

static int command_init(int argc, char **argv) {
    const char *path = argc >= 3 ? argv[2] : ".";
    avc_error error;
    avc_error_clear(&error);
    avc_repository repository = {0};
    avc_status status = avc_repository_init(path, AVC_DEFAULT_BRANCH,
                                            &repository, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: init failed: %s\n", error.message);
        return 1;
    }
    printf("Initialized empty AstraphosVC repository in %s\n",
           repository.metadata_path);
    avc_repository_free(&repository);
    return 0;
}

static int command_add(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "astraphosvc: usage: add <path>\n");
        return 1;
    }

    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    avc_odb odb;
    avc_odb_init(&odb);
    status = avc_odb_open(&odb, repo.objects_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        avc_repository_free(&repo);
        return 1;
    }

    char *index_path = avc_fs_join(repo.metadata_path, "index");
    if (index_path == NULL) {
        fprintf(stderr, "astraphosvc: out of memory\n");
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_index index;
    avc_index_init(&index);
    status = avc_index_load(&index, index_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to load index: %s\n",
                error.message);
        free(index_path);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        const char *file_path = argv[i];

        avc_file_stat statbuf;
        if (avc_fs_stat(file_path, &statbuf) != 0) {
            fprintf(stderr, "astraphosvc: cannot stat '%s': %s\n",
                    file_path, strerror(errno));
            continue;
        }
        if (!S_ISREG(statbuf.mode)) {
            fprintf(stderr, "astraphosvc: '%s' is not a regular file\n",
                    file_path);
            continue;
        }

        char *content = NULL;
        size_t content_size = 0;
        avc_error_clear(&error);
        status = avc_fs_read_file(file_path, &content, &content_size,
                                  &error);
        if (status != AVC_OK) {
            fprintf(stderr, "astraphosvc: failed to read '%s': %s\n",
                    file_path, error.message);
            continue;
        }

        avc_oid oid;
        avc_error_clear(&error);
        status = avc_odb_write_blob(&odb, content, content_size, oid,
                                    &error);
        free(content);
        if (status != AVC_OK) {
            fprintf(stderr, "astraphosvc: failed to store '%s': %s\n",
                    file_path, error.message);
            continue;
        }

        uint32_t mode = AVC_MODE_REGULAR;
        if (statbuf.mode & 0111) {
            mode = AVC_MODE_EXECUTABLE;
        }

        avc_error_clear(&error);
        avc_index_entry *existing = avc_index_find(&index, file_path);
        if (existing != NULL) {
            existing->dev = statbuf.dev;
            existing->ino = statbuf.ino;
            existing->uid = statbuf.uid;
            existing->gid = statbuf.gid;
            existing->size = statbuf.size;
            existing->ctime_sec = statbuf.ctime_sec;
            existing->mtime_sec = statbuf.mtime_sec;
            existing->mode = mode;
            memcpy(existing->oid, oid, 20);
            index.modified = 1;
        } else {
            status = avc_index_add(&index, file_path, mode, oid, &error);
            if (status == AVC_OK) {
                avc_index_entry *entry = avc_index_find(&index, file_path);
                if (entry != NULL) {
                    entry->dev = statbuf.dev;
                    entry->ino = statbuf.ino;
                    entry->uid = statbuf.uid;
                    entry->gid = statbuf.gid;
                    entry->size = statbuf.size;
                    entry->ctime_sec = statbuf.ctime_sec;
                    entry->mtime_sec = statbuf.mtime_sec;
                }
            }
        }
        if (status != AVC_OK) {
            fprintf(stderr, "astraphosvc: failed to index '%s': %s\n",
                    file_path, error.message);
            continue;
        }

        printf("add '%s'\n", file_path);
    }

    if (index.modified) {
        avc_error_clear(&error);
        status = avc_index_write(&index, index_path, &error);
        if (status != AVC_OK) {
            fprintf(stderr, "astraphosvc: failed to write index: %s\n",
                    error.message);
        }
    }

    avc_index_free(&index);
    free(index_path);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
    return 0;
}

static int command_status(int argc, char **argv) {
    (void)argc;
    (void)argv;

    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    char *index_path = avc_fs_join(repo.metadata_path, "index");
    if (index_path == NULL) {
        fprintf(stderr, "astraphosvc: out of memory\n");
        avc_repository_free(&repo);
        return 1;
    }

    avc_index index;
    avc_index_init(&index);
    status = avc_index_load(&index, index_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to load index: %s\n",
                error.message);
        free(index_path);
        avc_repository_free(&repo);
        return 1;
    }

    if (index.count == 0) {
        puts("nothing staged (use 'add' to stage files)");
    }

    for (size_t i = 0; i < index.count; i++) {
        const avc_index_entry *entry = &index.entries[i];

        avc_file_stat statbuf;
        int has_stat = (avc_fs_stat(entry->path, &statbuf) == 0);

        if (!has_stat) {
            printf("  deleted: %s\n", entry->path);
            continue;
        }

        if (entry->mtime_sec != statbuf.mtime_sec ||
            entry->size != statbuf.size) {

            char *content = NULL;
            size_t content_size = 0;
            avc_error_clear(&error);
            status = avc_fs_read_file(entry->path, &content,
                                      &content_size, &error);
            int modified = 1;
            if (status == AVC_OK) {
                avc_oid current_oid;
                avc_odb odb;
                avc_odb_init(&odb);
                avc_odb_open(&odb, repo.objects_path, &error);
                if (error.code == AVC_OK) {
                    avc_odb_write_blob(&odb, content, content_size,
                                       current_oid, &error);
                    if (error.code == AVC_OK &&
                        avc_oid_cmp(current_oid, entry->oid) == 0) {
                        modified = 0;
                    }
                }
                avc_odb_close(&odb);
                free(content);
            }

            if (modified) {
                printf("  modified: %s\n", entry->path);
            } else {
                printf("  unchanged: %s\n", entry->path);
            }
        } else {
            printf("  unchanged: %s\n", entry->path);
        }
    }

    avc_index_free(&index);
    free(index_path);
    avc_repository_free(&repo);
    return 0;
}

static avc_status get_signature(avc_signature *sig, const char *name_env,
                                const char *email_env, avc_error *error) {
    (void)error;
    const char *name = getenv(name_env);
    const char *email = getenv(email_env);
    if (name == NULL) name = getenv("GIT_AUTHOR_NAME");
    if (email == NULL) email = getenv("GIT_AUTHOR_EMAIL");
    if (name == NULL) name = "Astraphos User";
    if (email == NULL) email = "user@astraphosvc";

    snprintf(sig->name, sizeof(sig->name), "%s", name);
    snprintf(sig->email, sizeof(sig->email), "%s", email);
    sig->timestamp = time(NULL);

    struct tm lt;
    localtime_r(&sig->timestamp, &lt);
    char tz_str[16];
    strftime(tz_str, sizeof(tz_str), "%z", &lt);
    int h = 0, m = 0;
    sscanf(tz_str, "%3d%2d", &h, &m);
    sig->tz_offset = h * 60 + (h < 0 ? -m : m);

    return AVC_OK;
}

static int command_commit(int argc, char **argv) {
    const char *message = NULL;

    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-m") == 0) {
            message = argv[i + 1];
            break;
        }
    }
    if (message == NULL) {
        fprintf(stderr, "astraphosvc: usage: commit -m <message>\n");
        return 1;
    }

    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    avc_odb odb;
    avc_odb_init(&odb);
    status = avc_odb_open(&odb, repo.objects_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        avc_repository_free(&repo);
        return 1;
    }

    char *index_path = avc_fs_join(repo.metadata_path, "index");
    if (index_path == NULL) {
        fprintf(stderr, "astraphosvc: out of memory\n");
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_index index;
    avc_index_init(&index);
    status = avc_index_load(&index, index_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to load index: %s\n",
                error.message);
        free(index_path);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    if (index.count == 0) {
        fprintf(stderr, "astraphosvc: nothing to commit (add files first)\n");
        avc_index_free(&index);
        free(index_path);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_oid tree_oid;
    avc_error_clear(&error);
    status = avc_commit_build_tree(&odb, &index, tree_oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to build tree: %s\n",
                error.message);
        avc_index_free(&index);
        free(index_path);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_signature author, committer;
    avc_error_clear(&error);
    get_signature(&author, "ASTRAPHOSVC_AUTHOR_NAME",
                  "ASTRAPHOSVC_AUTHOR_EMAIL", &error);
    get_signature(&committer, "ASTRAPHOSVC_COMMITTER_NAME",
                  "ASTRAPHOSVC_COMMITTER_EMAIL", &error);

    avc_oid parent_oids[16];
    int parent_count = 0;

    avc_oid head_oid;
    avc_error_clear(&error);
    status = avc_refs_resolve_head(repo.metadata_path, head_oid, &error);
    if (status == AVC_OK) {
        memcpy(parent_oids[0], head_oid, 20);
        parent_count = 1;
    }

    avc_oid commit_oid;
    avc_error_clear(&error);
    status = avc_commit_create(&odb, tree_oid, (const avc_oid *)parent_oids, parent_count,
                               &author, &committer, message,
                               commit_oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to create commit: %s\n",
                error.message);
        avc_index_free(&index);
        free(index_path);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    char *ref_or_oid = NULL;
    int is_symbolic;
    avc_error_clear(&error);
    status = avc_refs_read_head(repo.metadata_path, &ref_or_oid,
                                &is_symbolic, &error);
    if (status == AVC_OK && is_symbolic) {
        avc_refs_write_ref(repo.metadata_path, ref_or_oid, commit_oid,
                           &error);
        free(ref_or_oid);
    } else {
        avc_refs_write_head_oid(repo.metadata_path, commit_oid, &error);
        free(ref_or_oid);
    }

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(commit_oid, hex);
    printf("[commit %s] %s\n", hex, message);

    avc_index_free(&index);
    free(index_path);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
    return 0;
}

static int command_log(int argc, char **argv) {
    (void)argc;
    (void)argv;

    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    avc_odb odb;
    avc_odb_init(&odb);
    status = avc_odb_open(&odb, repo.objects_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        avc_repository_free(&repo);
        return 1;
    }

    avc_oid head_oid;
    avc_error_clear(&error);
    status = avc_refs_resolve_head(repo.metadata_path, head_oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: no commits yet\n");
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_error_clear(&error);
    status = avc_commit_log(&odb, head_oid, 32, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: log error: %s\n", error.message);
    }

    avc_odb_close(&odb);
    avc_repository_free(&repo);
    return status == AVC_OK ? 0 : 1;
}

static int command_branch(int argc, char **argv) {
    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    if (argc == 2) {
        char **branches = NULL;
        int count = 0;
        avc_error_clear(&error);
        status = avc_refs_list_branches(repo.metadata_path, &branches,
                                        &count, &error);
        if (status != AVC_OK) {
            fprintf(stderr, "astraphosvc: %s\n", error.message);
            avc_repository_free(&repo);
            return 1;
        }

        char *current = NULL;
        avc_error_clear(&error);
        avc_refs_current_branch(repo.metadata_path, &current, &error);

        for (int i = 0; i < count; i++) {
            if (current != NULL && strcmp(branches[i], current) == 0) {
                printf("* %s\n", branches[i]);
            } else {
                printf("  %s\n", branches[i]);
            }
            free(branches[i]);
        }
        free(branches);
        free(current);
        avc_repository_free(&repo);
        return 0;
    }

    const char *branch_name = argv[2];
    char *refname = NULL;
    {
        size_t len = strlen("refs/heads/") + strlen(branch_name) + 1;
        refname = (char *)malloc(len);
        if (refname == NULL) {
            fprintf(stderr, "astraphosvc: out of memory\n");
            avc_repository_free(&repo);
            return 1;
        }
        snprintf(refname, len, "refs/heads/%s", branch_name);
    }

    avc_error_clear(&error);
    avc_oid head_oid;
    status = avc_refs_resolve_head(repo.metadata_path, head_oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: nothing to branch from (no commits)\n");
        free(refname);
        avc_repository_free(&repo);
        return 1;
    }

    avc_oid existing;
    avc_error_clear(&error);
    status = avc_refs_read_ref(repo.metadata_path, refname, existing, &error);
    if (status == AVC_OK) {
        fprintf(stderr, "astraphosvc: branch '%s' already exists\n",
                branch_name);
        free(refname);
        avc_repository_free(&repo);
        return 1;
    }

    avc_error_clear(&error);
    status = avc_refs_write_ref(repo.metadata_path, refname, head_oid, &error);
    free(refname);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to create branch: %s\n",
                error.message);
        avc_repository_free(&repo);
        return 1;
    }

    printf("Created branch '%s'\n", branch_name);
    avc_repository_free(&repo);
    return 0;
}

static int command_checkout(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "astraphosvc: usage: checkout <branch>\n");
        return 1;
    }

    const char *branch_name = argv[2];
    avc_error error;
    avc_error_clear(&error);

    size_t rlen = strlen("refs/heads/") + strlen(branch_name) + 1;
    char *refname = (char *)malloc(rlen);
    if (refname == NULL) {
        fprintf(stderr, "astraphosvc: out of memory\n");
        return 1;
    }
    snprintf(refname, rlen, "refs/heads/%s", branch_name);

    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        free(refname);
        return 1;
    }

    avc_oid oid;
    avc_error_clear(&error);
    status = avc_refs_read_ref(repo.metadata_path, refname, oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: branch '%s' does not exist\n",
                branch_name);
        free(refname);
        avc_repository_free(&repo);
        return 1;
    }

    avc_error_clear(&error);
    status = avc_refs_write_head_ref(repo.metadata_path, refname, &error);
    free(refname);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: failed to switch branch: %s\n",
                error.message);
        avc_repository_free(&repo);
        return 1;
    }

    printf("Switched to branch '%s'\n", branch_name);
    avc_repository_free(&repo);
    return 0;
}

static int command_merge(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "astraphosvc: usage: merge <branch>\n");
        return 1;
    }

    const char *branch_name = argv[2];
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo = {0};
    avc_status status = avc_repository_discover(NULL, &repo, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        return 1;
    }

    avc_odb odb;
    avc_odb_init(&odb);
    status = avc_odb_open(&odb, repo.objects_path, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: %s\n", error.message);
        avc_repository_free(&repo);
        return 1;
    }

    size_t rlen = strlen("refs/heads/") + strlen(branch_name) + 1;
    char *refname = (char *)malloc(rlen);
    if (refname == NULL) {
        fprintf(stderr, "astraphosvc: out of memory\n");
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }
    snprintf(refname, rlen, "refs/heads/%s", branch_name);

    avc_oid branch_oid;
    avc_error_clear(&error);
    status = avc_refs_read_ref(repo.metadata_path, refname, branch_oid,
                               &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: branch '%s' does not exist\n",
                branch_name);
        free(refname);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }
    free(refname);

    avc_oid head_oid;
    avc_error_clear(&error);
    status = avc_refs_resolve_head(repo.metadata_path, head_oid, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: nothing to merge onto (no commits)\n");
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    avc_oid result_oid;
    int was_ff = 0;
    avc_error_clear(&error);
    status = avc_merge(&odb, repo.metadata_path, head_oid, branch_oid,
                        result_oid, &was_ff, &error);
    if (status == AVC_ERR_CONFLICT) {
        fprintf(stderr, "astraphosvc: merge conflict in '%s'\n",
                error.message);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: merge failed: %s\n", error.message);
        avc_odb_close(&odb);
        avc_repository_free(&repo);
        return 1;
    }

    char *current_branch = NULL;
    avc_error_clear(&error);
    avc_refs_current_branch(repo.metadata_path, &current_branch, &error);

    if (current_branch != NULL) {
        rlen = strlen("refs/heads/") + strlen(current_branch) + 1;
        refname = (char *)malloc(rlen);
        if (refname != NULL) {
            snprintf(refname, rlen, "refs/heads/%s", current_branch);
            avc_refs_write_ref(repo.metadata_path, refname, result_oid,
                               &error);
            free(refname);
        }
        free(current_branch);
    }

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(result_oid, hex);

    if (was_ff) {
        printf("Merge: fast-forward (%s)\n", hex);
    } else {
        printf("Merge: made merge commit (%s)\n", hex);
    }

    avc_odb_close(&odb);
    avc_repository_free(&repo);
    return 0;
}

int avc_cli_main(int argc, char **argv) {
    avc_log_from_environment();
    if (argc < 2 || strcmp(argv[1], "help") == 0 ||
        strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[1], "version") == 0 ||
        strcmp(argv[1], "--version") == 0) {
        puts("AstraphosVC " ASTRAPHOSVC_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "init") == 0) {
        return command_init(argc, argv);
    }
    if (strcmp(argv[1], "add") == 0) {
        return command_add(argc, argv);
    }
    if (strcmp(argv[1], "status") == 0) {
        return command_status(argc, argv);
    }
    if (strcmp(argv[1], "commit") == 0) {
        return command_commit(argc, argv);
    }
    if (strcmp(argv[1], "log") == 0) {
        return command_log(argc, argv);
    }
    if (strcmp(argv[1], "branch") == 0) {
        return command_branch(argc, argv);
    }
    if (strcmp(argv[1], "checkout") == 0) {
        return command_checkout(argc, argv);
    }
    if (strcmp(argv[1], "merge") == 0) {
        return command_merge(argc, argv);
    }

    fprintf(stderr, "astraphosvc: command '%s' is not yet implemented\n",
            argv[1]);
    return 2;
}
