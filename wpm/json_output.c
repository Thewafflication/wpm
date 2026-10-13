/** @file json_output.c @brief JSON escaping after shared text filtering. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_output.h"
#include "logging.h"

typedef struct json_event {
    wpm_event_class kind;
    char *code, *operation, *phase, *text;
} json_event;

static json_event events[32];
static unsigned event_count;
static int events_complete;

static void capture_event(wpm_event_class kind, const char *code,
                          const char *operation, const char *phase,
                          const char *text) {
    json_event *event;
    size_t code_size = strlen(code) + 1;
    size_t operation_size = strlen(operation) + 1;
    size_t phase_size = strlen(phase) + 1;
    if (event_count == sizeof(events) / sizeof(events[0])) {
        events_complete = 0;
        return;
    }
    event = &events[event_count];
    event->text =
        malloc(strlen(text) + 1 + code_size + operation_size + phase_size);
    if (!event->text) {
        events_complete = 0;
        return;
    }
    strcpy(event->text, text);
    event->kind = kind;
    event->code = event->text + strlen(text) + 1;
    event->operation = event->code + code_size;
    event->phase = event->operation + operation_size;
    memcpy(event->code, code, code_size);
    memcpy(event->operation, operation, operation_size);
    memcpy(event->phase, phase, phase_size);
    event_count++;
}

void wpm_json_events_begin(void) {
    for (unsigned i = 0; i < event_count; i++) {
        free(events[i].text);
    }
    event_count = 0;
    events_complete = 1;
    wpm_event_set_sink(capture_event);
}

int wpm_json_events_end(void) {
    static const char *classes[] = {"info",    "progress", "success",
                                    "warning", "error",    "prompt",
                                    "script",  "result",   "debug"};
    fputc('[', stdout);
    for (unsigned i = 0; i < event_count; i++) {
        json_event *event = &events[i];
        if (i) {
            fputc(',', stdout);
        }
        fputs("{\"severity\":", stdout);
        events_complete &= wpm_json_string(classes[event->kind]);
        fputs(",\"code\":", stdout);
        events_complete &= wpm_json_string(event->code);
        fputs(",\"operation\":", stdout);
        events_complete &= wpm_json_string(event->operation);
        fputs(",\"phase\":", stdout);
        events_complete &= wpm_json_string(event->phase);
        fputs(",\"message\":", stdout);
        events_complete &= wpm_json_string(event->text);
        fputc('}', stdout);
        free(event->text);
    }
    fputc(']', stdout);
    event_count = 0;
    wpm_event_set_sink(NULL);
    return events_complete && !ferror(stdout);
}

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
