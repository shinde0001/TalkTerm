/*
 * main.c - TalkTerm
 * Main REPL loop. Reads input, parses it, executes it.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.h"
#include "history.h"
#include "colors.h"
#include "executor.h"

#define VERSION "0.2.0"

/* ctrl+c handler - just reset the prompt instead of killing the process */
static volatile int interrupted = 0;

static void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

static void print_banner(void) {
    printf("\n");
    printf(C_CYAN C_BOLD
        "  ┌──────────────────────────────────────────┐\n"
        "  │                                          │\n"
        "  │   " C_BWHITE "TalkTerm" C_CYAN "  v%s                     │\n"
        "  │   " C_RESET C_DIM "Talk to your terminal" C_CYAN C_BOLD "              │\n"
        "  │                                          │\n"
        "  │   " C_RESET C_GRAY "Type " C_BWHITE "help" C_GRAY " for commands, " C_BWHITE "exit" C_GRAY " to quit" C_CYAN C_BOLD " │\n"
        "  │                                          │\n"
        "  └──────────────────────────────────────────┘\n"
        C_RESET "\n", VERSION);
}

/* build the colored prompt with cwd */
static void build_prompt(char *prompt, size_t size) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strncpy(cwd, "???", sizeof(cwd));

    // shorten /home/user to ~
    const char *home = getenv("HOME");
    char display_path[256];

    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
    } else {
        strncpy(display_path, cwd, sizeof(display_path));
        display_path[sizeof(display_path) - 1] = '\0';
    }

    snprintf(prompt, size,
        C_BOLD C_CYAN "talk" C_RESET C_DIM ":" C_BOLD C_BLUE "%s" C_RESET C_BGREEN " ❯ " C_RESET,
        display_path);
}

static void print_help(void) {
    printf("\n");
    printf(C_BOLD C_BWHITE "  Commands:" C_RESET "\n\n");

    printf(C_CYAN C_BOLD "  Filesystem" C_RESET "\n");
    printf(C_GRAY "     create folder <name>       " C_RESET "Create a directory\n");
    printf(C_GRAY "     create file <name>         " C_RESET "Create an empty file\n");
    printf(C_GRAY "     delete file <name>         " C_RESET "Delete a file\n");
    printf(C_GRAY "     delete folder <name>       " C_RESET "Delete a directory\n");
    printf(C_GRAY "     rename file <old> to <new> " C_RESET "Rename a file\n");
    printf(C_GRAY "     move <src> to <dest>       " C_RESET "Move a file or folder\n");
    printf(C_GRAY "     copy <src> to <dest>       " C_RESET "Copy a file or folder\n");
    printf(C_GRAY "     show file <name>           " C_RESET "Display file contents\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  Navigation" C_RESET "\n");
    printf(C_GRAY "     go to <path>               " C_RESET "Change directory\n");
    printf(C_GRAY "     go back                    " C_RESET "Go up one level\n");
    printf(C_GRAY "     go home                    " C_RESET "Go to home directory\n");
    printf(C_GRAY "     show files                 " C_RESET "List files\n");
    printf(C_GRAY "     show hidden files          " C_RESET "List all files (inc. hidden)\n");
    printf(C_GRAY "     show files detailed        " C_RESET "Detailed listing\n");
    printf(C_GRAY "     show current folder        " C_RESET "Print working directory\n");
    printf(C_GRAY "     find file <name>           " C_RESET "Search for a file\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  Packages" C_RESET "\n");
    printf(C_GRAY "     install <package>          " C_RESET "Install via pip\n");
    printf(C_GRAY "     install system <package>   " C_RESET "Install via apt\n");
    printf(C_GRAY "     uninstall <package>        " C_RESET "Uninstall a package\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  System" C_RESET "\n");
    printf(C_GRAY "     check <tool> version       " C_RESET "Check tool version\n");
    printf(C_GRAY "     show system info           " C_RESET "OS information\n");
    printf(C_GRAY "     show memory                " C_RESET "RAM usage\n");
    printf(C_GRAY "     show disk                  " C_RESET "Disk usage\n");
    printf(C_GRAY "     clear                      " C_RESET "Clear screen\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  Git" C_RESET "\n");
    printf(C_GRAY "     start git                  " C_RESET "git init\n");
    printf(C_GRAY "     check git status           " C_RESET "git status\n");
    printf(C_GRAY "     save changes <message>     " C_RESET "add + commit\n");
    printf(C_GRAY "     push changes               " C_RESET "git push\n");
    printf(C_GRAY "     pull changes               " C_RESET "git pull\n");
    printf(C_GRAY "     create branch <name>       " C_RESET "New branch\n");
    printf(C_GRAY "     show branches              " C_RESET "List branches\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  Python" C_RESET "\n");
    printf(C_GRAY "     run <script.py>            " C_RESET "Run a script\n");
    printf(C_GRAY "     create environment <name>  " C_RESET "Create venv\n");
    printf("\n");

    printf(C_CYAN C_BOLD "  Network" C_RESET "\n");
    printf(C_GRAY "     check internet             " C_RESET "Test connectivity\n");
    printf(C_GRAY "     show ip                    " C_RESET "Local IP address\n");
    printf(C_GRAY "     ping <host>                " C_RESET "Ping a host\n");
    printf("\n");

    printf(C_DIM "  You can also type regular terminal commands directly" C_RESET "\n");
    printf(C_DIM "  (ls, git status, sudo apt update, etc.)" C_RESET "\n\n");
}

static void print_version(void) {
    printf(C_CYAN C_BOLD "  TalkTerm" C_RESET " v%s\n", VERSION);
    printf(C_DIM "  A simplified CLI for Linux\n" C_RESET);
}

int main(void) {
    // setup ctrl+c handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    parser_init();
    history_init();
    print_banner();

    char prompt[512];

    while (1) {
        interrupted = 0;
        build_prompt(prompt, sizeof(prompt));

        char *input = readline(prompt);

        if (input == NULL) {  // ctrl+d
            printf("\n");
            PRINT_INFO("Goodbye!\n");
            break;
        }

        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        add_history(input);

        ParsedCommand cmd;
        int result = parser_parse(input, &cmd);

        switch (cmd.verb) {
            case VERB_EXIT:
                printf("\n");
                PRINT_INFO("Goodbye!\n");
                free(input);
                goto cleanup;

            case VERB_HELP:
                print_help();
                break;

            case VERB_VERSION:
                print_version();
                break;

            case VERB_CLEAR:
                printf("\033[2J\033[H");
                break;

            case VERB_HISTORY: {
                HIST_ENTRY **hist_list = history_list();
                if (hist_list) {
                    printf("\n" C_BOLD "  Command History:" C_RESET "\n\n");
                    int start = history_length > 20 ? history_length - 20 : 0;
                    for (int i = start; i < history_length; i++) {
                        printf(C_DIM "  %3d  " C_RESET "%s\n", i + 1, hist_list[i]->line);
                    }
                    printf("\n");
                } else {
                    PRINT_INFO("No history yet.");
                }
                break;
            }

            case VERB_RAW:
                executor_run(&cmd);
                break;

            case VERB_UNKNOWN:
                if (result == -1) {
                    printf(C_RED C_BOLD "  ✗ " C_RESET C_RED
                        "Command not recognized: " C_RESET C_BOLD "'%s'" C_RESET "\n", input);
                    printf(C_DIM "    Type " C_RESET C_BWHITE "help" C_RESET C_DIM
                        " to see available commands.\n" C_RESET);
                }
                break;

            default:
                executor_run(&cmd);
                break;
        }

        free(input);
    }

cleanup:
    history_cleanup();
    return 0;
}
