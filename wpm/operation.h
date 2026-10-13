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
int wpm_operation_completed(void);

/** Commit external-state uncertainty before attempting a package process. */
int wpm_operation_external_start(const char *phase);
/** Retain the observed script exit; failed writes block further mutation. */
int wpm_operation_script_result(unsigned long exit_code);
/** Durably begin a bounded controlled phase; no inferred completion occurs. */
int wpm_operation_phase_start(const char *phase);
/** Commit a known phase completion, bounded to 16 entries per operation. */
int wpm_operation_phase_finish(const char *phase);
/** Derive an approved canonical retained name; zero means legacy behavior.
 * Returns -1 on invalid state/capacity, 1 on success. Does not write files.
 * The caller owns output; identity is taken only from the active approved plan.
 */
int wpm_operation_store_name(char *output, size_t size);

/** Select the retained immutable input for the active approved archive.
 * Return 1 with a borrowed native read handle for the selected archive, 0 for
 * inactive/unrelated input, or -1 for an invalid selected context. Never reopen
 * a pathname or grant authority from a recorded path. Cursor is mutable; calls
 * are serialized by the operation dispatcher. Ownership remains with the plan.
 */
int wpm_operation_input(const char *archive, void **input);

/** Retain approved bytes at the active plan's canonical package-store name.
 * Return 0 for legacy execution, 1 on committed success, -1 on failure. The
 * destination must exactly match the selected data root and approved identity.
 * Copy reads the held recovery input, verifies/flushes a fresh temporary file,
 * and publishes it atomically by handle. No original pathname is reopened.
 * Failure preserves temporary evidence and never widens destination scope.
 */
int wpm_operation_store_input(const char *destination);
/** Release plan inputs and memory. */
void wpm_operation_close(void);
#endif
