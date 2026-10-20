/*
 * parser.h - command parsing types and API
 */

#ifndef PARSER_H
#define PARSER_H

#define MAX_NAME_LEN   512
#define MAX_INPUT_LEN  1024
#define MAX_TOKENS     64

typedef enum {
    VERB_CREATE, VERB_DELETE, VERB_SHOW, VERB_GO,
    VERB_INSTALL, VERB_UNINSTALL, VERB_RUN, VERB_CHECK,
    VERB_RENAME, VERB_MOVE, VERB_COPY, VERB_FIND,
    VERB_UPDATE, VERB_UPGRADE, VERB_CLEAR, VERB_EDIT,
    VERB_MAKE,
    VERB_STAGE, VERB_UNSTAGE, VERB_COMMIT,
    VERB_PUSH, VERB_PULL, VERB_SWITCH, VERB_MERGE, VERB_CLONE,
    VERB_COMPRESS, VERB_EXTRACT, VERB_COMPARE, VERB_DOWNLOAD,
    VERB_PING, VERB_KILL, VERB_START,
    VERB_ACTIVATE, VERB_DEACTIVATE,
    VERB_SAVE, VERB_STASH, VERB_POP,
    VERB_SORT, VERB_COUNT, VERB_CALCULATE,
    VERB_UNDO, VERB_DISCARD, VERB_SEARCH,
    VERB_SHUTDOWN, VERB_RESTART,
    VERB_HELP, VERB_HISTORY, VERB_EXIT, VERB_VERSION,
    VERB_RAW,       // pass-through to shell
    VERB_UNKNOWN
} CommandVerb;

typedef enum {
    TGT_NONE,
    TGT_FILE, TGT_FOLDER, TGT_FILES, TGT_FOLDERS,
    TGT_PACKAGE, TGT_VERSION, TGT_BRANCH, TGT_ENVIRONMENT,
    TGT_PROCESS, TGT_REPO, TGT_CHANGES, TGT_STATUS,
    TGT_REMOTE, TGT_STASH, TGT_INTERNET, TGT_IP,
    TGT_PORT, TGT_PORTS, TGT_SYSTEM, TGT_PACKAGES,
    TGT_SCREEN, TGT_TREE, TGT_CALENDAR, TGT_DATE,
    TGT_UPTIME, TGT_USERS, TGT_HOSTNAME,
    TGT_MEMORY, TGT_CPU, TGT_DISK, TGT_PROCESSES,
    TGT_PATH, TGT_LOG, TGT_DIFF,
    TGT_PERMISSIONS, TGT_EXECUTABLE, TGT_OWNER, TGT_LINES
} TargetType;

typedef enum {
    MOD_NONE,
    MOD_HIDDEN, MOD_ALL, MOD_CURRENT, MOD_DETAILED,
    MOD_RECURSIVE, MOD_FORCE, MOD_GLOBAL, MOD_SYSTEM,
    MOD_HEAD, MOD_TAIL,
    MOD_SORTED_SIZE, MOD_SORTED_TIME,
    MOD_PIP, MOD_NPM, MOD_INTERACTIVE, MOD_PUBLIC
} Modifier;

typedef struct {
    CommandVerb verb;
    Modifier   modifier;
    TargetType target_type;
    char       target_name[MAX_NAME_LEN];
    char       destination[MAX_NAME_LEN];   // for "rename X to Y"
    char       raw_input[MAX_INPUT_LEN];
    int        is_raw;
} ParsedCommand;

void parser_init(void);
int parser_parse(const char *input, ParsedCommand *cmd);
const char *verb_to_string(CommandVerb verb);
const char *target_to_string(TargetType target);
const char *modifier_to_string(Modifier mod);

#endif
