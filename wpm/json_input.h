/** @file json_input.h @brief Shared JSON schema number recognition. */
#ifndef WPM_JSON_INPUT_H
#define WPM_JSON_INPUT_H

#include <stddef.h>

/** Skip JSON whitespace in a non-NULL NUL-terminated borrowed string. */
const char *wpm_json_space(const char *text);
/** Skip an optional UTF-8 BOM and whitespace; borrow the resulting pointer. */
const char *wpm_json_content(const char *text);
/** Validate complete JSON grammar without recursion or filesystem access.
 * Input must be bounded and NUL-terminated. Does not validate raw UTF-8 or
 * duplicate keys; schema admission and encoding checks remain with callers.
 * Allocation or grammar failure returns zero. */
int wpm_json_document_valid(const char *text);
/** Decode one JSON string into an optional bounded caller-owned buffer.
 * Advances the borrowed cursor only on success. Escaped NUL cannot enter an
 * output buffer; surrogate pairs are validated. Raw UTF-8 is caller-checked.
 */
int wpm_json_read_string(const char **source, char *output, size_t size);
/** Advance a cursor over one validated value to its enclosing delimiter.
 * Caller first validates document grammar; no allocation or mutation occurs.
 */
int wpm_json_value_end(const char **source);
/** Borrow the first matching root-object value; NULL if absent or malformed.
 * This preserves legacy catalog lookup. It does not reject duplicate fields.
 */
const char *wpm_json_field(const char *document, const char *name);
/** Borrow exactly one matching root-object value; duplicate/missing is NULL.
 * Caller validates the complete document before extracting fields. */
const char *wpm_json_unique_field(const char *document, const char *name);

/** Recognize a root object's version as the JSON number exactly one.
 * Uses decimal digits/exponent without floating-point rounding. Callers retain
 * their document grammar, duplicate-field, and remaining schema checks. */
int wpm_json_version_one(const char *document);

#endif
