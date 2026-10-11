/** @file capability-probe.c @brief Direct tests below command dispatch. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "capability.h"
#include "helpers.h"
#include "logging.h"
int wpm_event_printf(wpm_event_class kind, const char *code,
                     const char *operation, const char *phase,
                     const char *format, ...) {
    (void)kind;
    (void)code;
    (void)operation;
    (void)phase;
    (void)format;
    return 0;
}
int main(int argc, char **argv) {
    FILE *file;
    char text[16] = "";
    const char *modes[] = {"w", "wb", "a", "ab", "r+", "w+", "a+"};
    if (argc != 2) {
        return 1;
    }
    file = wpm_fopen(argv[1], "wb");
    if (!file) {
        return 1;
    }
    fputs("sentinel", file);
    fclose(file);
    wpm_set_readonly(1);
    wpm_set_readonly(
        0); /* A lower-level caller cannot restore write authority. */
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        file = wpm_fopen(argv[1], modes[i]);
        if (file) {
            fclose(file);
            return 1;
        }
    }
    if (wpm_require_capability(WPM_CAP_WRITE, "write") ||
        wpm_require_capability(WPM_CAP_SCRIPT, "script") ||
        wpm_require_capability(WPM_CAP_HANDOFF, "handoff") ||
        wpm_require_capability(WPM_CAP_NETWORK, "network")) {
        return 1;
    }
    file = wpm_fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    fgets(text, sizeof(text), file);
    fclose(file);
    if (strcmp(text, "sentinel")) {
        return 1;
    }
    remove(argv[1]);
    puts(
        "PASS: direct write, script, handoff, and network capabilities denied");
    return 0;
}
