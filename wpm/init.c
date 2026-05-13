#include <stdio.h>
#include "init.h"
#include "helpers.h"

/* ---------- File Creators ---------- */

static void create_package_txt(const char* name) {
    const char* path = ".wpm/package.txt";

    if (file_exists(path)) return;

    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
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

    fclose(f);
}

static void create_index_csv(void) {
    const char* path = ".wpm/index.csv";

    if (file_exists(path)) return;

    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "filename,crc\n");
    fclose(f);
}

static void create_ignore(void) {
    const char* path = ".wpm/wpmignore.txt";

    if (file_exists(path)) return;

    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        ".wpm/\n"
        "*.log\n"
        "build/\n"
    );

    fclose(f);
}

static void create_install_cmd(void) {
    const char* path = ".wpm/install.cmd";

    if (file_exists(path)) return;

    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        "@echo off\n"
        "echo Installing package...\n"
        "REM Add install commands below\n"
    );

    fclose(f);
}

static void create_remove_cmd(void) {
    const char* path = ".wpm/remove.cmd";

    if (file_exists(path)) return;

    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        "@echo off\n"
        "echo Removing package...\n"
        "REM Add removal commands below\n"
    );

    fclose(f);
}

/* ---------- Public ---------- */

void wpm_init_run(const char* name) {
    create_package_txt(name);
    create_index_csv();
    create_ignore();
    create_install_cmd();
    create_remove_cmd();
}