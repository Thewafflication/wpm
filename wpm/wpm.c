// wpm.cpp : Defines the entry point for the application.
//

#include "wpm.h"
#include "archive.h"
#include "helpers.h"
#include "init.h"
#include "repository.h"
#include "signing.h"
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
    char program_files[MAX_PATH];
    DWORD path_length;

    if (!wpm_get_environment_variable("ProgramW6432", program_files, sizeof(program_files)) &&
        !wpm_get_environment_variable("ProgramFiles", program_files, sizeof(program_files))) {
        program_files[0] = '\0';
    }
    path_length = GetModuleFileNameA(NULL, executable_path, sizeof(executable_path));

    if (program_files[0] && path_length > 0 && path_length < sizeof(executable_path) &&
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

static void print_windows_dependency_version(const char* display_name, const char* module_name)
{
    HMODULE module = GetModuleHandleA(module_name);
    char path[MAX_PATH];
    DWORD handle = 0;
    DWORD size;
    void* version_data;
    VS_FIXEDFILEINFO* fixed_info = NULL;
    UINT fixed_info_size = 0;

    if (!module || GetModuleFileNameA(module, path, sizeof(path)) == 0 ||
        (size = GetFileVersionInfoSizeA(path, &handle)) == 0 ||
        (version_data = malloc(size)) == NULL) {
        printf("  %s unknown (Windows system library)\n", display_name);
        return;
    }
    if (!GetFileVersionInfoA(path, 0, size, version_data) ||
        !VerQueryValueA(version_data, "\\", (LPVOID*)&fixed_info, &fixed_info_size) ||
        fixed_info_size < sizeof(*fixed_info)) {
        printf("  %s unknown (Windows system library)\n", display_name);
        free(version_data);
        return;
    }
    printf("  %s %u.%u.%u.%u (Windows system library)\n", display_name,
        HIWORD(fixed_info->dwFileVersionMS), LOWORD(fixed_info->dwFileVersionMS),
        HIWORD(fixed_info->dwFileVersionLS), LOWORD(fixed_info->dwFileVersionLS));
    free(version_data);
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
    if ((argc == 8 || argc == 9) && strcmp(argv[1], "--complete-self-upgrade") == 0) {
        DWORD parent_id = (DWORD)strtoul(argv[3], NULL, 10);
        int completed;
        const char* self_upgrade_log_path = argc == 9 ? argv[8] : NULL;
        FILE* self_upgrade_log = NULL;
        if (self_upgrade_log_path) {
            if (fopen_s(&self_upgrade_log, self_upgrade_log_path, "a") != 0) return 1;
            fprintf(self_upgrade_log, "Completing WPM self-upgrade: %s %s -> %s\n", argv[5], argv[6], argv[4]);
            fclose(self_upgrade_log);
            SetEnvironmentVariableA("WPM_SELF_UPGRADE_LOG", self_upgrade_log_path);
        }
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parent_id);
        if (parent) {
            WaitForSingleObject(parent, INFINITE);
            CloseHandle(parent);
        }
        if (!wpm_initialize_data_directories()) return 1;
        completed = wpm_archive_upgrade(argv[2], atoi(argv[7]) != 0,
            "wpm", argv[4], argv[5], argv[6]);
        if (self_upgrade_log_path && fopen_s(&self_upgrade_log, self_upgrade_log_path, "a") == 0) {
            fprintf(self_upgrade_log, "Result: wpm %s %s\n", argv[5], completed ? "upgraded" : "failed");
            fclose(self_upgrade_log);
        }
        return completed ? 0 : 1;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "--offline") == 0) {
            offline = 1;
        }
        else if (strcmp(argv[i], "--version") == 0 && command_index == -1) {
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
            const char* signing_key = NULL;
            char default_key[4096];

            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--verbose") == 0) continue;
                if (strcmp(argv[i], "--no-index") == 0) {
                    no_index = 1;
                }
                else if (strcmp(argv[i], "--sign") == 0 && i + 1 < argc) {
                    signing_key = argv[++i];
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
            if (!signing_key) {
                int default_result = wpm_get_default_key(default_key, sizeof(default_key));
                if (default_result < 0) return 1;
                if (default_result > 0) signing_key = default_key;
            }
            if (!wpm_archive_build(source_dir, output_dir ? output_dir : ".", !no_index, signing_key)) return 1;
            if (no_index) printf("Skipped package index update.\n");
            break;
        }

        case CMD_VERIFY: {
            int package_count = 0;
            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--verbose") == 0) continue;
                package_count++;
                if (!wpm_archive_verify(argv[i])) return 1;
            }
            if (package_count == 0) {
                printf("No packages specified.\n");
                return 1;
            }
            break;
        }

        case CMD_INSTALL: {
            int package_count = 0;
            int allow_unsigned = 0;
            const char* selected_arch = NULL;
            const char* selected_version = NULL;
            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--allow-unsigned") == 0) { allow_unsigned = 1; continue; }
                if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc) { selected_arch = argv[++i]; continue; }
                if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) { selected_version = argv[++i]; continue; }
                if (strcmp(argv[i], "--verbose") != 0 && strcmp(argv[i], "--offline") != 0) package_count++;
            }
            if (package_count == 0) {
                printf("No packages specified.\n");
                return 1;
            }

            for (int i = command_index + 1; i < argc; i++) {
                const char* extension;
                if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "--offline") == 0 || strcmp(argv[i], "--allow-unsigned") == 0) continue;
                if (strcmp(argv[i], "--arch") == 0 || strcmp(argv[i], "--version") == 0) { i++; continue; }
                extension = strrchr(argv[i], '.');
                if (extension && _stricmp(extension, ".zip") == 0) {
                    if (selected_arch || selected_version) { printf("Error: --arch and --version are not valid for ZIP-path installation.\n"); return 1; }
                    if (!wpm_archive_install(argv[i], allow_unsigned)) return 1;
                } else if (!wpm_repo_install(argv[i], selected_arch, selected_version, offline, allow_unsigned)) return 1;
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

        case CMD_KEY: {
            const char* action = command_index + 1 < argc ? argv[command_index + 1] : "";
            if (strcmp(action, "default") == 0 && command_index + 2 < argc && strcmp(argv[command_index + 2], "--clear") == 0) {
                if (!wpm_clear_default_key()) return 1;
                printf("Default signing key cleared.\n");
            }
            else if (strcmp(action, "default") == 0 && command_index + 2 < argc) {
                if (!wpm_set_default_key(argv[command_index + 2])) return 1;
            }
            else { printf("Usage: wpm key default <private-key-file>|--clear\n"); return 1; }
            break;
        }

        case CMD_TRUST: {
            const char* action = command_index + 1 < argc ? argv[command_index + 1] : "list";
            if (strcmp(action, "add") == 0 && command_index + 2 < argc) { if (!wpm_trust_add(argv[command_index + 2])) return 1; }
            else if (strcmp(action, "list") == 0) { if (!wpm_trust_list()) return 1; }
            else if (strcmp(action, "revoke") == 0 && command_index + 2 < argc) { if (!wpm_trust_revoke(argv[command_index + 2])) return 1; }
            else { printf("Usage: wpm trust <add|list|revoke> ...\n"); return 1; }
            break;
        }

        case CMD_CONFIG: {
            const char* action = command_index + 1 < argc ? argv[command_index + 1] : "";
            const char* setting = command_index + 2 < argc ? argv[command_index + 2] : "";
            const char* package = NULL;
            for (int i = command_index + 3; i < argc; i++) {
                if (strcmp(argv[i], "--package") == 0 && i + 1 < argc) package = argv[++i];
            }
            if (strcmp(setting, "prerelease") != 0) { printf("Usage: wpm config <set|get|unset> prerelease ...\n"); return 1; }
            if (strcmp(action, "get") == 0) {
                if (!wpm_config_prerelease_get(package)) return 1;
            }
            else if (strcmp(action, "set") == 0 && command_index + 3 < argc) {
                const char* value = argv[command_index + 3];
                if (_stricmp(value, "true") && _stricmp(value, "false")) { printf("Error: prerelease value must be true or false.\n"); return 1; }
                if (!wpm_config_prerelease_set(package, _stricmp(value, "true") == 0)) return 1;
            }
            else if (strcmp(action, "unset") == 0 && package) {
                if (!wpm_config_prerelease_unset(package)) return 1;
            }
            else { printf("Usage: wpm config <set|get|unset> prerelease ...\n"); return 1; }
            break;
        }

        case CMD_UNKNOWN:
            if (strcmp(argv[command_index], "keygen") == 0) {
                int make_default = argc == command_index + 4 && strcmp(argv[command_index + 3], "--default") == 0;
                if ((argc != command_index + 3 && !make_default) || !wpm_keygen(argv[command_index + 1], argv[command_index + 2], make_default)) return 1;
                break;
            }
            printf("Unknown command.\n");
            return 1;

        case CMD_UPGRADE: {
            const char* names[128];
            int name_count = 0, all = 0, allow_unsigned = 0, assume_yes = 0;
            const char* selected_arch = NULL;
            const char* selected_version = NULL;
            for (int i = command_index + 1; i < argc; i++) {
                if (strcmp(argv[i], "--all") == 0) all = 1;
                else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--yes") == 0) assume_yes = 1;
                else if (strcmp(argv[i], "--allow-unsigned") == 0) allow_unsigned = 1;
                else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc) selected_arch = argv[++i];
                else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) selected_version = argv[++i];
                else if (strcmp(argv[i], "--offline") && strcmp(argv[i], "--verbose") && name_count < 128) names[name_count++] = argv[i];
            }
            if ((!all && name_count == 0) || (all && name_count > 0) || (all && selected_version) || (assume_yes && !all)) {
                printf("Usage: wpm upgrade <package...> [--arch <arch>] [--version <semver>] | --all [--arch <arch>]\n");
                return 1;
            }
            if (!wpm_repo_upgrade(names, name_count, all, selected_arch, selected_version, offline, allow_unsigned, assume_yes)) return 1;
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

        default: return 1;
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
    print_windows_dependency_version("urlmon", "urlmon.dll");
    print_windows_dependency_version("advapi32", "advapi32.dll");
}

void print_usage(Command c) {
    printf("Usage:\n");
    printf("  wpm <command> [options]\n\n");

    printf("Commands:\n");
    printf("  build <source_dir> [output_dir] [--no-index]\n");
    printf("      Build a package from source\n\n");

    printf("  verify <package...>\n");
    printf("      Validate signed packages without installing them\n\n");

    printf("  install <package...>\n");
    printf("      Install one or more packages\n\n");

    printf("  init [package_name]\n");
    printf("      Initialize new package\n\n");

    printf("  remove <package...>\n");
    printf("      Remove one or more packages\n\n");

    printf("  repo <add|list|remove|update> ...\n");
    printf("      Configure HTTPS package repositories\n\n");

    printf("  keygen <private-key-file> <public-key-file> [--default]\n");
    printf("      Generate an Ed25519 signing key pair\n\n");
    printf("  key default <private-key-file>|--clear\n");
    printf("      Configure the default signing key\n\n");
    printf("  trust <add|list|revoke> ...\n");
    printf("      Manage trusted package signing keys\n\n");

    printf("  config <set|get|unset> prerelease ...\n");
    printf("      Configure stable or prerelease package selection\n\n");

    printf("  update\n");
    printf("      Update package index\n\n");

    printf("  upgrade <package...> | --all [-y]\n");
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

    printf("  --sign <private-key-file>\n");
    printf("      Sign a package during build\n\n");
    printf("  --allow-unsigned\n");
    printf("      Allow installation of an unsigned package\n\n");
    printf("  -y, --yes\n");
    printf("      Confirm a planned --all upgrade without prompting\n\n");

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
    if (strcmp(cmd, "verify") == 0) return CMD_VERIFY;
    if (strcmp(cmd, "install") == 0) return CMD_INSTALL;
    if (strcmp(cmd, "remove") == 0) return CMD_REMOVE;
    if (strcmp(cmd, "repo") == 0) return CMD_REPO;
    if (strcmp(cmd, "key") == 0) return CMD_KEY;
    if (strcmp(cmd, "trust") == 0) return CMD_TRUST;
    if (strcmp(cmd, "config") == 0) return CMD_CONFIG;
    if (strcmp(cmd, "update") == 0) return CMD_UPDATE;
    if (strcmp(cmd, "upgrade") == 0) return CMD_UPGRADE;
    return CMD_UNKNOWN;
}
