// wpm.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <stdio.h>
#include <string.h>

#define VERSION "1.0.0"

typedef enum {
    CMD_UNKNOWN,
    CMD_INIT,
    CMD_BUILD,
    CMD_INSTALL,
    CMD_REMOVE,
    CMD_UPDATE,
    CMD_UPGRADE
} Command;

Command parse_command(const char* cmd);

void print_version();
void print_usage(Command c);