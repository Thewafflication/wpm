/** @file path_guard.h @brief Canonical local-drive directory guards.
 * This Windows boundary holds ancestors against rename and rejects reparse
 * paths. It does not grant authority to execute a recorded path or claim
 * support for device/UNC namespaces. Guards are process-local and not shared
 * across threads; release every successfully or partially opened guard.
 */
#ifndef WPM_PATH_GUARD_H
#define WPM_PATH_GUARD_H

#include <stddef.h>

/** Opaque owned collection of directory identity handles. */
typedef struct wpm_path_guard wpm_path_guard;

/** Distinguish absent query state from unsafe or inaccessible state. */
typedef enum {
    WPM_PATH_OPEN,
    WPM_PATH_ABSENT,
    WPM_PATH_UNSAFE,
    WPM_PATH_FAILED
} wpm_path_result;

/** Canonicalize and guard every directory ancestor, including the volume root.
 * @param path Borrowed bounded NUL-terminated directory path.
 * @param create Zero guarantees no mutation; one permits write-capable
 * bootstrap; two additionally requires a fresh final operation directory.
 * @param canonical Caller-owned output buffer; valid only on OPEN.
 * @param size Capacity of canonical, including its terminator.
 * @param guard Receives an owned guard, also on partial failure; release it.
 * @return OPEN only for guarded ordinary directories. Missing read-only paths
 * return ABSENT; reparse/device/UNC paths return UNSAFE; other errors FAILED.
 * No content, trust, cleanup, or mutation authority is inferred from a guard.
 */
wpm_path_result wpm_path_guard_open(const char *path, int create,
                                    char *canonical, size_t size,
                                    wpm_path_guard **guard);

/** Release a guard and its handles; NULL is safe. Does not mutate filesystem.
 * @param guard Owned guard consumed by this call; no subsequent access. */
void wpm_path_guard_close(wpm_path_guard *guard);

/** Atomically rename an exclusively held DELETE-capable file within its own
 * guarded parent, without closing it or traversing a destination pathname.
 * File must match the latest successful fresh creation through this guard;
 * caller must flush its bytes first. Unknown source identities are rejected.
 * Only a bounded ASCII basename is accepted; no path or stream syntax.
 * File and guard remain borrowed. Missing native API or any failure fails
 * closed.
 */
int wpm_path_guard_publish(wpm_path_guard *guard, void *file, const char *name,
                           int replace);

/** Open one ordinary single-link file relative to the held final directory.
 * Fresh nonzero creates an exclusive READ/WRITE/DELETE-capable file; zero
 * opens existing read-only, denying writes/deletes. Returns an owned native
 * handle (CloseHandle), or NULL with a Windows error. No path/stream syntax.
 * Ancestors are checked before/after; creation failure preserves any evidence.
 */
void *wpm_path_guard_file(wpm_path_guard *guard, const char *name, int fresh);

/** Enumerate matching JSON basenames through the held final directory handle.
 * Return 1 for a bounded name, 0 at end, -1 on unsafe/unreadable state.
 * Restart nonzero begins a new scan; one scan per guard, no shared cursor.
 */
int wpm_path_guard_next_json(wpm_path_guard *guard, char *name, size_t size,
                             int restart);

#endif
