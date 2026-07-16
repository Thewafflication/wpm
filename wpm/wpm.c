// wpm.cpp : Defines the entry point for the application.
//

#include "wpm.h"
#include "archive.h"
#include "helpers.h"
#include "init.h"
#include "repository.h"
#include <windows.h>

static int path_is_beneath(const char* path, const char* root)
{
    size_t root_length = strlen(root);

    while (root_length > 0 && (root[root_length - 1] == '\\' || root[root_length - 1] == '/')) {
        root_length--;
    }

    return _strnicmp(path, root, root_length) == 0 &&
        (path[root_length] == '\0' || path[root_length] == '\\' || path[root_length] == '/');
}

static void print_runtime_mode(void)
{
    char executable_path[MAX_PATH];
    char managed_root[MAX_PATH];
    const char* program_files = getenv("ProgramW6432");
    DWORD path_length;

    if (!program_files || !program_files[0]) program_files = getenv("ProgramFiles");
    path_length = GetModuleFileNameA(NULL, executable_path, sizeof(executable_path));

    if (program_files && program_files[0] && path_length > 0 && path_length < sizeof(executable_path) &&
        snprintf(managed_root, sizeof(managed_root), "%s\\WPM", program_files) > 0 &&
        path_is_beneath(executable_path, managed_root)) {
        printf("Runtime mode: managed\n");
    }
    else {
        printf("Runtime mode: portable\n");
    }
    if (path_length > 0 && path_length < sizeof(executable_path)) {
        printf("Executable: %s\n", executable_path);
    }
}

static void print_diagnostics(void)
{
    char data_root[MAX_PATH];
    print_runtime_mode();
    if (!wpm_get_data_root(data_root, sizeof(data_root))) {
        printf("Data directory: unknown\n");
        return;
    }
    printf("Data directory: %s\n", data_root);
    printf("Package directory: %s\\packages\n", data_root);
    printf("Cache directory: %s\\cache\n", data_root);
    printf("Configuration directory: %s\\config\n", data_root);
}

int main(int argc, char *argv[])
{
	int verbose = 0;
	int offline = 0;
    int command_index = -1;
	int show_version = 0;
	int show_diagnostics = 0;

	if (argc == 1) {
		print_version();
        print_usage(0);
        return 0;
	}
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "--offline") == 0) {
            offline = 1;
        }
        else if (strcmp(argv[i], "--version") == 0) {
            show_version = 1;
        }
        else if (strcmp(argv[i], "--diagnose") == 0) {
            show_diagnostics = 1;
        }
        else if (command_index == -1) {
            command_index = i;
        }
    }

    if (show_version) {
        print_version();
        if (verbose) print_runtime_mode();
        return 0;
    }

    if (show_diagnostics) {
        print_diagnostics();
        return 0;
    }

    if (command_index == -1) {
        print_version();
        if (verbose) print_runtime_mode();
        print_usage(0);
        return 0;
    }

	Command cmd = parse_command(argv[command_index]);
    wpm_set_verbose(verbose);
	if (verbose) print_runtime_mode();
	if (!wpm_initialize_data_directories()) return 1;
	
    switch (cmd) {
        case CMD_BUILD: {
            const char* source_dir = NULL;
            const char* output_dir = NULL;
            int no_index = 0;

            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--verbose") == 0) continue;
                if (strcmp(argv[i], "--no-index") == 0) {
                    no_index = 1;
                }
                else if (!source_dir) {
                    source_dir = argv[i];
                }
                else if (!output_dir) {
                    output_dir = argv[i];
                }
                else {
                    printf("Usage: wpm build <source_dir> [output_dir] [--no-index] [--verbose]\n");
                    return 1;
                }
            }
            if (!source_dir) {
                printf("Usage: wpm build <source_dir> [output_dir] [--no-index]\n");
                return 1;
            }
            if (!wpm_archive_build(source_dir, output_dir ? output_dir : ".", !no_index)) return 1;
            if (no_index) printf("Skipped package index update.\n");
            break;
        }

        case CMD_INSTALL: {
            int package_count = 0;
            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--verbose") != 0 && strcmp(argv[i], "--offline") != 0) package_count++;
            }
            if (package_count == 0) {
                printf("No packages specified.\n");
                return 1;
            }

            for (int i = command_index + 1; i < argc; i++) {
                const char* extension;
                if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "--offline") == 0) continue;
                extension = strrchr(argv[i], '.');
                if (extension && _stricmp(extension, ".zip") == 0) {
                    if (!wpm_archive_install(argv[i])) return 1;
                } else if (!wpm_repo_install(argv[i], offline)) return 1;
            }
            break;
        }

        case CMD_REMOVE: {
            if (argc < 3) {
                printf("No packages specified.\n");
                return 1;
            }

            for (int i = 2; i < argc; i++) {
                if (!wpm_archive_remove(argv[i])) return 1;
            }
            break;
        }

        case CMD_REPO: {
            const char* action = command_index + 1 < argc ? argv[command_index + 1] : "list";
            if (strcmp(action, "list") == 0 && (command_index + 2 == argc || (command_index + 3 == argc && strcmp(argv[command_index + 2], "--offline") == 0))) {
                if (!wpm_repo_list()) return 1;
            } else if (strcmp(action, "add") == 0) {
                int priority = 0;
                if (command_index + 2 >= argc) { printf("Usage: wpm repo add <https-url> [--priority <integer>]\n"); return 1; }
                if (command_index + 3 < argc && (command_index + 5 != argc || strcmp(argv[command_index + 3], "--priority") != 0)) { printf("Usage: wpm repo add <https-url> [--priority <integer>]\n"); return 1; }
                if (command_index + 3 < argc) priority = atoi(argv[command_index + 4]);
                if (!wpm_repo_add(argv[command_index + 2], priority)) return 1;
            } else if (strcmp(action, "remove") == 0 && command_index + 3 == argc) {
                if (!wpm_repo_remove(argv[command_index + 2])) return 1;
            } else if (strcmp(action, "update") == 0 && (command_index + 2 == argc || (command_index + 3 == argc && strcmp(argv[command_index + 2], "--offline") == 0))) {
                if (!wpm_repo_update(offline)) return 1;
            } else { printf("Usage: wpm repo <add|list|remove|update> ...\n"); return 1; }
            break;
        }

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
            if (!wpm_repo_update(offline)) return 1;
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

    printf("  repo <add|list|remove|update> ...\n");
    printf("      Configure HTTPS package repositories\n\n");

    printf("  update\n");
    printf("      Update package index\n\n");

    printf("  upgrade <package...>\n");
    printf("      Upgrade one or more packages\n\n");

    printf("Options:\n");
    printf("  --version\n");
    printf("      Display WPM and dependency version information\n\n");

    printf("  --verbose\n");
    printf("      Display detailed file-operation progress; may appear before or after a command\n\n");

    printf("  --diagnose\n");
    printf("      Display runtime mode and resolved WPM locations\n\n");

    printf("  --no-index\n");
    printf("      Skip updating index during build\n\n");

    printf("  --offline\n");
    printf("      Use cached repository data only\n\n");

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
    if (strcmp(cmd, "repo") == 0) return CMD_REPO;
    if (strcmp(cmd, "update") == 0) return CMD_UPDATE;
    if (strcmp(cmd, "upgrade") == 0) return CMD_UPGRADE;
    return CMD_UNKNOWN;
}
