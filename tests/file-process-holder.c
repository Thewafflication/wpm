/** @file file-process-holder.c @brief Native NT 5.x file/module test holder. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    HANDLE file = INVALID_HANDLE_VALUE;
    HMODULE module = NULL;
    if (argc != 3) {
        return 2;
    }
    if (!strcmp(argv[1], "hold")) {
        file =
            CreateFileA(argv[2], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            return 3;
        }
    } else if (!strcmp(argv[1], "module")) {
        char source[MAX_PATH];
        UINT length = GetSystemDirectoryA(source, sizeof(source));
        if (!length || length > MAX_PATH - 14) {
            return 4;
        }
        strcat(source, "\\version.dll");
        if (!CopyFileA(source, argv[2], TRUE) ||
            !(module = LoadLibraryA(argv[2]))) {
            return 5;
        }
    } else if (strcmp(argv[1], "sleep")) {
        return 6;
    }
    puts("ready");
    fflush(stdout);
    Sleep(120000);
    if (module) {
        FreeLibrary(module);
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    return 0;
}
