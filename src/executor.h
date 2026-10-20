/*
 * executor.h - command execution interface
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

// takes a parsed command and runs the real shell command
int executor_run(const ParsedCommand *cmd);

#endif
