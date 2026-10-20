/*
 * colors.h - ANSI escape codes for terminal colors
 */

#ifndef COLORS_H
#define COLORS_H

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_ITALIC  "\033[3m"
#define C_UNDER   "\033[4m"

#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"
#define C_GRAY    "\033[90m"

#define C_BRED    "\033[91m"
#define C_BGREEN  "\033[92m"
#define C_BYELLOW "\033[93m"
#define C_BBLUE   "\033[94m"
#define C_BMAGENTA "\033[95m"
#define C_BCYAN   "\033[96m"
#define C_BWHITE  "\033[97m"

// quick helpers for common output patterns
#define PRINT_SUCCESS(msg) printf(C_GREEN C_BOLD "  ✓ " C_RESET C_GREEN msg C_RESET "\n")
#define PRINT_ERROR(msg)   printf(C_RED C_BOLD "  ✗ " C_RESET C_RED msg C_RESET "\n")
#define PRINT_WARN(msg)    printf(C_YELLOW C_BOLD "  ⚠ " C_RESET C_YELLOW msg C_RESET "\n")
#define PRINT_INFO(msg)    printf(C_CYAN C_BOLD "  ℹ " C_RESET C_CYAN msg C_RESET "\n")

#endif
