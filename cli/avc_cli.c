#include "cli/avc_cli.h"

#include "api/astraphosvc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

    fprintf(stderr, "astraphosvc: command '%s' is not yet implemented\n",
            argv[1]);
    return 2;
}
