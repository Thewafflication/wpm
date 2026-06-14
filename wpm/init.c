#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "init.h"
#include "helpers.h"

/* ---------- File Creators ---------- */

static int create_wpm_directory(void) {
    DWORD attributes = GetFileAttributesA(".wpm");

    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return 1;
        printf("Error: .wpm exists but is not a directory.\n");
        return 0;
    }

    if (!CreateDirectoryA(".wpm", NULL)) {
        printf("Error: could not create .wpm directory.\n");
        return 0;
    }

    printf("Created .wpm/\n");
    return 1;
}

static int create_package_txt(const char* name) {
    const char* path = ".wpm/package.txt";

    if (file_exists(path)) return 1;

    FILE* f = fopen(path, "w");
    if (!f) return 0;

    int result = fprintf(f,
        "name=%s\n"
        "version=0.1.0\n"
        "description=\n"
        "license=\n"
        "build_date=\n"
        "repo=\n"
        "homepage=\n"
        "maintainer=\n",
        name
    );

    int close_result = fclose(f);
    return result >= 0 && close_result == 0;
}

static int create_index_csv(void) {
    const char* path = ".wpm/index.csv";

    if (file_exists(path)) return 1;

    FILE* f = fopen(path, "w");
    if (!f) return 0;

    int result = fprintf(f, "filename,crc\n");
    int close_result = fclose(f);
    return result >= 0 && close_result == 0;
}

static int create_ignore(void) {
    const char* path = ".wpm/wpmignore.txt";

    if (file_exists(path)) return 1;

    FILE* f = fopen(path, "w");
    if (!f) return 0;

    int result = fprintf(f,
        ".wpm/\n"
        "*.log\n"
        "build/\n"
    );

    int close_result = fclose(f);
    return result >= 0 && close_result == 0;
}

static int create_install_cmd(void) {
    const char* path = ".wpm/install.cmd";

    if (file_exists(path)) return 1;

    FILE* f = fopen(path, "w");
    if (!f) return 0;

    int result = fprintf(f,
        "@echo off\n"
        "echo Installing package...\n"
        "REM Add install commands below\n"
    );

    int close_result = fclose(f);
    return result >= 0 && close_result == 0;
}

static int create_remove_cmd(void) {
    const char* path = ".wpm/remove.cmd";

    if (file_exists(path)) return 1;

    FILE* f = fopen(path, "w");
    if (!f) return 0;

    int result = fprintf(f,
        "@echo off\n"
        "echo Removing package...\n"
        "REM Add removal commands below\n"
    );

    int close_result = fclose(f);
    return result >= 0 && close_result == 0;
}

/* ---------- Public ---------- */

int wpm_current_directory_name(char* name, size_t size) {
    char path[MAX_PATH];
    char* directory_name;
    DWORD length;

    if (!name || size == 0) return 0;

    length = GetCurrentDirectoryA(sizeof(path), path);
    if (length == 0 || length >= sizeof(path)) {
        printf("Error: could not determine the current directory.\n");
        return 0;
    }

    while (length > 3 && (path[length - 1] == '\\' || path[length - 1] == '/')) {
        path[--length] = '\0';
    }

    directory_name = strrchr(path, '\\');
    if (!directory_name) directory_name = strrchr(path, '/');
    directory_name = directory_name ? directory_name + 1 : path;

    if (!directory_name[0]) {
        printf("Error: the current directory does not have a package name.\n");
        return 0;
    }
    if (strlen(directory_name) >= size) {
        printf("Error: current directory name exceeds the package name limit.\n");
        return 0;
    }

    strcpy_s(name, size, directory_name);
    return 1;
}

int wpm_package_name_is_valid(const char* name) {
    if (!name || !name[0] || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;

    for (const unsigned char* current = (const unsigned char*)name; *current; current++) {
        if (!isalnum(*current) && *current != '-' && *current != '_' && *current != '.') {
            return 0;
        }
    }

    return 1;
}

int wpm_init_run(const char* name) {
    if (!create_wpm_directory()) return 0;

    if (!create_package_txt(name) ||
        !create_index_csv() ||
        !create_ignore() ||
        !create_install_cmd() ||
        !create_remove_cmd()) {
        printf("Error: could not create one or more package files.\n");
        return 0;
    }

    return 1;
}
