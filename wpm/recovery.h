/** @file recovery.h @brief Read-only failure audit inspection. */
#ifndef WPM_RECOVERY_H
#define WPM_RECOVERY_H

/** List recorded failed upgrades without scripts, web retrieval, or writes.
 * Returns nonzero for a complete query, including an empty audit directory. */
int wpm_recovery_list(void);

/** Examine between 1 and 4096 matching entries; defaults remain unchanged. */
int wpm_recovery_list_limit(unsigned limit);

/** Render the same bounded query as UTF-8 wpm.recovery-list.v1 JSON. */
int wpm_recovery_list_json(unsigned limit);

/** Validate optional bounded native name and supported architecture filters. */
int wpm_recovery_filters_valid(const char *name, const char *arch);
/** Filter the same examined subset; source-wide invalid/limit health remains.
 * JSON filter/selection fields are optional; original wrappers are unchanged.
 */
int wpm_recovery_list_filtered(unsigned limit, int json, const char *name,
                               const char *arch);

/** Validate one lowercase 128-bit operation ID without interpreting paths. */
int wpm_recovery_id_valid(const char *id);
/** Query additive recovery records without writes, fetches, or scripts.
 * @param limit Bound on examined records, 1..4096; source errors remain
 * visible.
 * @param json Nonzero selects wpm.recovery-query.v1 instead of human events.
 * @param id Optional validated operation ID for a single inspection.
 * @param archive Optional explicit current input for inspection only; stored
 * paths never grant authority. NULL leaves current input
 * unverified/non-success.
 * @param allow_unsigned Explicit validation override, requires archive.
 * @param name Optional bounded package filter.
 * @param arch Optional supported architecture filter.
 * @return Nonzero for a complete query, never a retry/cleanup authorization.
 * All arguments are borrowed; the query owns/releases records and path guards.
 */
int wpm_recovery_records(unsigned limit, int json, const char *id,
                         const char *archive, int allow_unsigned,
                         const char *name, const char *arch);

#endif
