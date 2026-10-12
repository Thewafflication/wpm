/** @file json_input.h @brief Shared JSON schema number recognition. */
#ifndef WPM_JSON_INPUT_H
#define WPM_JSON_INPUT_H

/** Recognize a root object's version as the JSON number exactly one.
 * Uses decimal digits/exponent without floating-point rounding. Callers retain
 * their document grammar, duplicate-field, and remaining schema checks. */
int wpm_json_version_one(const char *document);

#endif
