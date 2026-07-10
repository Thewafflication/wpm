// wpm.cpp : Defines the entry point for the application.
//

#include "wpm.h"
#include "archive.h"
#include "helpers.h"
#include "init.h"

int main(int argc, char *argv[])
{
	if (argc == 1) {
		print_version();
        print_usage(0);
        return 0;
	}

    if (strcmp(argv[1], "--version") == 0) {
        if (argc > 2 && strcmp(argv[2], "--verbose") == 0) {
            print_verbose_version();
        }
        else {
            print_version();
        }
        return 0;
    }

    if (strcmp(argv[1], "--verbose") == 0) {
        print_verbose_version();
        return 0;
    }
	
    Command cmd = parse_command(argv[1]);
    switch (cmd) {
        case CMD_BUILD: {
            if (argc < 3) {
                printf("Usage: wpm build <source_dir> [output_dir] [--no-index]\n");
                return 1;
            }

            const char* source = argv[2];
            const char* output = NULL;
            int no_index = 0;

            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "--no-index") == 0) {
                    no_index = 1;
                }
                else {
                    output = argv[i];
                }
            }

            if (!wpm_archive_build(source, output ? output : ".")) return 1;
            if (no_index) printf("Skipped package index update.\n");
            break;
        }

        case CMD_INSTALL: {
            if (argc < 3) {
                printf("No packages specified.\n");
                return 1;
            }

            for (int i = 2; i < argc; i++) {
                if (!wpm_archive_install(argv[i])) return 1;
            }
            break;
        }

        case CMD_REMOVE:
        case CMD_UPGRADE: {
            if (argc < 3) {
                printf("No packages specified.\n");
                return 1;
            }

            printf("Packages:\n");
            for (int i = 2; i < argc; i++) {
                printf("- %s\n", argv[i]);
            }
            break;
        }
        case CMD_INIT: {
            char name[128] = { 0 };
            printf("Initializing WPM package...\n\n");

            if (argc > 3) {
                printf("Usage: wpm init [package_name]\n");
                return 1;
            }

            if (argc == 3) {
                if (strlen(argv[2]) >= sizeof(name)) {
                    printf("Invalid package name. Maximum length is %zu characters.\n",
                        sizeof(name) - 1);
                    return 1;
                }
                strcpy_s(name, sizeof(name), argv[2]);
            }
            else {
                if (!wpm_current_directory_name(name, sizeof(name))) return 1;
            }

            if (!wpm_package_name_is_valid(name)) {
                printf("Invalid package name. Use letters, numbers, '.', '-', or '_'.\n");
                return 1;
            }

            printf("\n");

            if (!wpm_init_run(name)) return 1;

            printf("\nDone.\n");
            break;
        }

        case CMD_UPDATE:
            printf("Updating...\n");
            break;

        default:
            printf("Unknown command.\n");
            return 1;
    }
    return 0;
}

void print_version() 
{
    printf("=================================================================\n");
	printf("Waughtal Package Manager (wpm) Version %s \n", WPM_VERSION);
    printf("=================================================================\n");
}

void print_verbose_version()
{
    print_version();
    printf("Dependencies:\n");
    printf("  miniz %s (commit %s%s)\n",
        WPM_MINIZ_VERSION,
        WPM_MINIZ_COMMIT,
        WPM_MINIZ_DIRTY ? ", dirty" : "");
    printf("  libsodium %s (commit %s%s)\n",
        WPM_SODIUM_VERSION,
        WPM_SODIUM_COMMIT,
        WPM_SODIUM_DIRTY ? ", dirty" : "");
}

void print_usage(Command c) {
    printf("Usage:\n");
    printf("  wpm <command> [options]\n\n");

    printf("Commands:\n");
    printf("  build <source_dir> [output_dir] [--no-index]\n");
    printf("      Build a package from source\n\n");

    printf("  install <package...>\n");
    printf("      Install one or more packages\n\n");

    printf("  init [package_name]\n");
    printf("      Initialize new package\n\n");

    printf("  remove <package...>\n");
    printf("      Remove one or more packages\n\n");

    printf("  update\n");
    printf("      Update package index\n\n");

    printf("  upgrade <package...>\n");
    printf("      Upgrade one or more packages\n\n");

    printf("Options:\n");
    printf("  --version [--verbose]\n");
    printf("      Display WPM version and optional dependency details\n\n");

    printf("  --verbose\n");
    printf("      Display detailed WPM and dependency version information\n\n");

    printf("  --no-index\n");
    printf("      Skip updating index during build\n\n");

    printf("Examples:\n");
    printf("  wpm build ./my_project\n");
    printf("  wpm build ./my_project ./dist --no-index\n");
    printf("  wpm init my-package\n");
    printf("  wpm install pkg1 pkg2\n");
    printf("  wpm update\n\n");
}

Command parse_command(const char* cmd) {
    if (strcmp(cmd, "init") == 0) return CMD_INIT;
    if (strcmp(cmd, "build") == 0) return CMD_BUILD;
    if (strcmp(cmd, "install") == 0) return CMD_INSTALL;
    if (strcmp(cmd, "remove") == 0) return CMD_REMOVE;
    if (strcmp(cmd, "update") == 0) return CMD_UPDATE;
    if (strcmp(cmd, "upgrade") == 0) return CMD_UPGRADE;
    return CMD_UNKNOWN;
}
