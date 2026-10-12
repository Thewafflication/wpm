/** @file repository.h @brief Public package repository interface. */
#ifndef WPM_REPOSITORY_H
#define WPM_REPOSITORY_H

/** Validate a version selector without accessing or changing repository state.
 */
int wpm_repo_version_is_valid(const char *version);

/** Enable or disable detailed repository diagnostics. */
void wpm_repo_set_verbose(int enabled);
/** Create a fresh schema-1 filesystem repository without replacing an index.
 * A dry run disables mutation and previews the target layout. */
int wpm_repo_init(const char *directory, int dry_run);
/** Verify one filesystem catalog and its archives using existing trust only.
 * Performs no managed writes, web retrieval, extraction, or scripts. */
int wpm_repo_verify(const char *directory, int allow_unsigned);
/** Require a catalog signature by the explicitly selected index public key.
 * This authority is separate from the package trust store; no writes/prompts.
 */
int wpm_repo_verify_index(const char *directory, int allow_unsigned,
                          const char *public_key);
/** Sign an unchanged catalog with an explicit private/public index-key pair.
 * Dry-run reads public selection only; existing signatures need replacement
 * authorization. Legacy consumers still authenticate packages separately. */
int wpm_repo_sign(const char *directory, const char *private_key,
                  const char *public_key, int replace, int allow_unsigned,
                  int dry_run);
/** Append a validated archive to a filesystem catalog without overwriting
 * package identities. Dry-run performs no managed writes. */
int wpm_repo_add_package(const char *directory, const char *archive,
                         int allow_unsigned, int dry_run);
/** Add or reprioritize a supported repository locator. */
int wpm_repo_add(const char *url, int priority, int allow_insecure_http);
/** List configured repositories in resolution order. */
int wpm_repo_list(void);
/** List retained installed-package metadata without writes or extraction.
 * Optional identity selectors are exact; this does not verify trust/deployment.
 */
int wpm_repo_list_installed(const char *name, const char *arch,
                            const char *version);
/** List local/cached linked-catalog metadata without writes or web retrieval.
 * Reports prerelease policy; does not fetch or validate package archives. */
int wpm_repo_list_available(const char *name, const char *arch,
                            const char *version);
/** Narrow catalog rows to a displayed ordinal (1..32); zero selects all.
 * Health and graph bounds still apply to the entire inspected graph. */
int wpm_repo_list_available_from(const char *name, const char *arch,
                                 const char *version, unsigned ordinal);
/** Show all matching retained/catalog metadata without selecting or mutating.
 * Returns nonzero only for complete metadata with at least one match. */
int wpm_repo_show(const char *name, const char *arch, const char *version);
/** Narrow the catalog portion of show; retained metadata remains visible. */
int wpm_repo_show_from(const char *name, const char *arch, const char *version,
                       unsigned ordinal);
/** Render shared retained/catalog queries as wpm.package-query.v1 JSON.
 * Available selects catalog-only list; show combines both metadata views. */
int wpm_repo_query_json(const char *name, const char *arch, const char *version,
                        unsigned ordinal, int available, int show);
/** Remove a configured repository. */
int wpm_repo_remove(const char *url);
/** Inspect configured catalog health without managed writes or web retrieval.
 * Returns nonzero only when all configured catalogs are usable and fresh. */
int wpm_repo_diagnose(void);
/** Inspect configured and linked catalog health without managed writes or web
 * retrieval.
 */
int wpm_repo_diagnose_links(void);
/** Emit a versioned JSON catalog-health report without locators or writes. */
int wpm_repo_diagnose_json(int follow_links);
/** Refresh repository indexes, optionally using cached data only. */
int wpm_repo_update(int offline);
/** Resolve and install one package using the requested selectors. */
int wpm_repo_install(const char *package_name, const char *arch,
                     const char *version, int offline, int allow_unsigned);
/** Resolve and upgrade selected or all installed package identities. */
int wpm_repo_upgrade(const char **package_names, int package_count, int all,
                     const char *arch, const char *version, int offline,
                     int allow_unsigned, int assume_yes);
/** Set the global or package-specific prerelease policy. */
int wpm_config_prerelease_set(const char *package_name, int enabled);
/** Print the effective global or package-specific prerelease policy. */
int wpm_config_prerelease_get(const char *package_name);
/** Remove a package-specific prerelease policy override. */
int wpm_config_prerelease_unset(const char *package_name);

#endif
