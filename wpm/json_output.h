/** @file json_output.h @brief Safe UTF-8 JSON field serialization. */
#ifndef WPM_JSON_OUTPUT_H
#define WPM_JSON_OUTPUT_H

#include <stdio.h>

/** Write a filtered JSON field to an owned stream without console events. */
int wpm_json_write_string(FILE *output, const char *text);

/** Allocate one filtered JSON field; NULL denotes encoding/allocation failure.
 * Caller frees the result. A NULL input encodes the JSON literal null. */
char *wpm_json_encode_string(const char *text);

/** Write one redacted JSON string to stdout, or null for unavailable text.
 * Invalid UTF-8/allocation failure writes null and returns zero. Fields must
 * be bounded, NUL-terminated public/support text, never private key material.
 */
int wpm_json_string(const char *text);

/** Capture up to 32 typed redacted diagnostics without console/log writes. */
void wpm_json_events_begin(void);
/** Write the captured array, release its text, and restore console rendering.
 * Returns zero on allocation, bounds, encoding, or output failure.
 */
int wpm_json_events_end(void);

#endif
