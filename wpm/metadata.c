/** @file metadata.c @brief Optional descriptive package metadata. */
#include <stdio.h>
#include <string.h>
#include "metadata.h"
#include "json_output.h"

const char *const wpm_metadata_names[WPM_METADATA_FIELDS] = {
    "description", "tags",       "repo",      "homepage",
    "license",     "maintainer", "build_date"};

int wpm_metadata_field(const char *name) {
    for (int i = 0; i < WPM_METADATA_FIELDS; i++) {
        if (!_stricmp(name, wpm_metadata_names[i])) {
            return i;
        }
    }
    return -1;
}

int wpm_metadata_set(wpm_optional_metadata *metadata, int field,
                     const char *value) {
    size_t length = strlen(value);
    if (field < 0 || field >= WPM_METADATA_FIELDS ||
        length >= WPM_METADATA_SIZE || !wpm_json_utf8_valid(value)) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p < 32 || *p == 127) {
            return 0;
        }
    }
    memcpy(metadata->values[field], value, length + 1);
    return 1;
}

int wpm_metadata_json(const wpm_optional_metadata *metadata, char *output,
                      size_t capacity) {
    size_t used = 0;
    if (!capacity) {
        return 0;
    }
    output[0] = 0;
    for (int i = 0; i < WPM_METADATA_FIELDS; i++) {
        const char *value = metadata->values[i];
        int length;
        if (!*value) {
            continue;
        }
        length = snprintf(output + used, capacity - used, ",\"%s\":\"",
                          wpm_metadata_names[i]);
        if (length < 0 || (size_t)length >= capacity - used) {
            return 0;
        }
        used += (size_t)length;
        while (*value) {
            if (capacity - used < 4) {
                return 0;
            }
            if (*value == '"' || *value == '\\') {
                output[used++] = '\\';
            }
            output[used++] = *value++;
        }
        if (capacity - used < 2) {
            return 0;
        }
        output[used++] = '"';
        output[used] = 0;
    }
    return 1;
}
