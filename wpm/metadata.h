/** @file metadata.h @brief Optional descriptive package metadata. */
#ifndef WPM_METADATA_H
#define WPM_METADATA_H

#include <stddef.h>

#define WPM_METADATA_FIELDS 7
#define WPM_METADATA_SIZE 1024
#define WPM_METADATA_JSON_SIZE (WPM_METADATA_FIELDS * 2100)

/** Optional UTF-8, single-line informational values, never execution inputs. */
typedef struct {
    char values[WPM_METADATA_FIELDS][WPM_METADATA_SIZE];
} wpm_optional_metadata;

extern const char *const wpm_metadata_names[WPM_METADATA_FIELDS];
/** Return a known field index, or -1 for an extension not interpreted by WPM.
 */
int wpm_metadata_field(const char *name);
/** Validate and copy a single-line value; empty means omitted. */
int wpm_metadata_set(wpm_optional_metadata *metadata, int field,
                     const char *value);
/** Serialize nonempty fields as comma-prefixed JSON object members. */
int wpm_metadata_json(const wpm_optional_metadata *metadata, char *output,
                      size_t capacity);

#endif
