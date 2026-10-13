/** @file json_input.c @brief Bounded-input C99 JSON lexical helpers.
 * Callers provide NUL-terminated documents with explicit size bounds. Grammar
 * admission is iterative; field extraction borrows input and never executes it.
 */
#include <stdlib.h>
#include <string.h>
#include "json_input.h"

const char *wpm_json_space(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return p;
}
static int json_hex4(const char **source, unsigned long *value) {
    const char *p = *source;
    unsigned long result = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)*p;
        unsigned long digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return 0;
        }
        result = (result << 4) | digit;
        p++;
    }
    *source = p;
    *value = result;
    return 1;
}
int wpm_json_read_string(const char **source, char *output, size_t size) {
    const char *p = wpm_json_space(*source);
    size_t n = 0;
    if (*p != '"') {
        return 0;
    }
    p++;
    while (*p && *p != '"') {
        unsigned long codepoint = (unsigned char)*p++;
        unsigned char bytes[4];
        size_t length = 1;
        if (codepoint < 0x20) {
            return 0;
        }
        if (codepoint == '\\') {
            switch (*p++) {
            case '"':
                codepoint = '"';
                break;
            case '\\':
                codepoint = '\\';
                break;
            case '/':
                codepoint = '/';
                break;
            case 'b':
                codepoint = '\b';
                break;
            case 'f':
                codepoint = '\f';
                break;
            case 'n':
                codepoint = '\n';
                break;
            case 'r':
                codepoint = '\r';
                break;
            case 't':
                codepoint = '\t';
                break;
            case 'u': {
                unsigned long low;
                if (!json_hex4(&p, &codepoint) || (!codepoint && output)) {
                    return 0;
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (*p != '\\' || p[1] != 'u') {
                        return 0;
                    }
                    p += 2;
                    if (!json_hex4(&p, &low) || low < 0xdc00 || low > 0xdfff) {
                        return 0;
                    }
                    codepoint =
                        0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return 0;
                }
                if (codepoint < 0x80) {
                    bytes[0] = (unsigned char)codepoint;
                } else if (codepoint < 0x800) {
                    length = 2;
                    bytes[0] = (unsigned char)(0xc0 | (codepoint >> 6));
                    bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3f));
                } else if (codepoint < 0x10000) {
                    length = 3;
                    bytes[0] = (unsigned char)(0xe0 | (codepoint >> 12));
                    bytes[1] =
                        (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
                    bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3f));
                } else {
                    length = 4;
                    bytes[0] = (unsigned char)(0xf0 | (codepoint >> 18));
                    bytes[1] =
                        (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
                    bytes[2] =
                        (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
                    bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3f));
                }
                if (length >= size || n >= size - length) {
                    return 0;
                }
                if (output) {
                    memcpy(output + n, bytes, length);
                }
                n += length;
                continue;
            }
            default:
                return 0;
            }
        }
        if (n + 1 >= size) {
            return 0;
        }
        if (output) {
            output[n] = (char)codepoint;
        }
        n++;
    }
    if (*p != '"') {
        return 0;
    }
    if (output) {
        output[n] = '\0';
    }
    *source = p + 1;
    return 1;
}
int wpm_json_value_end(const char **source) {
    const char *p = wpm_json_space(*source);
    int depth = 0, quote = 0;
    while (*p) {
        if (quote) {
            if (*p == '\\' && p[1]) {
                p += 2;
            } else {
                if (*p == '"') {
                    quote = 0;
                }
                p++;
            }
            continue;
        }
        if (*p == '"') {
            quote = 1;
        } else if (*p == '{' || *p == '[') {
            depth++;
        } else if (*p == '}' || *p == ']') {
            if (!depth) {
                break;
            }
            depth--;
        } else if (*p == ',' && depth == 0) {
            break;
        }
        p++;
    }
    *source = p;
    return depth == 0 && !quote;
}
const char *wpm_json_content(const char *text) {
    const unsigned char *bytes = (const unsigned char *)text;
    if (bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) {
        text += 3;
    }
    return wpm_json_space(text);
}
/* Iterative grammar validation: depth is bounded only by the 4 MiB input,
 * not by the C call stack. Unknown extension values remain valid JSON. */
int wpm_json_document_valid(const char *text) {
    enum {
        ROOT_VALUE,
        ROOT_END,
        OBJECT_FIRST,
        OBJECT_KEY,
        OBJECT_COLON,
        OBJECT_VALUE,
        OBJECT_END,
        ARRAY_FIRST,
        ARRAY_VALUE,
        ARRAY_END
    };
    const char *p = wpm_json_content(text);
    size_t depth = 0, capacity = strlen(p) + 1;
    unsigned char *states = malloc(capacity);
    int valid = 0;
    if (!states) {
        return 0;
    }
    states[0] = ROOT_VALUE;
    for (;;) {
        unsigned char state = states[depth];
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (state == ROOT_END) {
            valid = !*p;
            break;
        }
        if (state == OBJECT_FIRST || state == OBJECT_KEY) {
            if (state == OBJECT_FIRST && *p == '}') {
                p++;
                depth--;
                continue;
            }
            if (!wpm_json_read_string(&p, NULL, (size_t)-1)) {
                break;
            }
            states[depth] = OBJECT_COLON;
            continue;
        }
        if (state == OBJECT_COLON) {
            if (*p != ':') {
                break;
            }
            p++;
            states[depth] = OBJECT_VALUE;
            continue;
        }
        if (state == OBJECT_END || state == ARRAY_END) {
            if (*p == (state == OBJECT_END ? '}' : ']')) {
                p++;
                depth--;
                continue;
            }
            if (*p != ',') {
                break;
            }
            p++;
            states[depth] = state == OBJECT_END ? OBJECT_KEY : ARRAY_VALUE;
            continue;
        }
        if (state == ARRAY_FIRST && *p == ']') {
            p++;
            depth--;
            continue;
        }
        states[depth] = state == ROOT_VALUE     ? ROOT_END
                        : state == OBJECT_VALUE ? OBJECT_END
                                                : ARRAY_END;
        if (*p == '{' || *p == '[') {
            if (depth + 1 >= capacity) {
                break;
            }
            states[++depth] = *p++ == '{' ? OBJECT_FIRST : ARRAY_FIRST;
            continue;
        }
        if (*p == '"') {
            if (!wpm_json_read_string(&p, NULL, (size_t)-1)) {
                break;
            }
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            if (*p == '-') {
                p++;
            }
            if (*p == '0') {
                p++;
            } else {
                if (*p < '1' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
            if (*p == '.') {
                p++;
                if (*p < '0' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') {
                    p++;
                }
                if (*p < '0' || *p > '9') {
                    break;
                }
                do {
                    p++;
                } while (*p >= '0' && *p <= '9');
            }
        } else if (!strncmp(p, "true", 4)) {
            p += 4;
        } else if (!strncmp(p, "false", 5)) {
            p += 5;
        } else if (!strncmp(p, "null", 4)) {
            p += 4;
        } else {
            break;
        }
    }
    free(states);
    return valid;
}
/* Find root fields without confusing package metadata with index metadata. */
const char *wpm_json_field(const char *text, const char *wanted) {
    const char *p = wpm_json_content(text);
    if (*p != '{') {
        return NULL;
    }
    p++;
    while (*(p = wpm_json_space(p)) && *p != '}') {
        char key[32];
        if (!wpm_json_read_string(&p, key, sizeof(key))) {
            /* A long or NUL-containing unknown key need not fit our key buffer.
             */
            if (!wpm_json_read_string(&p, NULL, (size_t)-1)) {
                return NULL;
            }
            key[0] = 0;
        }
        if (*(p = wpm_json_space(p)) != ':') {
            return NULL;
        }
        p = wpm_json_space(p + 1);
        if (strcmp(key, wanted) == 0) {
            return p;
        }
        wpm_json_value_end(&p);
        p = wpm_json_space(p);
        if (*p != ',') {
            return NULL;
        }
        p++;
    }
    return NULL;
}
int wpm_json_version_one(const char *text) {
    const char *value = text ? wpm_json_field(text, "version") : NULL,
               *p = value;
    int position = 0, integer_digits = 0, nonzero = -1;
    int exponent = 0, exponent_sign = 1;
    if (!p || *p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        if (*p != '0') {
            if (*p != '1' || nonzero >= 0) {
                return 0;
            }
            nonzero = position;
        }
        position++;
        integer_digits++;
        p++;
    }
    if (integer_digits > 1 && *value == '0') {
        return 0;
    }
    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9') {
            return 0;
        }
        while (*p >= '0' && *p <= '9') {
            if (*p != '0') {
                if (*p != '1' || nonzero >= 0) {
                    return 0;
                }
                nonzero = position;
            }
            position++;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-' || *p == '+') {
            exponent_sign = *p++ == '-' ? -1 : 1;
        }
        if (*p < '0' || *p > '9') {
            return 0;
        }
        while (*p >= '0' && *p <= '9') {
            int digit = *p++ - '0';
            /* A 4 MiB token cannot balance an exponent larger than this. */
            if (exponent > (4 * 1024 * 1024 - digit) / 10) {
                return 0;
            }
            exponent = exponent * 10 + digit;
        }
    }
    p = wpm_json_space(p);
    return nonzero >= 0 &&
           integer_digits - nonzero - 1 + exponent_sign * exponent == 0 &&
           (*p == ',' || *p == '}');
}

const char *wpm_json_unique_field(const char *document, const char *name) {
    const char *p = wpm_json_content(document), *result = NULL;
    if (*p != '{') {
        return NULL;
    }
    p++;
    while (*(p = wpm_json_space(p)) && *p != '}') {
        char key[128];
        if (!wpm_json_read_string(&p, key, sizeof(key))) {
            if (!wpm_json_read_string(&p, NULL, (size_t)-1)) {
                return NULL;
            }
            key[0] = 0;
        }
        if (*(p = wpm_json_space(p)) != ':') {
            return NULL;
        }
        p = wpm_json_space(p + 1);
        if (!strcmp(key, name)) {
            if (result) {
                return NULL;
            }
            result = p;
        }
        if (!wpm_json_value_end(&p)) {
            return NULL;
        }
        p = wpm_json_space(p);
        if (*p != ',') {
            break;
        }
        p++;
    }
    return *p == '}' ? result : NULL;
}
