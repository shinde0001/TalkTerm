/*
 * history.c - persistent command history
 * Saves to ~/.talkterm_history using readline's built-in stuff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "history.h"

#define HISTORY_FILE ".talkterm_history"
#define MAX_HISTORY  1000

static char history_path[512] = "";

static void build_history_path(void) {
    const char *home = getenv("HOME");
    if (home)
        snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
}

void history_init(void) {
    build_history_path();
    stifle_history(MAX_HISTORY);

    if (strlen(history_path) > 0)
        read_history(history_path);
}

void history_save_session(void) {
    if (strlen(history_path) > 0) {
        write_history(history_path);
        history_truncate_file(history_path, MAX_HISTORY);
    }
}

void history_cleanup(void) {
    history_save_session();
    clear_history();
}
