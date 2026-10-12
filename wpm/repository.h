/** @file repository.h @brief Public package repository interface. */
#ifndef WPM_REPOSITORY_H
#define WPM_REPOSITORY_H

/** Validate a version selector without accessing or changing repository state.
 */
int wpm_repo_version_is_valid(const char *version);

/** Enable or disable detailed repository diagnostics. */
void wpm_repo_set_verbose(int enabled);
/** Add or reprioritize a supported repository locator. */
int wpm_repo_add(const char *url, int priority, int allow_insecure_http);
/** List configured repositories in resolution order. */
int wpm_repo_list(void);
/** Remove a configured repository. */
int wpm_repo_remove(const char *url);
/** Inspect configured catalog health without writes or network access.
 * Returns nonzero only when all configured catalogs are usable and fresh. */
int wpm_repo_diagnose(void);
/** Inspect configured and linked catalog health without writes or networking.
 */
int wpm_repo_diagnose_links(void);
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
