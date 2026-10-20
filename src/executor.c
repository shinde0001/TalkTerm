/*
 * executor.c - takes a ParsedCommand and runs the corresponding shell command
 *
 * Each verb gets mapped to one or more real commands. Some commands
 * (like delete) ask for confirmation first.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "executor.h"
#include "colors.h"

static int run_shell(const char *cmd) {
    int ret = system(cmd);
    if (ret == -1) {
        PRINT_ERROR("Failed to execute command.");
        return -1;
    }
    return WEXITSTATUS(ret);
}

// --- filesystem ---

static int exec_create(const ParsedCommand *cmd) {
    char buf[2048];
    if (cmd->target_type == TGT_FOLDER) {
        snprintf(buf, sizeof(buf), "mkdir -p '%s'", cmd->target_name);
        if (run_shell(buf) == 0) { PRINT_SUCCESS("Folder created."); return 0; }
    } else if (cmd->target_type == TGT_FILE) {
        snprintf(buf, sizeof(buf), "touch '%s'", cmd->target_name);
        if (run_shell(buf) == 0) { PRINT_SUCCESS("File created."); return 0; }
    } else if (cmd->target_type == TGT_ENVIRONMENT) {
        snprintf(buf, sizeof(buf), "python3 -m venv '%s'", cmd->target_name);
        if (run_shell(buf) == 0) { PRINT_SUCCESS("Virtual environment created."); return 0; }
    } else if (cmd->target_type == TGT_BRANCH) {
        snprintf(buf, sizeof(buf), "git checkout -b '%s'", cmd->target_name);
        return run_shell(buf);
    } else {
        snprintf(buf, sizeof(buf), "touch '%s'", cmd->target_name);
        if (run_shell(buf) == 0) { PRINT_SUCCESS("Created."); return 0; }
    }
    return -1;
}

static int exec_delete(const ParsedCommand *cmd) {
    char buf[2048];

    // always confirm before deleting
    if (cmd->target_type == TGT_FOLDER) {
        printf(C_YELLOW "  Delete folder '%s'? [y/N] " C_RESET, cmd->target_name);
        int c = getchar(); while (getchar() != '\n');
        if (c != 'y' && c != 'Y') { PRINT_INFO("Cancelled."); return 0; }
        snprintf(buf, sizeof(buf), "rm -rf '%s'", cmd->target_name);
    } else if (cmd->target_type == TGT_BRANCH) {
        snprintf(buf, sizeof(buf), "git branch -d '%s'", cmd->target_name);
        return run_shell(buf);
    } else {
        printf(C_YELLOW "  Delete '%s'? [y/N] " C_RESET, cmd->target_name);
        int c = getchar(); while (getchar() != '\n');
        if (c != 'y' && c != 'Y') { PRINT_INFO("Cancelled."); return 0; }
        snprintf(buf, sizeof(buf), "rm '%s'", cmd->target_name);
    }

    if (run_shell(buf) == 0) { PRINT_SUCCESS("Deleted."); return 0; }
    return -1;
}

static int exec_show(const ParsedCommand *cmd) {
    char buf[2048];

    switch (cmd->target_type) {
        case TGT_FILE:
            if (cmd->modifier == MOD_HEAD)
                snprintf(buf, sizeof(buf), "head '%s'", cmd->target_name);
            else if (cmd->modifier == MOD_TAIL)
                snprintf(buf, sizeof(buf), "tail '%s'", cmd->target_name);
            else
                snprintf(buf, sizeof(buf), "cat '%s'", cmd->target_name);
            return run_shell(buf);

        case TGT_FILES:
        case TGT_FOLDERS:
            if (cmd->modifier == MOD_HIDDEN)       return run_shell("ls -la --color=auto");
            if (cmd->modifier == MOD_DETAILED)     return run_shell("ls -lh --color=auto");
            if (cmd->modifier == MOD_SORTED_SIZE)  return run_shell("ls -lhS --color=auto");
            if (cmd->modifier == MOD_SORTED_TIME)  return run_shell("ls -lht --color=auto");
            return run_shell("ls --color=auto");

        case TGT_TREE:
            return run_shell("tree -C 2>/dev/null || find . -print | head -50");
        case TGT_FOLDER:
            return cmd->modifier == MOD_CURRENT ? run_shell("pwd") : run_shell("ls --color=auto");
        case TGT_SYSTEM:     return run_shell("uname -a");
        case TGT_MEMORY:     return run_shell("free -h");
        case TGT_CPU:        return run_shell("lscpu | head -20");
        case TGT_DISK:       return run_shell("df -h");
        case TGT_PROCESSES:  return run_shell("ps aux --sort=-%mem | head -15");
        case TGT_IP:         return run_shell("hostname -I 2>/dev/null || ip addr show | grep 'inet '");
        case TGT_PORTS:
        case TGT_PORT:       return run_shell("ss -tulnp 2>/dev/null || netstat -tulnp 2>/dev/null");
        case TGT_BRANCH:     return run_shell("git branch -a");
        case TGT_STATUS:     return run_shell("git status");
        case TGT_CHANGES:    return run_shell("git diff");
        case TGT_LOG:        return run_shell("git log --oneline -20");
        case TGT_REMOTE:     return run_shell("git remote -v");
        case TGT_STASH:      return run_shell("git stash list");
        case TGT_CALENDAR:   return run_shell("cal");
        case TGT_DATE:       return run_shell("date");
        case TGT_UPTIME:     return run_shell("uptime");
        case TGT_USERS:      return run_shell("who");
        case TGT_HOSTNAME:   return run_shell("hostname");
        case TGT_PATH:       return run_shell("echo $PATH | tr ':' '\\n'");
        case TGT_DIFF:       return run_shell("git diff");
        case TGT_PACKAGES:   return run_shell("dpkg --list 2>/dev/null | tail -20 || rpm -qa 2>/dev/null | tail -20");
        case TGT_PERMISSIONS:
            snprintf(buf, sizeof(buf), "stat '%s'", cmd->target_name);
            return run_shell(buf);
        case TGT_INTERNET:
            printf(C_DIM "  Checking connectivity..." C_RESET "\n");
            return run_shell("ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1 && echo '  Connected' || echo '  No connection'");
        case TGT_VERSION:
            if (strlen(cmd->target_name) > 0) {
                snprintf(buf, sizeof(buf), "%s --version 2>/dev/null || %s -V 2>/dev/null || %s -v 2>/dev/null",
                    cmd->target_name, cmd->target_name, cmd->target_name);
                return run_shell(buf);
            }
            return run_shell("uname -r");
        default:
            // no target type - try showing as file, or just ls
            if (strlen(cmd->target_name) > 0) {
                snprintf(buf, sizeof(buf), "cat '%s'", cmd->target_name);
                return run_shell(buf);
            }
            return run_shell("ls --color=auto");
    }
}

// --- navigation ---

static int exec_go(const ParsedCommand *cmd) {
    // "go to <path>" puts path in destination (parser eats "to")
    // "go back" puts "back" in target_name
    const char *target = strlen(cmd->target_name) > 0 ? cmd->target_name :
                         strlen(cmd->destination) > 0 ? cmd->destination : "";
    char path[512];

    if (strlen(target) == 0) {
        const char *home = getenv("HOME");
        chdir(home ? home : "/");
        PRINT_SUCCESS("Home directory.");
        return 0;
    }

    if (strcasecmp(target, "back") == 0 || strcasecmp(target, "up") == 0) {
        strncpy(path, "..", sizeof(path));
    } else if (strcasecmp(target, "home") == 0) {
        const char *home = getenv("HOME");
        strncpy(path, home ? home : "/", sizeof(path));
    } else {
        strncpy(path, target, sizeof(path));
    }
    path[sizeof(path) - 1] = '\0';

    if (chdir(path) == 0) {
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd)))
            printf(C_GREEN C_BOLD "  ✓ " C_RESET C_GREEN "%s" C_RESET "\n", cwd);
        return 0;
    }

    printf(C_RED C_BOLD "  ✗ " C_RESET C_RED "Cannot navigate to '%s'" C_RESET "\n", path);
    return -1;
}

// --- file operations ---

static int exec_rename(const ParsedCommand *cmd) {
    if (strlen(cmd->destination) == 0) {
        PRINT_ERROR("Usage: rename <old> to <new>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "mv '%s' '%s'", cmd->target_name, cmd->destination);
    if (run_shell(buf) == 0) { PRINT_SUCCESS("Renamed."); return 0; }
    return -1;
}

static int exec_move(const ParsedCommand *cmd) {
    if (strlen(cmd->destination) == 0) {
        PRINT_ERROR("Usage: move <source> to <dest>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "mv '%s' '%s'", cmd->target_name, cmd->destination);
    if (run_shell(buf) == 0) { PRINT_SUCCESS("Moved."); return 0; }
    return -1;
}

static int exec_copy(const ParsedCommand *cmd) {
    if (strlen(cmd->destination) == 0) {
        PRINT_ERROR("Usage: copy <source> to <dest>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "cp -r '%s' '%s'", cmd->target_name, cmd->destination);
    if (run_shell(buf) == 0) { PRINT_SUCCESS("Copied."); return 0; }
    return -1;
}

// --- search ---

static int exec_find(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: find <filename>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "find . -iname '*%s*' 2>/dev/null | head -20", cmd->target_name);
    return run_shell(buf);
}

static int exec_search(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: search <text>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "grep -rn --color=auto '%s' . 2>/dev/null | head -30", cmd->target_name);
    return run_shell(buf);
}

// --- packages ---

static int exec_install(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: install <package>");
        return -1;
    }
    char buf[2048];
    if (cmd->modifier == MOD_SYSTEM || cmd->target_type == TGT_SYSTEM)
        snprintf(buf, sizeof(buf), "sudo apt install -y '%s'", cmd->target_name);
    else if (cmd->modifier == MOD_NPM)
        snprintf(buf, sizeof(buf), "npm install '%s'", cmd->target_name);
    else
        snprintf(buf, sizeof(buf), "pip install '%s'", cmd->target_name);
    return run_shell(buf);
}

static int exec_uninstall(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: uninstall <package>");
        return -1;
    }
    char buf[2048];
    if (cmd->modifier == MOD_SYSTEM || cmd->target_type == TGT_SYSTEM)
        snprintf(buf, sizeof(buf), "sudo apt remove '%s'", cmd->target_name);
    else
        snprintf(buf, sizeof(buf), "pip uninstall '%s'", cmd->target_name);
    return run_shell(buf);
}

// --- system/check ---

static int exec_check(const ParsedCommand *cmd) {
    char buf[2048];
    if (cmd->target_type == TGT_INTERNET) {
        printf(C_DIM "  Checking..." C_RESET "\n");
        return run_shell("ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1 && echo '  Connected' || echo '  No connection'");
    }
    if (cmd->target_type == TGT_STATUS)
        return run_shell("git status");

    if (cmd->target_type == TGT_VERSION || strlen(cmd->target_name) > 0) {
        const char *tool = strlen(cmd->target_name) > 0 ? cmd->target_name : "gcc";
        snprintf(buf, sizeof(buf), "%s --version 2>/dev/null || %s -V 2>/dev/null || echo 'Not found'", tool, tool);
        return run_shell(buf);
    }
    return run_shell("uname -a");
}

// --- git ---

static int exec_git_save(const ParsedCommand *cmd) {
    char buf[2048];
    const char *msg = strlen(cmd->target_name) > 0 ? cmd->target_name : "Update";
    run_shell("git add -A");
    snprintf(buf, sizeof(buf), "git commit -m '%s'", msg);
    return run_shell(buf);
}

// --- run/edit ---

static int exec_run(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: run <script>");
        return -1;
    }
    char buf[2048];
    const char *name = cmd->target_name;
    size_t len = strlen(name);

    if (len > 3 && strcmp(name + len - 3, ".py") == 0)
        snprintf(buf, sizeof(buf), "python3 '%s'", name);
    else if (len > 3 && strcmp(name + len - 3, ".sh") == 0)
        snprintf(buf, sizeof(buf), "bash '%s'", name);
    else
        snprintf(buf, sizeof(buf), "./'%s'", name);
    return run_shell(buf);
}

static int exec_edit(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: edit <file>");
        return -1;
    }
    char buf[2048];
    const char *editor = getenv("EDITOR");
    if (!editor) editor = "nano";
    snprintf(buf, sizeof(buf), "%s '%s'", editor, cmd->target_name);
    return run_shell(buf);
}

// --- archive ---

static int exec_compress(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: compress <file/folder>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "tar -czf '%s.tar.gz' '%s'", cmd->target_name, cmd->target_name);
    if (run_shell(buf) == 0) { PRINT_SUCCESS("Compressed."); return 0; }
    return -1;
}

static int exec_extract(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: extract <archive>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "tar -xzf '%s' 2>/dev/null || unzip '%s' 2>/dev/null", cmd->target_name, cmd->target_name);
    if (run_shell(buf) == 0) { PRINT_SUCCESS("Extracted."); return 0; }
    return -1;
}

static int exec_kill(const ParsedCommand *cmd) {
    if (strlen(cmd->target_name) == 0) {
        PRINT_ERROR("Usage: kill <process>");
        return -1;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "pkill '%s' 2>/dev/null || kill %s 2>/dev/null", cmd->target_name, cmd->target_name);
    return run_shell(buf);
}

// --- main dispatch ---

int executor_run(const ParsedCommand *cmd) {
    char buf[2048];

    if (cmd->is_raw)
        return run_shell(cmd->target_name);

    switch (cmd->verb) {
        case VERB_CREATE:    return exec_create(cmd);
        case VERB_DELETE:    return exec_delete(cmd);
        case VERB_SHOW:      return exec_show(cmd);
        case VERB_GO:        return exec_go(cmd);
        case VERB_RENAME:    return exec_rename(cmd);
        case VERB_MOVE:      return exec_move(cmd);
        case VERB_COPY:      return exec_copy(cmd);
        case VERB_FIND:      return exec_find(cmd);
        case VERB_SEARCH:    return exec_search(cmd);
        case VERB_INSTALL:   return exec_install(cmd);
        case VERB_UNINSTALL: return exec_uninstall(cmd);
        case VERB_CHECK:     return exec_check(cmd);
        case VERB_RUN:       return exec_run(cmd);
        case VERB_EDIT:      return exec_edit(cmd);
        case VERB_COMPRESS:  return exec_compress(cmd);
        case VERB_EXTRACT:   return exec_extract(cmd);
        case VERB_KILL:      return exec_kill(cmd);
        case VERB_SAVE:      return exec_git_save(cmd);

        case VERB_PING:
            snprintf(buf, sizeof(buf), "ping -c 4 '%s'",
                strlen(cmd->target_name) > 0 ? cmd->target_name : "8.8.8.8");
            return run_shell(buf);

        case VERB_STAGE:     return run_shell("git add -A");
        case VERB_UNSTAGE:   return run_shell("git reset HEAD");
        case VERB_COMMIT:
            snprintf(buf, sizeof(buf), "git commit -m '%s'",
                strlen(cmd->target_name) > 0 ? cmd->target_name : "Update");
            return run_shell(buf);
        case VERB_PUSH:      return run_shell("git push");
        case VERB_PULL:      return run_shell("git pull");
        case VERB_CLONE:
            snprintf(buf, sizeof(buf), "git clone '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_SWITCH:
            snprintf(buf, sizeof(buf), "git checkout '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_MERGE:
            snprintf(buf, sizeof(buf), "git merge '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_STASH:     return run_shell("git stash");
        case VERB_POP:       return run_shell("git stash pop");
        case VERB_UNDO:      return run_shell("git checkout -- .");
        case VERB_DISCARD:   return run_shell("git checkout -- .");

        case VERB_UPDATE:    return run_shell("sudo apt update");
        case VERB_UPGRADE:   return run_shell("sudo apt upgrade -y");
        case VERB_DOWNLOAD:
            snprintf(buf, sizeof(buf), "wget '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_ACTIVATE:
            printf(C_DIM "  Run: " C_RESET "source %s/bin/activate\n",
                strlen(cmd->target_name) > 0 ? cmd->target_name : "venv");
            return 0;
        case VERB_DEACTIVATE:
            printf(C_DIM "  Run: " C_RESET "deactivate\n");
            return 0;
        case VERB_START:
            if (cmd->target_type == TGT_REPO) return run_shell("git init");
            if (strlen(cmd->target_name) > 0) return run_shell(cmd->target_name);
            return run_shell("git init");
        case VERB_SHUTDOWN:  return run_shell("sudo shutdown now");
        case VERB_RESTART:   return run_shell("sudo reboot");
        case VERB_COMPARE:
            if (strlen(cmd->destination) > 0)
                snprintf(buf, sizeof(buf), "diff '%s' '%s'", cmd->target_name, cmd->destination);
            else
                snprintf(buf, sizeof(buf), "git diff '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_COUNT:
            if (cmd->target_type == TGT_LINES && strlen(cmd->target_name) > 0)
                snprintf(buf, sizeof(buf), "wc -l '%s'", cmd->target_name);
            else if (cmd->target_type == TGT_FILES)
                snprintf(buf, sizeof(buf), "ls -1 | wc -l");
            else if (strlen(cmd->target_name) > 0)
                snprintf(buf, sizeof(buf), "wc -l '%s'", cmd->target_name);
            else
                snprintf(buf, sizeof(buf), "ls -1 | wc -l");
            return run_shell(buf);
        case VERB_SORT:
            snprintf(buf, sizeof(buf), "sort '%s'", cmd->target_name);
            return run_shell(buf);
        case VERB_CALCULATE:
            snprintf(buf, sizeof(buf), "echo '%s' | bc -l 2>/dev/null || echo 'bc not installed'", cmd->target_name);
            return run_shell(buf);

        default:
            PRINT_ERROR("Command not yet implemented.");
            return -1;
    }
}
