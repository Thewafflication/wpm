#include <limits.h>
#include <stdio.h>
#include <string.h>
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
    int input_size;

    if (size == 0) return;
    input_size = size > INT_MAX ? INT_MAX : (int)size;

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, input_size, stdin)) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
}
