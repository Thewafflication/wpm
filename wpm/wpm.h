// wpm.h : Include file for standard system include files,
// or project specific include files.

/** @file wpm.h @brief Command-line types and presentation functions. */
#pragma once

#include <stdio.h>
#include <string.h>

#include "version.h"

/** Commands accepted by the WPM command-line parser. */
typedef enum {
    CMD_UNKNOWN, /**< Unrecognized command text. */
    CMD_INIT,    /**< Initialize package sources. */
    CMD_BUILD,   /**< Build a package archive. */
    CMD_VERIFY,  /**< Verify a package archive. */
    CMD_INSTALL, /**< Install a package. */
    CMD_REMOVE,  /**< Remove a package. */
    CMD_REPO,    /**< Manage repositories. */
    CMD_TRUST,   /**< Manage trusted signing keys. */
    CMD_KEY,     /**< Manage signing-key configuration. */
    CMD_CONFIG,  /**< Manage WPM configuration. */
    CMD_UPDATE,  /**< Refresh repository metadata. */
    CMD_UPGRADE  /**< Upgrade installed packages. */
} Command;

/**
 * Map command text to its command identifier.
 * @param[in] cmd Command text.
 * @return The matching command, or CMD_UNKNOWN.
 */
Command parse_command(const char* cmd);

/** Print WPM and dependency version information to standard output. */
void print_version();

/**
 * Print general or command-specific usage information.
 * @param[in] c Command whose usage should be shown, or CMD_UNKNOWN.
 */
void print_usage(Command c);
