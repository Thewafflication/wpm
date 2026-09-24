// wpm.cpp : Defines the entry point for the application.
//

/** @file wpm.c @brief WPM command-line parsing and process entry point. */
#include "wpm.h"
#include "archive.h"
#include "helpers.h"
#include "init.h"
#include "logging.h"
#include "repository.h"
#include "signing.h"
#include <stdlib.h>
#include <windows.h>
#ifdef WPM_HAS_WCRT
#include <wcrt/cpu.h>
#endif

#ifdef WPM_HAS_WCRT
static void print_cpu_count(const char* label, unsigned long count)
{
    if (count) printf("  %s: %lu\n", label, count);
    else printf("  %s: unknown\n", label);
}
#endif

static void print_cpu_info(void)
{
#ifdef WPM_HAS_WCRT
    static const struct {
        unsigned long flag;
        const char* name;
    } features[] = {
        { WCRT_CPU_MMX, "MMX" },
        { WCRT_CPU_SSE, "SSE" },
        { WCRT_CPU_SSE2, "SSE2" },
        { WCRT_CPU_SSE3, "SSE3" },
        { WCRT_CPU_SSSE3, "SSSE3" },
        { WCRT_CPU_SSE41, "SSE4.1" },
        { WCRT_CPU_SSE42, "SSE4.2" },
        { WCRT_CPU_AES, "AES" },
        { WCRT_CPU_AVX, "AVX" },
        { WCRT_CPU_AVX2, "AVX2" },
        { WCRT_CPU_FMA, "FMA" },
        { WCRT_CPU_AVX512F, "AVX512F" },
        { WCRT_CPU_BMI1, "BMI1" },
        { WCRT_CPU_BMI2, "BMI2" },
        { WCRT_CPU_NEON, "NEON" },
        { WCRT_CPU_ARM_CRYPTO, "ARM-CRYPTO" },
        { WCRT_CPU_ARM_CRC32, "ARM-CRC32" },
        { WCRT_CPU_ARM_ATOMICS, "ARM-ATOMICS" }
    };
    const struct wcrt_cpu_info* cpu = wcrt_cpu_get_info();
    const char* architecture = "unknown";
    size_t i;
    int printed = 0;

    if (cpu) {
        switch (cpu->architecture) {
            case WCRT_CPU_ARCH_X86: architecture = "x86"; break;
            case WCRT_CPU_ARCH_X64: architecture = "x64"; break;
            case WCRT_CPU_ARCH_ARM64: architecture = "arm64"; break;
        }
    }
    printf("CPU:\n");
    printf("  Process architecture: %s\n", architecture);
    printf("  Vendor: %s\n", cpu && cpu->vendor[0] ? cpu->vendor : "unknown");
    printf("  Brand: %s\n", cpu && cpu->brand[0] ? cpu->brand : "unknown");
    print_cpu_count("Logical processors (system)", wcrt_cpu_logical_count());
    print_cpu_count("Physical cores (system)", wcrt_cpu_core_count());
    print_cpu_count("Available processors (process affinity)", wcrt_cpu_available_count());
    printf("  Usable instruction sets:");
    if (cpu) {
        for (i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
            if (cpu->features & features[i].flag) {
                printf(" %s", features[i].name);
                printed = 1;
            }
        }
    }
    if (!printed) printf(" %s", cpu ? "none detected" : "unknown");
    printf("\n");
#else
    printf("CPU: detection unavailable\n");
#endif
}

static const char* command_name(Command command)
{
    switch (command) {
        case CMD_INIT: return "init";
        case CMD_BUILD: return "build";
        case CMD_VERIFY: return "verify";
        case CMD_INSTALL: return "install";
        case CMD_REMOVE: return "remove";
        case CMD_REPO: return "repo";
        case CMD_KEYGEN: return "keygen";
        case CMD_KEY: return "key";
        case CMD_TRUST: return "trust";
        case CMD_CONFIG: return "config";
        case CMD_UPDATE: return "update";
        case CMD_UPGRADE: return "upgrade";
        default: return "command";
    }
}

static void print_usage_line(Command command)
{
    switch (command) {
        case CMD_INIT: printf("Usage: wpm init [package_name]\n"); break;
        case CMD_BUILD: printf("Usage: wpm build <source_dir> [output_dir] [--no-index] [--sign <private-key-file>]\n"); break;
        case CMD_VERIFY: printf("Usage: wpm verify <package...>\n"); break;
        case CMD_INSTALL: printf("Usage: wpm install <package...> [--arch <arch>] [--version <semver>] [--offline] [--allow-unsigned]\n"); break;
        case CMD_REMOVE: printf("Usage: wpm remove <package...>\n"); break;
        case CMD_REPO: printf("Usage: wpm repo <add|list|remove|update> ...\n"); break;
        case CMD_KEYGEN: printf("Usage: wpm keygen <private-key-file> <public-key-file> [--default]\n"); break;
        case CMD_KEY: printf("Usage: wpm key default <private-key-file>|--clear\n"); break;
        case CMD_TRUST: printf("Usage: wpm trust <add|list|revoke> ...\n"); break;
        case CMD_CONFIG: printf("Usage: wpm config <set|get|unset> prerelease ...\n"); break;
        case CMD_UPDATE: printf("Usage: wpm update [--offline]\n"); break;
        case CMD_UPGRADE: printf("Usage: wpm upgrade <package...> [--arch <arch>] [--version <semver>] | --all [--arch <arch>] [-y|--yes]\n"); break;
        default: printf("Usage: wpm <command> [options]\n"); break;
    }
}

static int command_option_takes_value(Command command, const char* option)
{
    return (command == CMD_BUILD && strcmp(option, "--sign") == 0) ||
        ((command == CMD_INSTALL || command == CMD_UPGRADE) &&
            (strcmp(option, "--arch") == 0 || strcmp(option, "--version") == 0)) ||
        (command == CMD_REPO && strcmp(option, "--priority") == 0) ||
        (command == CMD_CONFIG && strcmp(option, "--package") == 0);
}

static int command_option_is_known(Command command, const char* option)
{
    if (strcmp(option, "--verbose") == 0) return 1;
    if (command_option_takes_value(command, option)) return 1;
    switch (command) {
        case CMD_BUILD: return strcmp(option, "--no-index") == 0;
        case CMD_INSTALL:
            return strcmp(option, "--allow-unsigned") == 0 || strcmp(option, "--offline") == 0;
        case CMD_REPO:
            return strcmp(option, "--allow-insecure-http") == 0 || strcmp(option, "--offline") == 0;
        case CMD_KEYGEN: return strcmp(option, "--default") == 0;
        case CMD_KEY: return strcmp(option, "--clear") == 0;
        case CMD_UPDATE: return strcmp(option, "--offline") == 0;
        case CMD_UPGRADE:
            return strcmp(option, "--all") == 0 || strcmp(option, "-y") == 0 ||
                strcmp(option, "--yes") == 0 || strcmp(option, "--allow-unsigned") == 0 ||
                strcmp(option, "--offline") == 0;
        default: return 0;
    }
}

static int validate_command_options(Command command, int argc, char** argv, int command_index)
{
    int i;
    const char* name = command_name(command);
    for (i = command_index + 1; i < argc; i++) {
        const char* option = argv[i];
        if (option[0] != '-') continue;
        if (!command_option_is_known(command, option)) {
            printf("Error: invalid option for %s: %s.\n", name, option);
            print_usage_line(command);
            printf("Run 'wpm help %s' for more information.\n", name);
            return 0;
        }
        if (command_option_takes_value(command, option)) {
            int negative_priority = command == CMD_REPO && strcmp(option, "--priority") == 0 &&
                i + 1 < argc && argv[i + 1][0] == '-' &&
                argv[i + 1][1] >= '0' && argv[i + 1][1] <= '9';
            if (i + 1 >= argc || (argv[i + 1][0] == '-' && !negative_priority)) {
                printf("Error: option %s requires a value for %s.\n", option, name);
                print_usage_line(command);
                printf("Run 'wpm help %s' for more information.\n", name);
                return 0;
            }
            i++;
        }
    }
    return 1;
}

static int invalid_operand_shape(Command command, const char* detail)
{
    const char* name = command_name(command);
    printf("Error: %s for %s.\n", detail, name);
    print_usage_line(command);
    printf("Run 'wpm help %s' for more information.\n", name);
    return 0;
}

static int validate_command_operands(Command command, int argc, char** argv, int command_index)
{
    int i;
    int operands = 0;
    for (i = command_index + 1; i < argc; i++) {
        if (argv[i][0] == '-' && command_option_is_known(command, argv[i])) {
            if (command_option_takes_value(command, argv[i])) i++;
            continue;
        }
        operands++;
    }
    switch (command) {
        case CMD_INIT:
            if (operands > 1) return invalid_operand_shape(command, "too many operands");
            break;
        case CMD_BUILD:
            if (operands < 1) return invalid_operand_shape(command, "missing required source directory");
            if (operands > 2) return invalid_operand_shape(command, "too many operands");
            break;
        case CMD_VERIFY:
        case CMD_INSTALL:
        case CMD_REMOVE:
            if (operands < 1) return invalid_operand_shape(command, "missing required package operand");
            break;
        case CMD_KEYGEN:
            if (operands < 2) return invalid_operand_shape(command, "missing required key-file operand");
            if (operands > 2) return invalid_operand_shape(command, "too many operands");
            break;
        case CMD_UPDATE:
            if (operands > 0) return invalid_operand_shape(command, "unexpected operand");
            break;
        default:
            break;
    }
    return 1;
}

static int command_has_option(int argc, char** argv, int command_index, const char* wanted)
{
    int i;
    for (i = command_index + 1; i < argc; i++) {
        if (strcmp(argv[i], wanted) == 0) return 1;
    }
    return 0;
}

static int collect_command_operands(Command command, int argc, char** argv,
    int command_index, const char** operands, int capacity)
{
    int i;
    int count = 0;
    for (i = command_index + 1; i < argc; i++) {
        if (argv[i][0] == '-' && command_option_is_known(command, argv[i])) {
            if (command_option_takes_value(command, argv[i])) i++;
            continue;
        }
        if (count < capacity) operands[count] = argv[i];
        count++;
    }
    return count;
}

static int invalid_action_shape(Command command, const char* detail)
{
    const char* name = command_name(command);
    printf("Error: %s for %s.\n", detail, name);
    print_usage_line(command);
    printf("Run 'wpm help %s' for more information.\n", name);
    return 0;
}

static int validate_command_actions(Command command, int argc, char** argv, int command_index)
{
    const char* operands[8];
    int count = collect_command_operands(command, argc, argv, command_index, operands, 8);
    const char* action = count > 0 ? operands[0] : "list";

    if (command == CMD_REPO) {
        if ((strcmp(action, "list") == 0 || strcmp(action, "update") == 0) && count == 1) return 1;
        if (count == 0) return 1;
        if (strcmp(action, "add") == 0 && count == 2) return 1;
        if (strcmp(action, "remove") == 0 && count == 2) return 1;
        return invalid_action_shape(command, "invalid action or operand count");
    }
    if (command == CMD_KEY) {
        if (count == 1 && strcmp(action, "default") == 0 &&
            command_has_option(argc, argv, command_index, "--clear")) return 1;
        if (count == 2 && strcmp(action, "default") == 0 &&
            !command_has_option(argc, argv, command_index, "--clear")) return 1;
        return invalid_action_shape(command, "expected default with one key path or --clear");
    }
    if (command == CMD_TRUST) {
        if (count == 0 || (count == 1 && strcmp(action, "list") == 0)) return 1;
        if (count == 2 && (strcmp(action, "add") == 0 || strcmp(action, "revoke") == 0)) return 1;
        return invalid_action_shape(command, "invalid action or operand count");
    }
    if (command == CMD_CONFIG) {
        int has_package = command_has_option(argc, argv, command_index, "--package");
        if (count >= 2 && strcmp(operands[1], "prerelease") == 0) {
            if (strcmp(action, "get") == 0 && count == 2) return 1;
            if (strcmp(action, "set") == 0 && count == 3 &&
                (_stricmp(operands[2], "true") == 0 || _stricmp(operands[2], "false") == 0)) return 1;
            if (strcmp(action, "unset") == 0 && count == 2 && has_package) return 1;
        }
        return invalid_action_shape(command, "invalid action, setting, value, or operand count");
    }
    return 1;
}

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

static void print_windows_dependency_version(const char* display_name, const char* module_name, const char* description)
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
        printf("  %s unknown (%s)\n", display_name, description);
        return;
    }
    if (!GetFileVersionInfoA(path, 0, size, version_data) ||
        !VerQueryValueA(version_data, "\\", (LPVOID*)&fixed_info, &fixed_info_size) ||
        fixed_info_size < sizeof(*fixed_info)) {
        printf("  %s unknown (%s)\n", display_name, description);
        free(version_data);
        return;
    }
    printf("  %s %u.%u.%u.%u (%s)\n", display_name,
        HIWORD(fixed_info->dwFileVersionMS), LOWORD(fixed_info->dwFileVersionMS),
        HIWORD(fixed_info->dwFileVersionLS), LOWORD(fixed_info->dwFileVersionLS), description);
    free(version_data);
}

int main(int argc, char *argv[])
{
	int verbose = 0;
	int offline = 0;
    int command_index = -1;
	int show_version = 0;
	int show_diagnostics = 0;
    int command_help = 0;

    /* Resolve and remove the global color option before command parsing so it
       may appear before or after the command without becoming an operand. */
    for (int i = 1; i < argc; i++) {
        const char* color_value = NULL;
        int remove_count = 0;
        if (strcmp(argv[i], "--verbose") == 0 &&
            !(argc >= 2 && strcmp(argv[1], "--complete-self-upgrade") == 0)) {
            verbose = 1;
            remove_count = 1;
        }
        else if (strcmp(argv[i], "--color") == 0) {
            if (i + 1 < argc) {
                color_value = argv[i + 1];
                remove_count = 2;
            }
        }
        else if (strncmp(argv[i], "--color=", 8) == 0) {
            color_value = argv[i] + 8;
            remove_count = 1;
        }
        if (strncmp(argv[i], "--color", 7) == 0 &&
            (!remove_count || !wpm_set_color_policy(color_value))) {
            printf("Error: invalid --color value%s%s; expected auto, always, or never.\n",
                color_value ? ": " : "", color_value ? color_value : "");
            printf("Usage: wpm [--color auto|always|never] <command> [options]\n");
            printf("Run 'wpm --help' for more information.\n");
            return 1;
        }
        if (remove_count) {
            int j;
            for (j = i; j + remove_count < argc; j++) argv[j] = argv[j + remove_count];
            argc -= remove_count;
            i--;
        }
    }

	if (argc == 1) {
		print_version();
        if (verbose) print_runtime_mode();
        printf("Usage: wpm <command> [options]\n");
        printf("Run 'wpm --help' for commands and options.\n");
        return 0;
	}
    if ((argc == 8 || argc == 9 || argc == 10) && strcmp(argv[1], "--complete-self-upgrade") == 0) {
        DWORD parent_id = (DWORD)strtoul(argv[3], NULL, 10);
        int completed;
        int completion_verbose = strcmp(argv[argc - 1], "--verbose") == 0;
        const char* self_upgrade_log_path = argc >= 9 && strcmp(argv[8], "--verbose") != 0 ? argv[8] : NULL;
        FILE* self_upgrade_log = NULL;
        wpm_set_verbose(completion_verbose);
        SetEnvironmentVariableA("WPM_VERBOSE", completion_verbose ? "1" : NULL);
        if (self_upgrade_log_path) {
            if (fopen_s(&self_upgrade_log, self_upgrade_log_path, "a") != 0) return 1;
            fprintf(self_upgrade_log, "Completing WPM self-upgrade: %s %s -> %s\n", argv[5], argv[6], argv[4]);
            fclose(self_upgrade_log);
            SetEnvironmentVariableA("WPM_SELF_UPGRADE_LOG", self_upgrade_log_path);
        }
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parent_id);
        if (parent) {
            printf("WPM self-upgrade handoff started (PID %lu); waiting for invoking process PID %lu to exit.\n",
                (unsigned long)GetCurrentProcessId(), (unsigned long)parent_id);
            fflush(stdout);
            WaitForSingleObject(parent, INFINITE);
            CloseHandle(parent);
        }
        printf("Invoking WPM process has exited; completing the self-upgrade now.\n");
        printf("Self-upgrade stage 2 of 2: the new WPM %s independently re-verifies the "
            "package before applying it (this is why the package is validated twice).\n", argv[4]);
        fflush(stdout);
        if (!wpm_initialize_data_directories()) return 1;
        if (!wpm_log_initialize()) {
            fprintf(stderr, "Warning: WPM could not initialize its operational log.\n");
        }
        completed = wpm_archive_upgrade(argv[2], atoi(argv[7]) != 0,
            "wpm", argv[4], argv[5], argv[6]);
        printf("Result: wpm %s %s\n", argv[5], completed ? "upgraded" : "failed");
        fflush(stdout);
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
        else if ((strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) &&
                 command_index == -1) {
            print_usage(0);
            return 0;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            command_help = 1;
        }
        else if (strcmp(argv[i], "--diagnose") == 0) {
            show_diagnostics = 1;
        }
        else if (command_index == -1) {
            command_index = i;
        }
    }

    if (command_index >= 0 && strcmp(argv[command_index], "help") == 0) {
        Command help_command;
        if (command_index + 1 == argc) {
            print_usage(CMD_UNKNOWN);
            return 0;
        }
        if (command_index + 2 != argc) {
            printf("Error: help accepts one command name.\n");
            printf("Usage: wpm help [command]\n");
            return 1;
        }
        help_command = parse_command(argv[command_index + 1]);
        if (help_command == CMD_UNKNOWN) {
            printf("Error: unknown help topic: %s.\n", argv[command_index + 1]);
            printf("Usage: wpm help [command]\n");
            printf("Run 'wpm --help' for available commands.\n");
            return 1;
        }
        print_usage(help_command);
        return 0;
    }

    if (command_help && command_index >= 0) {
        Command help_command = parse_command(argv[command_index]);
        if (help_command == CMD_UNKNOWN) {
            printf("Error: unknown command: %s.\n", argv[command_index]);
            printf("Usage: wpm help [command]\n");
            return 1;
        }
        print_usage(help_command);
        return 0;
    }

    if (show_version) {
        print_version();
        if (verbose) {
            print_runtime_mode();
            print_cpu_info();
        }
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
    if (cmd != CMD_UNKNOWN && !validate_command_options(cmd, argc, argv, command_index)) return 1;
    if (cmd != CMD_UNKNOWN && !validate_command_operands(cmd, argc, argv, command_index)) return 1;
    if (cmd != CMD_UNKNOWN && !validate_command_actions(cmd, argc, argv, command_index)) return 1;
    wpm_set_verbose(verbose);
	SetEnvironmentVariableA("WPM_VERBOSE", verbose ? "1" : NULL);
	wpm_repo_set_verbose(verbose);
	if (verbose) print_runtime_mode();
	if (!wpm_initialize_data_directories()) return 1;
	if (!wpm_log_initialize()) {
        fprintf(stderr, "Warning: WPM could not initialize its operational log.\n");
    }
	
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
            int package_number = 0;
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
                wpm_archive_set_progress(++package_number, package_count);
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
                int allow_insecure_http = 0;
                int valid = command_index + 2 < argc;
                for (int i = command_index + 3; valid && i < argc; i++) {
                    if (strcmp(argv[i], "--priority") == 0 && i + 1 < argc) {
                        priority = atoi(argv[++i]);
                    } else if (strcmp(argv[i], "--allow-insecure-http") == 0) {
                        allow_insecure_http = 1;
                    } else {
                        valid = 0;
                    }
                }
                if (!valid) { printf("Usage: wpm repo add <https-url|http-url|directory|UNC-path> [--priority <integer>] [--allow-insecure-http]\n"); return 1; }
                if (!wpm_repo_add(argv[command_index + 2], priority, allow_insecure_http)) return 1;
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
                printf("Result: default signing key cleared\n");
            }
            else if (strcmp(action, "default") == 0 && command_index + 2 < argc) {
                if (!wpm_set_default_key(argv[command_index + 2])) return 1;
                printf("Result: default signing key configured\n");
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
            printf("Error: unknown command: %s.\n", argv[command_index]);
            printf("Usage: wpm <command> [options]\n");
            printf("Run 'wpm --help' for available commands.\n");
            return 1;

        case CMD_KEYGEN: {
            int make_default = argc == command_index + 4 && strcmp(argv[command_index + 3], "--default") == 0;
            if ((argc != command_index + 3 && !make_default) ||
                !wpm_keygen(argv[command_index + 1], argv[command_index + 2], make_default)) return 1;
            break;
        }

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
    printf("  minizip-ng %s (commit %s%s)\n",
        WPM_MINIZIP_NG_VERSION,
        WPM_MINIZIP_NG_COMMIT,
        WPM_MINIZIP_NG_DIRTY ? ", dirty" : "");
    printf("  zlib-ng %s (commit %s%s)\n",
        WPM_ZLIB_NG_VERSION,
        WPM_ZLIB_NG_COMMIT,
        WPM_ZLIB_NG_DIRTY ? ", dirty" : "");
    printf("  libsodium %s (commit %s%s)\n",
        WPM_SODIUM_VERSION,
        WPM_SODIUM_COMMIT,
        WPM_SODIUM_DIRTY ? ", dirty" : "");
#ifdef WPM_HAS_WCRT
    if (GetModuleHandleA("wcrt.dll")) {
        print_windows_dependency_version("wcrt", "wcrt.dll", "runtime library");
    }
    else {
        printf("  wcrt %s (runtime library)\n", WPM_WCRT_VERSION);
    }
#endif
    print_windows_dependency_version("urlmon", "urlmon.dll", "Windows system library");
    print_windows_dependency_version("advapi32", "advapi32.dll", "Windows system library");
}

void print_usage(Command c) {
    if (c != CMD_UNKNOWN) {
        switch (c) {
            case CMD_INIT:
                print_usage_line(c);
                printf("\nInitialize package metadata in the current directory.\n\nExample:\n  wpm init my-package\n");
                return;
            case CMD_BUILD:
                print_usage_line(c);
                printf("\nBuild and optionally sign a package archive.\n\nExample:\n  wpm build .\\my-package .\\dist --sign .\\release.private\n");
                return;
            case CMD_VERIFY:
                print_usage_line(c);
                printf("\nValidate packages without installing them.\n\nExample:\n  wpm verify .\\dist\\my-package-any-1.0.0.zip\n");
                return;
            case CMD_INSTALL:
                print_usage_line(c);
                printf("\nInstall a local archive or a package selected from repositories.\n\nExample:\n  wpm install my-package --arch x64\n");
                return;
            case CMD_REMOVE:
                print_usage_line(c);
                printf("\nRemove packages using their retained archives.\n\nExample:\n  wpm remove my-package-x64-1.0.0\n");
                return;
            case CMD_REPO:
                print_usage_line(c);
                printf("\nManage package repositories.\n\nExamples:\n  wpm repo add .\\repository --priority 10\n  wpm repo list\n  wpm repo update --offline\n");
                return;
            case CMD_KEYGEN:
                print_usage_line(c);
                printf("\nGenerate an Ed25519 signing-key pair.\n\nExample:\n  wpm keygen .\\release.private .\\release.public --default\n");
                return;
            case CMD_KEY:
                print_usage_line(c);
                printf("\nSelect or clear the default package-signing key.\n\nExamples:\n  wpm key default .\\release.private\n  wpm key default --clear\n");
                return;
            case CMD_TRUST:
                print_usage_line(c);
                printf("\nManage trusted package-signing keys.\n\nExamples:\n  wpm trust add .\\maintainer.public\n  wpm trust list\n  wpm trust revoke <64-character-key-id>\n");
                return;
            case CMD_CONFIG:
                print_usage_line(c);
                printf("\nConfigure prerelease package selection.\n\nExample:\n  wpm config set prerelease true --package my-package\n");
                return;
            case CMD_UPDATE:
                print_usage_line(c);
                printf("\nRefresh repository metadata and report available updates.\n\nExample:\n  wpm update\n");
                return;
            case CMD_UPGRADE:
                print_usage_line(c);
                printf("\nUpgrade selected installed packages.\n\nExamples:\n  wpm upgrade my-package\n  wpm upgrade --all --yes\n");
                return;
            default:
                break;
        }
    }
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
    printf("      Configure HTTPS, opted-in HTTP, local, or UNC package repositories\n\n");

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
    printf("  -h, --help\n");
    printf("      Display this help information\n\n");

    printf("  --version\n");
    printf("      Display WPM and dependency version information\n\n");

    printf("  --verbose\n");
    printf("      Display detailed file-operation progress; may appear before or after a command\n\n");

    printf("  --color <auto|always|never>\n");
    printf("      Control semantic output styling; default is auto\n\n");

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
    printf("  wpm init my-package\n");
    printf("  wpm verify .\\dist\\my-package-any-1.0.0.zip\n");
    printf("  wpm install my-package --arch x64\n");
    printf("  wpm remove my-package-x64-1.0.0\n");
    printf("  wpm repo add .\\repository --priority 10\n");
    printf("  wpm repo update\n");
    printf("  wpm keygen .\\release.private .\\release.public --default\n");
    printf("  wpm key default .\\release.private\n");
    printf("  wpm trust add .\\maintainer.public\n");
    printf("  wpm config set prerelease true --package my-package\n");
    printf("  wpm update\n");
    printf("  wpm upgrade --all --yes\n");
    printf("  wpm --diagnose\n");
    printf("  wpm remove my-package-x64-1.0.0 && wpm install my-package\n\n");
}

Command parse_command(const char* cmd) {
    if (strcmp(cmd, "init") == 0) return CMD_INIT;
    if (strcmp(cmd, "build") == 0) return CMD_BUILD;
    if (strcmp(cmd, "verify") == 0) return CMD_VERIFY;
    if (strcmp(cmd, "install") == 0) return CMD_INSTALL;
    if (strcmp(cmd, "remove") == 0) return CMD_REMOVE;
    if (strcmp(cmd, "repo") == 0) return CMD_REPO;
    if (strcmp(cmd, "keygen") == 0) return CMD_KEYGEN;
    if (strcmp(cmd, "key") == 0) return CMD_KEY;
    if (strcmp(cmd, "trust") == 0) return CMD_TRUST;
    if (strcmp(cmd, "config") == 0) return CMD_CONFIG;
    if (strcmp(cmd, "update") == 0) return CMD_UPDATE;
    if (strcmp(cmd, "upgrade") == 0) return CMD_UPGRADE;
    return CMD_UNKNOWN;
}
