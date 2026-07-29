/** @file init.h @brief Package project initialization interface. */
#ifndef WPM_INIT_H
#define WPM_INIT_H

#include <stddef.h>

/**
 * Read the final component of the current working directory.
 * @param[out] name Destination buffer.
 * @param[in] size Capacity of @p name in bytes.
 * @return Nonzero on success; zero if the path cannot be read or stored.
 */
int wpm_current_directory_name(char* name, size_t size);

/**
 * Validate a package name for use in WPM metadata and archive names.
 * @param[in] name Candidate package name.
 * @return Nonzero when valid; otherwise zero.
 */
int wpm_package_name_is_valid(const char* name);

/**
 * Initialize package-support files in the current directory.
 * @param[in] name Valid package name.
 * @return Nonzero on success; zero after reporting a filesystem error.
 */
int wpm_init_run(const char* name);

#endif
