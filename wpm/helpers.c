#include <stdio.h>
#include "helpers.h"

int file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

void get_input(const char* prompt, char* buffer, size_t size) {
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, size, stdin)) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
}