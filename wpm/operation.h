/** @file operation.h @brief Immutable package plans and execution
 * authorization. */
#ifndef WPM_OPERATION_H
#define WPM_OPERATION_H
#include <stddef.h>
/** Begin collecting a command's complete package plan. */
void wpm_operation_begin(int assume_yes);
/** Whether the dispatcher is currently collecting plans instead of executing.
 */
int wpm_operation_collecting(void);
/** Whether any package plan authorization is active. */
int wpm_operation_active(void);
/** Bind repository selection to its exact source locator. */
void wpm_operation_set_source(const char *source);
/** Approve separately bounded cache preparation before its first write. */
int wpm_operation_prepare(const char *source, const char *destination);
/** Register or revalidate an exact archive/identity/source authorization. */
int wpm_operation_archive(const char *action, const char *archive,
                          const char *old_version, const char *source,
                          int allow_unsigned);
/** Render, bind approval to the digest, and enter execution; zero cancels. */
/** Returns 1 approved, 0 cancelled, or -1 invalidated. */
int wpm_operation_confirm(void);
/** Recheck inputs and selected state immediately before a package script. */
int wpm_operation_before_script(void);
/** Record the new controlled state after a completed authorized item. */
void wpm_operation_completed(void);
/** Release plan inputs and memory. */
void wpm_operation_close(void);
#endif
