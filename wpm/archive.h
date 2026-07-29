/** @file archive.h @brief Public package archive operations. */
#ifndef WPM_ARCHIVE_H
#define WPM_ARCHIVE_H

#include <stddef.h>

/** Metadata read from a WPM package archive. */
typedef struct wpm_package_info {
    char name[128];         /**< Package identity. */
    char version[64];       /**< Semantic package version. */
    char arch[16];          /**< Target architecture. */
    char archive_name[4096]; /**< Original archive filename. */
} wpm_package_info;

/**
 * Resolve the active WPM data root.
 * @param[out] result Destination buffer.
 * @param[in] result_size Capacity of @p result in bytes.
 * @return Nonzero on success; zero if the path cannot be resolved or stored.
 */
int wpm_get_data_root(char* result, size_t result_size);

/**
 * Create the data, temporary, package, cache, and configuration directories.
 * @return Nonzero on success; zero after reporting a filesystem error.
 */
int wpm_initialize_data_directories(void);

/**
 * Enable or disable detailed archive diagnostics.
 * @param[in] enabled Nonzero to enable verbose output.
 */
void wpm_set_verbose(int enabled);

/**
 * Set progress counters used by subsequent archive operations.
 * @param[in] current One-based current operation index.
 * @param[in] total Total operation count, or zero when unknown.
 */
void wpm_archive_set_progress(int current, int total);

/**
 * Build a package archive from a source directory.
 * @param[in] source_dir Package source directory.
 * @param[in] output_dir Destination directory.
 * @param[in] update_index Nonzero to refresh the source package index.
 * @param[in] private_key Optional signing-key path; may be NULL.
 * @return Nonzero on success; zero after reporting a validation or I/O error.
 */
int wpm_archive_build(const char* source_dir, const char* output_dir, int update_index, const char* private_key);

/**
 * Extract a package archive into a destination directory.
 * @param[in] archive_path Archive to extract.
 * @param[in] destination_dir Destination directory.
 * @return Nonzero on success; zero for an unsafe archive or filesystem error.
 */
int wpm_archive_extract(const char* archive_path, const char* destination_dir);

/**
 * Validate an archive without installing it.
 * @param[in] archive_path Archive to validate.
 * @return Nonzero when valid; zero after reporting the validation failure.
 */
int wpm_archive_verify(const char* archive_path);

/**
 * Validate and install an archive.
 * @param[in] archive_path Archive to install.
 * @param[in] allow_unsigned Nonzero to permit the unsigned-package flow.
 * @return Nonzero on success; zero after reporting the failure.
 */
int wpm_archive_install(const char* archive_path, int allow_unsigned);

/**
 * Read package identity metadata without installing the archive.
 * @param[in] archive_path Archive to inspect.
 * @param[out] info Populated package information.
 * @return Nonzero on success; zero if metadata is missing or invalid.
 */
int wpm_archive_inspect(const char* archive_path, wpm_package_info* info);

/**
 * Upgrade an installed package from a validated candidate archive.
 * @param[in] archive_path Candidate archive.
 * @param[in] allow_unsigned Nonzero to permit the unsigned-package flow.
 * @param[in] expected_name Required package name.
 * @param[in] expected_version Required new version.
 * @param[in] expected_arch Required architecture.
 * @param[in] old_version Currently installed version.
 * @return Nonzero on success; zero after reporting or auditing the failure.
 */
int wpm_archive_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_name, const char* expected_version,
    const char* expected_arch, const char* old_version);

/**
 * Cache and launch the handoff used to upgrade WPM itself.
 * @param[in] archive_path Candidate archive.
 * @param[in] allow_unsigned Nonzero to permit the unsigned-package flow.
 * @param[in] expected_version Required new WPM version.
 * @param[in] expected_arch Required architecture.
 * @param[in] old_version Currently installed WPM version.
 * @return Nonzero when scheduled; zero after reporting the failure.
 */
int wpm_archive_schedule_self_upgrade(const char* archive_path, int allow_unsigned,
    const char* expected_version, const char* expected_arch,
    const char* old_version);

/**
 * Remove an installed package using its retained archive metadata.
 * @param[in] package_name Stored archive name.
 * @return Nonzero on success; zero after reporting the failure.
 */
int wpm_archive_remove(const char* package_name);

#endif
