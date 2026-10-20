/*
 * parser.c - tokenize input + match verbs, targets, modifiers
 *
 * The idea is simple: split user input into words, check if the first
 * word is a known verb (with synonyms), then scan the rest for target
 * types and modifiers. If first word matches a known CLI tool (like
 * "git", "ls", "sudo"), we skip parsing and pass it through as raw.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "parser.h"

// --- verb synonym table ---
// maps words to their canonical verb. "make" -> CREATE, "rm" -> DELETE, etc.

typedef struct {
    const char  *word;
    CommandVerb  verb;
} VerbEntry;

static const VerbEntry verb_table[] = {
    // filesystem
    {"create", VERB_CREATE}, {"make", VERB_CREATE}, {"new", VERB_CREATE},
    {"add", VERB_CREATE}, {"touch", VERB_CREATE},

    {"delete", VERB_DELETE}, {"remove", VERB_DELETE}, {"rm", VERB_DELETE},
    {"del", VERB_DELETE}, {"erase", VERB_DELETE}, {"destroy", VERB_DELETE},

    {"show", VERB_SHOW}, {"list", VERB_SHOW}, {"display", VERB_SHOW},
    {"view", VERB_SHOW}, {"see", VERB_SHOW}, {"print", VERB_SHOW},

    {"go", VERB_GO}, {"navigate", VERB_GO}, {"enter", VERB_GO}, {"open", VERB_GO},

    {"rename", VERB_RENAME},
    {"move", VERB_MOVE}, {"mv", VERB_MOVE}, {"transfer", VERB_MOVE}, {"relocate", VERB_MOVE},
    {"copy", VERB_COPY}, {"cp", VERB_COPY}, {"duplicate", VERB_COPY},
    {"find", VERB_FIND}, {"search", VERB_SEARCH}, {"locate", VERB_FIND},
    {"edit", VERB_EDIT},

    // packages
    {"install", VERB_INSTALL}, {"setup", VERB_INSTALL}, {"get", VERB_INSTALL},
    {"download", VERB_DOWNLOAD},
    {"uninstall", VERB_UNINSTALL},
    {"update", VERB_UPDATE}, {"upgrade", VERB_UPGRADE}, {"refresh", VERB_UPDATE},

    // execution
    {"run", VERB_RUN}, {"execute", VERB_RUN}, {"start", VERB_START}, {"launch", VERB_RUN},
    {"check", VERB_CHECK}, {"verify", VERB_CHECK},

    // git
    {"stage", VERB_STAGE}, {"unstage", VERB_UNSTAGE}, {"commit", VERB_COMMIT},
    {"push", VERB_PUSH}, {"pull", VERB_PULL}, {"switch", VERB_SWITCH},
    {"merge", VERB_MERGE}, {"clone", VERB_CLONE}, {"save", VERB_SAVE},
    {"stash", VERB_STASH}, {"pop", VERB_POP},
    {"undo", VERB_UNDO}, {"discard", VERB_DISCARD},

    // system
    {"clear", VERB_CLEAR}, {"cls", VERB_CLEAR}, {"clean", VERB_CLEAR},
    {"kill", VERB_KILL},
    {"shutdown", VERB_SHUTDOWN}, {"restart", VERB_RESTART}, {"reboot", VERB_RESTART},

    // network
    {"ping", VERB_PING},

    // python
    {"activate", VERB_ACTIVATE}, {"deactivate", VERB_DEACTIVATE},

    // archive
    {"compress", VERB_COMPRESS}, {"extract", VERB_EXTRACT}, {"unzip", VERB_EXTRACT},

    // misc
    {"compare", VERB_COMPARE}, {"sort", VERB_SORT}, {"count", VERB_COUNT},
    {"calculate", VERB_CALCULATE}, {"calc", VERB_CALCULATE},

    // meta
    {"help", VERB_HELP}, {"history", VERB_HISTORY},
    {"exit", VERB_EXIT}, {"quit", VERB_EXIT}, {"bye", VERB_EXIT},
    {"version", VERB_VERSION},

    {NULL, VERB_UNKNOWN}
};

// --- target types ---

typedef struct {
    const char *word;
    TargetType  type;
} TargetEntry;

static const TargetEntry target_table[] = {
    {"file", TGT_FILE}, {"folder", TGT_FOLDER}, {"directory", TGT_FOLDER},
    {"dir", TGT_FOLDER}, {"files", TGT_FILES}, {"folders", TGT_FOLDERS},
    {"directories", TGT_FOLDERS},
    {"package", TGT_PACKAGE}, {"packages", TGT_PACKAGES},
    {"version", TGT_VERSION},
    {"branch", TGT_BRANCH}, {"branches", TGT_BRANCH},
    {"environment", TGT_ENVIRONMENT}, {"env", TGT_ENVIRONMENT}, {"venv", TGT_ENVIRONMENT},
    {"process", TGT_PROCESS}, {"processes", TGT_PROCESSES},
    {"repo", TGT_REPO}, {"repository", TGT_REPO},
    {"changes", TGT_CHANGES}, {"status", TGT_STATUS},
    {"remote", TGT_REMOTE}, {"stash", TGT_STASH},
    {"internet", TGT_INTERNET}, {"ip", TGT_IP},
    {"port", TGT_PORT}, {"ports", TGT_PORTS},
    {"system", TGT_SYSTEM}, {"screen", TGT_SCREEN},
    {"tree", TGT_TREE}, {"calendar", TGT_CALENDAR},
    {"date", TGT_DATE}, {"uptime", TGT_UPTIME},
    {"users", TGT_USERS}, {"hostname", TGT_HOSTNAME},
    {"memory", TGT_MEMORY}, {"ram", TGT_MEMORY},
    {"cpu", TGT_CPU}, {"disk", TGT_DISK},
    {"path", TGT_PATH}, {"log", TGT_LOG}, {"diff", TGT_DIFF},
    {"permissions", TGT_PERMISSIONS}, {"executable", TGT_EXECUTABLE},
    {"owner", TGT_OWNER}, {"lines", TGT_LINES},
    {NULL, TGT_NONE}
};

// --- modifiers ---

typedef struct {
    const char *word;
    Modifier    mod;
} ModEntry;

static const ModEntry mod_table[] = {
    {"hidden", MOD_HIDDEN}, {"all", MOD_ALL}, {"current", MOD_CURRENT},
    {"detailed", MOD_DETAILED}, {"detail", MOD_DETAILED},
    {"recursive", MOD_RECURSIVE}, {"force", MOD_FORCE},
    {"global", MOD_GLOBAL}, {"globally", MOD_GLOBAL},
    {"system", MOD_SYSTEM},
    {"head", MOD_HEAD}, {"tail", MOD_TAIL},
    {"interactive", MOD_INTERACTIVE},
    {"pip", MOD_PIP}, {"npm", MOD_NPM}, {"public", MOD_PUBLIC},
    {NULL, MOD_NONE}
};

/*
 * Known CLI tools - if the user types one of these as the first word,
 * we just pass the whole line through to the shell instead of parsing it.
 */
static const char *raw_commands[] = {
    // core shell
    "ls", "cd", "pwd", "cat", "head", "tail", "less", "more",
    "grep", "awk", "sed", "cut", "tr", "wc", "sort", "uniq",
    "chmod", "chown", "chgrp", "ln", "stat", "file",
    "echo", "export", "source", "alias", "unalias",
    "xargs", "tee", "watch",
    // file ops
    "mkdir", "rmdir", "touch", "cp", "mv", "rm",
    "tar", "zip", "gzip", "bzip2", "diff", "patch",
    // system
    "sudo", "su", "whoami", "who", "id",
    "ps", "top", "htop", "kill", "killall", "pkill",
    "df", "du", "free", "uname", "hostname", "uptime",
    "date", "cal", "bc",
    "systemctl", "journalctl", "service",
    "shutdown", "reboot", "halt",
    "lsblk", "mount", "umount", "fdisk",
    "dmesg", "lspci", "lsusb",
    // package managers
    "apt", "apt-get", "dpkg", "dnf", "yum", "rpm",
    "pacman", "yay", "paru", "snap", "flatpak", "brew",
    // dev tools
    "git", "svn",
    "python", "python3", "pip", "pip3",
    "node", "npm", "npx", "yarn", "pnpm",
    "gcc", "g++", "cc", "make", "cmake",
    "cargo", "rustc", "java", "javac", "mvn", "gradle",
    "go", "ruby", "perl", "php",
    "gdb", "valgrind", "strace", "ltrace",
    // editors
    "nano", "vim", "vi", "emacs", "code",
    // network
    "curl", "wget", "ssh", "scp", "sftp", "rsync",
    "netstat", "ss", "ip", "ifconfig",
    "ping", "traceroute", "nslookup", "dig",
    "nmap", "nc", "telnet",
    // containers
    "docker", "docker-compose", "podman", "kubectl", "helm",
    // misc
    "man", "info", "whatis", "whereis", "which", "type",
    "history", "clear", "xdg-open", "xclip",
    NULL
};

// multi-word modifiers like "sorted by size"
typedef struct {
    const char *word1;
    const char *word2;
    const char *word3;
    Modifier    mod;
} MultiWordMod;

static const MultiWordMod multi_mods[] = {
    {"sorted", "by", "size", MOD_SORTED_SIZE},
    {"sorted", "by", "time", MOD_SORTED_TIME},
    {NULL, NULL, NULL, MOD_NONE}
};

// case-insensitive compare
static int str_eq(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

// lookup helpers
static CommandVerb lookup_verb(const char *word) {
    for (int i = 0; verb_table[i].word != NULL; i++)
        if (str_eq(word, verb_table[i].word))
            return verb_table[i].verb;
    return VERB_UNKNOWN;
}

static TargetType lookup_target(const char *word) {
    for (int i = 0; target_table[i].word != NULL; i++)
        if (str_eq(word, target_table[i].word))
            return target_table[i].type;
    return TGT_NONE;
}

static Modifier lookup_modifier(const char *word) {
    for (int i = 0; mod_table[i].word != NULL; i++)
        if (str_eq(word, mod_table[i].word))
            return mod_table[i].mod;
    return MOD_NONE;
}

static int is_raw_command(const char *word) {
    for (int i = 0; raw_commands[i] != NULL; i++)
        if (str_eq(word, raw_commands[i]))
            return 1;
    return 0;
}

static int is_preposition(const char *word) {
    return str_eq(word, "to") || str_eq(word, "into") ||
           str_eq(word, "in") || str_eq(word, "from") ||
           str_eq(word, "and") || str_eq(word, "with") ||
           str_eq(word, "as");
}

// split input into array of words
static int tokenize(const char *input, char tokens[][MAX_NAME_LEN], int max_tokens) {
    int count = 0;
    const char *p = input;

    while (*p && count < max_tokens) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        int i = 0;
        while (*p && !isspace((unsigned char)*p) && i < MAX_NAME_LEN - 1)
            tokens[count][i++] = *p++;
        tokens[count][i] = '\0';
        count++;
    }

    return count;
}

void parser_init(void) {
    // nothing for now - might load custom mappings from config later
}

/*
 * Parse user input into a ParsedCommand.
 *
 * Strategy:
 *  1. tokenize
 *  2. check if first word is a raw CLI tool -> pass through
 *  3. look up verb
 *  4. scan remaining words for modifiers + target types
 *  5. leftover words become target_name (before "to") or destination (after "to")
 */
int parser_parse(const char *input, ParsedCommand *cmd) {
    memset(cmd, 0, sizeof(ParsedCommand));
    cmd->verb = VERB_UNKNOWN;
    cmd->modifier = MOD_NONE;
    cmd->target_type = TGT_NONE;
    cmd->is_raw = 0;

    strncpy(cmd->raw_input, input, MAX_INPUT_LEN - 1);
    cmd->raw_input[MAX_INPUT_LEN - 1] = '\0';

    // trim whitespace
    char trimmed[MAX_INPUT_LEN];
    strncpy(trimmed, input, MAX_INPUT_LEN - 1);
    trimmed[MAX_INPUT_LEN - 1] = '\0';

    char *start = trimmed;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

    if (strlen(start) == 0) {
        cmd->verb = VERB_UNKNOWN;
        return -1;
    }

    char tokens[MAX_TOKENS][MAX_NAME_LEN];
    int ntokens = tokenize(start, tokens, MAX_TOKENS);
    if (ntokens == 0) return -1;

    // try verb first
    CommandVerb verb = lookup_verb(tokens[0]);

    if (verb != VERB_UNKNOWN) {
        cmd->verb = verb;
    } else if (is_raw_command(tokens[0])) {
        cmd->verb = VERB_RAW;
        cmd->is_raw = 1;
        strncpy(cmd->target_name, start, MAX_NAME_LEN - 1);
        return 0;
    } else {
        cmd->verb = VERB_UNKNOWN;
        return -1;
    }

    // track which tokens we've already handled
    int consumed[MAX_TOKENS];
    memset(consumed, 0, sizeof(consumed));
    consumed[0] = 1;

    int to_index = -1;

    // check for multi-word modifiers first ("sorted by size")
    for (int i = 1; i < ntokens - 1; i++) {
        for (int m = 0; multi_mods[m].word1 != NULL; m++) {
            if (str_eq(tokens[i], multi_mods[m].word1) &&
                i + 1 < ntokens && str_eq(tokens[i + 1], multi_mods[m].word2)) {
                if (multi_mods[m].word3 != NULL && i + 2 < ntokens &&
                    str_eq(tokens[i + 2], multi_mods[m].word3)) {
                    cmd->modifier = multi_mods[m].mod;
                    consumed[i] = consumed[i + 1] = consumed[i + 2] = 1;
                } else if (multi_mods[m].word3 == NULL) {
                    cmd->modifier = multi_mods[m].mod;
                    consumed[i] = consumed[i + 1] = 1;
                }
            }
        }
    }

    // scan for single-word modifiers and targets
    for (int i = 1; i < ntokens; i++) {
        if (consumed[i]) continue;

        // "to" preposition for rename/move/copy/go
        if (is_preposition(tokens[i]) &&
            (cmd->verb == VERB_RENAME || cmd->verb == VERB_MOVE ||
             cmd->verb == VERB_COPY || cmd->verb == VERB_GO ||
             cmd->verb == VERB_COMPARE)) {
            to_index = i;
            consumed[i] = 1;
            continue;
        }

        if (cmd->modifier == MOD_NONE) {
            Modifier m = lookup_modifier(tokens[i]);
            if (m != MOD_NONE) {
                cmd->modifier = m;
                consumed[i] = 1;
                continue;
            }
        }

        if (cmd->target_type == TGT_NONE) {
            TargetType t = lookup_target(tokens[i]);
            if (t != TGT_NONE) {
                cmd->target_type = t;
                consumed[i] = 1;
                continue;
            }
        }
    }

    // collect leftover tokens into target_name / destination
    char name_buf[MAX_NAME_LEN] = "";
    char dest_buf[MAX_NAME_LEN] = "";

    for (int i = 1; i < ntokens; i++) {
        if (consumed[i]) continue;

        if (to_index >= 0 && i > to_index) {
            if (strlen(dest_buf) > 0) strncat(dest_buf, " ", MAX_NAME_LEN - strlen(dest_buf) - 1);
            strncat(dest_buf, tokens[i], MAX_NAME_LEN - strlen(dest_buf) - 1);
        } else {
            if (strlen(name_buf) > 0) strncat(name_buf, " ", MAX_NAME_LEN - strlen(name_buf) - 1);
            strncat(name_buf, tokens[i], MAX_NAME_LEN - strlen(name_buf) - 1);
        }
    }

    strncpy(cmd->target_name, name_buf, MAX_NAME_LEN - 1);
    strncpy(cmd->destination, dest_buf, MAX_NAME_LEN - 1);

    return 0;
}

// --- string representations (for debug/logging) ---

const char *verb_to_string(CommandVerb verb) {
    switch (verb) {
        case VERB_CREATE:     return "CREATE";
        case VERB_DELETE:     return "DELETE";
        case VERB_SHOW:       return "SHOW";
        case VERB_GO:         return "GO";
        case VERB_INSTALL:    return "INSTALL";
        case VERB_UNINSTALL:  return "UNINSTALL";
        case VERB_RUN:        return "RUN";
        case VERB_CHECK:      return "CHECK";
        case VERB_RENAME:     return "RENAME";
        case VERB_MOVE:       return "MOVE";
        case VERB_COPY:       return "COPY";
        case VERB_FIND:       return "FIND";
        case VERB_UPDATE:     return "UPDATE";
        case VERB_UPGRADE:    return "UPGRADE";
        case VERB_CLEAR:      return "CLEAR";
        case VERB_EDIT:       return "EDIT";
        case VERB_MAKE:       return "MAKE";
        case VERB_STAGE:      return "STAGE";
        case VERB_UNSTAGE:    return "UNSTAGE";
        case VERB_COMMIT:     return "COMMIT";
        case VERB_PUSH:       return "PUSH";
        case VERB_PULL:       return "PULL";
        case VERB_SWITCH:     return "SWITCH";
        case VERB_MERGE:      return "MERGE";
        case VERB_CLONE:      return "CLONE";
        case VERB_COMPRESS:   return "COMPRESS";
        case VERB_EXTRACT:    return "EXTRACT";
        case VERB_COMPARE:    return "COMPARE";
        case VERB_DOWNLOAD:   return "DOWNLOAD";
        case VERB_PING:       return "PING";
        case VERB_KILL:       return "KILL";
        case VERB_START:      return "START";
        case VERB_ACTIVATE:   return "ACTIVATE";
        case VERB_DEACTIVATE: return "DEACTIVATE";
        case VERB_SAVE:       return "SAVE";
        case VERB_STASH:      return "STASH";
        case VERB_POP:        return "POP";
        case VERB_SORT:       return "SORT";
        case VERB_COUNT:      return "COUNT";
        case VERB_CALCULATE:  return "CALCULATE";
        case VERB_UNDO:       return "UNDO";
        case VERB_DISCARD:    return "DISCARD";
        case VERB_SEARCH:     return "SEARCH";
        case VERB_SHUTDOWN:   return "SHUTDOWN";
        case VERB_RESTART:    return "RESTART";
        case VERB_HELP:       return "HELP";
        case VERB_HISTORY:    return "HISTORY";
        case VERB_EXIT:       return "EXIT";
        case VERB_VERSION:    return "VERSION";
        case VERB_RAW:        return "RAW (pass-through)";
        case VERB_UNKNOWN:    return "UNKNOWN";
        default:              return "???";
    }
}

const char *target_to_string(TargetType target) {
    switch (target) {
        case TGT_NONE:        return "(none)";
        case TGT_FILE:        return "FILE";
        case TGT_FOLDER:      return "FOLDER";
        case TGT_FILES:       return "FILES";
        case TGT_FOLDERS:     return "FOLDERS";
        case TGT_PACKAGE:     return "PACKAGE";
        case TGT_VERSION:     return "VERSION";
        case TGT_BRANCH:      return "BRANCH";
        case TGT_ENVIRONMENT: return "ENVIRONMENT";
        case TGT_PROCESS:     return "PROCESS";
        case TGT_REPO:        return "REPO";
        case TGT_CHANGES:     return "CHANGES";
        case TGT_STATUS:      return "STATUS";
        case TGT_REMOTE:      return "REMOTE";
        case TGT_STASH:       return "STASH";
        case TGT_INTERNET:    return "INTERNET";
        case TGT_IP:          return "IP";
        case TGT_PORT:        return "PORT";
        case TGT_PORTS:       return "PORTS";
        case TGT_SYSTEM:      return "SYSTEM";
        case TGT_PACKAGES:    return "PACKAGES";
        case TGT_SCREEN:      return "SCREEN";
        case TGT_TREE:        return "TREE";
        case TGT_CALENDAR:    return "CALENDAR";
        case TGT_DATE:        return "DATE";
        case TGT_UPTIME:      return "UPTIME";
        case TGT_USERS:       return "USERS";
        case TGT_HOSTNAME:    return "HOSTNAME";
        case TGT_MEMORY:      return "MEMORY";
        case TGT_CPU:         return "CPU";
        case TGT_DISK:        return "DISK";
        case TGT_PROCESSES:   return "PROCESSES";
        case TGT_PATH:        return "PATH";
        case TGT_LOG:         return "LOG";
        case TGT_DIFF:        return "DIFF";
        case TGT_PERMISSIONS: return "PERMISSIONS";
        case TGT_EXECUTABLE:  return "EXECUTABLE";
        case TGT_OWNER:       return "OWNER";
        case TGT_LINES:       return "LINES";
        default:              return "???";
    }
}

const char *modifier_to_string(Modifier mod) {
    switch (mod) {
        case MOD_NONE:        return "(none)";
        case MOD_HIDDEN:      return "HIDDEN";
        case MOD_ALL:         return "ALL";
        case MOD_CURRENT:     return "CURRENT";
        case MOD_DETAILED:    return "DETAILED";
        case MOD_RECURSIVE:   return "RECURSIVE";
        case MOD_FORCE:       return "FORCE";
        case MOD_GLOBAL:      return "GLOBAL";
        case MOD_SYSTEM:      return "SYSTEM";
        case MOD_HEAD:        return "HEAD";
        case MOD_TAIL:        return "TAIL";
        case MOD_SORTED_SIZE: return "SORTED_BY_SIZE";
        case MOD_SORTED_TIME: return "SORTED_BY_TIME";
        case MOD_PIP:         return "PIP";
        case MOD_NPM:         return "NPM";
        case MOD_INTERACTIVE: return "INTERACTIVE";
        case MOD_PUBLIC:      return "PUBLIC";
        default:              return "???";
    }
}
