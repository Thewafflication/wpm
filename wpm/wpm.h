// wpm.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <stdio.h>
#include <string.h>

#include "version.h"

typedef enum {
    CMD_UNKNOWN,
    CMD_INIT,
    CMD_BUILD,
    CMD_VERIFY,
    CMD_INSTALL,
    CMD_REMOVE,
    CMD_REPO,
    CMD_TRUST,
    CMD_KEY,
    CMD_CONFIG,
    CMD_UPDATE,
    CMD_UPGRADE
} Command;

Command parse_command(const char* cmd);

void print_version();
void print_usage(Command c);
