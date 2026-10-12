/** @file json_output.c @brief JSON escaping after shared text filtering. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_output.h"
#include "logging.h"

static int json_utf8(const unsigned char *text) {
    while (*text) {
        unsigned value = *text++, minimum;
        int remaining;
        if (value < 0x80) {
            continue;
        }
        if (value >= 0xc2 && value <= 0xdf) {
            value &= 0x1f;
            minimum = 0x80;
            remaining = 1;
        } else if (value >= 0xe0 && value <= 0xef) {
            value &= 0x0f;
            minimum = 0x800;
            remaining = 2;
        } else if (value >= 0xf0 && value <= 0xf4) {
            value &= 7;
            minimum = 0x10000;
            remaining = 3;
        } else {
            return 0;
        }
        while (remaining--) {
            unsigned next = *text;
            if (next < 0x80 || next > 0xbf) {
                return 0;
            }
            text++;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff)) {
            return 0;
        }
    }
    return 1;
}

int wpm_json_string(const char *text) {
    char *safe;
    const unsigned char *p;
    if (!text) {
        fputs("null", stdout);
        return 1;
    }
    safe = malloc(strlen(text) + 1);
    if (!safe) {
        fputs("null", stdout);
        return 0;
    }
    strcpy(safe, text);
    wpm_redact_text(safe);
    if (!json_utf8((const unsigned char *)safe)) {
        free(safe);
        fputs("null", stdout);
        return 0;
    }
    fputc('"', stdout);
    for (p = (const unsigned char *)safe; *p; p++) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', stdout);
            fputc(*p, stdout);
        } else if (*p < 0x20) {
            fprintf(stdout, "\\u%04x", (unsigned)*p);
        } else {
            fputc(*p, stdout);
        }
    }
    fputc('"', stdout);
    free(safe);
    return !ferror(stdout);
}
