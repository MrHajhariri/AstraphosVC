#include "cli/avc_cli.h"

#include "api/astraphosvc.h"

#include <stdio.h>
#include <string.h>

static void print_help(void) {
    puts("AstraphosVC - modern distributed version control");
    puts("");
    puts("Usage:");
    puts("  astraphosvc <command> [options]");
    puts("");
    puts("Implemented commands:");
    puts("  init [path]             Initialize an AstraphosVC repository");
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
    avc_status status = avc_repository_init(path, AVC_DEFAULT_BRANCH, &repository, &error);
    if (status != AVC_OK) {
        fprintf(stderr, "astraphosvc: init failed: %s\n", error.message);
        return 1;
    }
    printf("Initialized empty AstraphosVC repository in %s\n", repository.metadata_path);
    avc_repository_free(&repository);
    return 0;
}

int avc_cli_main(int argc, char **argv) {
    avc_log_from_environment();
    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0) {
        puts("AstraphosVC " ASTRAPHOSVC_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "init") == 0) {
        return command_init(argc, argv);
    }

    fprintf(stderr, "astraphosvc: command '%s' is not yet implemented\n", argv[1]);
    return 2;
}
