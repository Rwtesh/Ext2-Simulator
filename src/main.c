#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/mkfs.h"
#include "../include/info.h"
#include "../include/ls.h"
#include "../include/touch.h"
#include "../include/mkdir.h"
#include "../include/rm.h"
#include "../include/fileio.h"
#define LINE_MAX 2048
#define TOK_MAX  64

static void print_help(void)
{
    printf("Commands:\n");
    printf("  mkfs   <img> <size_mb>\n");
    printf("  info   <img>\n");
    printf("  ls     <img> [path]\n");
    printf("  touch  <img> <path>\n");
    printf("  mkdir  <img> <path>\n");
    printf("  rm     <img> <path>\n");
    printf("  cat    <img> <path>\n");
    printf("  write  <img> <path> <text...>\n");
    printf("  help\n");
    printf("  exit | quit\n");
}

static int split_ws(char* line, char* argv[], int maxv)
{
    
    int argc = 0;
    char* p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            *p++ = '\0';
        if (!*p) break;

        if (argc >= maxv) break;
        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p++;
    }
    return argc;
}

static char* find_remainder(char* line, int token_index)
{
    int tok = 0;
    char* p = line;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    while (*p) {
        if (tok == token_index) return p;
        tok++;

        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    }
    return NULL;
}

int main(void)
{
    char line[LINE_MAX];

    while (1) {
        printf("ext2sim> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        char line_copy[LINE_MAX];
        strncpy(line_copy, line, sizeof(line_copy));
        line_copy[sizeof(line_copy) - 1] = '\0';

        char* argv[TOK_MAX];
        int argc = split_ws(line_copy, argv, TOK_MAX);
        if (argc == 0) continue;

        const char* cmd = argv[0];
        if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) {
            break;

        } else if (!strcmp(cmd, "help")) {
            print_help();

        } else if (!strcmp(cmd, "mkfs")) {
            if (argc != 3) {
                fprintf(stderr, "usage: mkfs <img> <size_mb>\n");
                continue;
            }
            u32 size_mb = (u32)strtoul(argv[2], NULL, 10);
            if (cmd_mkfs(argv[1], size_mb) != 0)
                fprintf(stderr, "mkfs failed\n");

        } else if (!strcmp(cmd, "info")) {
            if (argc != 2) {
                fprintf(stderr, "usage: info <img>\n");
                continue;
            }
            if (cmd_info(argv[1]) != 0)
                fprintf(stderr, "info failed\n");

        } else if (!strcmp(cmd, "ls")) {
            if (argc != 2 && argc != 3) {
                fprintf(stderr, "usage: ls <img> [path]\n");
                continue;
            }
            const char* path = (argc == 3) ? argv[2] : "/";
            if (cmd_ls(argv[1], path) != 0)
                fprintf(stderr, "ls failed\n");

        } else if (!strcmp(cmd, "touch")) {
            if (argc != 3) {
                fprintf(stderr, "usage: touch <img> <path>\n");
                continue;
            }
            if (cmdTouch(argv[1], argv[2]) != 0)
                fprintf(stderr, "touch failed\n");

        } else if (!strcmp(cmd, "mkdir")) {
            if (argc != 3) {
                fprintf(stderr, "usage: mkdir <img> <path>\n");
                continue;
            }
            if (cmd_mkdir(argv[1], argv[2]) != 0)
                fprintf(stderr, "mkdir failed\n");

        } else if (!strcmp(cmd, "rm")) {
            if (argc != 3) {
                fprintf(stderr, "usage: rm <img> <path>\n");
                continue;
            }
            if (cmd_rm(argv[1], argv[2]) != 0)
                fprintf(stderr, "rm failed\n");

        } else if (!strcmp(cmd, "cat")) {
            if (argc != 3) {
                fprintf(stderr, "usage: cat <img> <path>\n");
                continue;
            }
            if (cmd_cat(argv[1], argv[2]) != 0)
                fprintf(stderr, "cat failed\n");

        } else if (!strcmp(cmd, "write")) {
            if (argc < 4) {
                fprintf(stderr, "usage: write <img> <path> <text...>\n");
                continue;
            }

            char* text = find_remainder(line, 3);
            if (!text) text = "";

            size_t n = strlen(text);
            while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
                text[n - 1] = '\0';
                n--;
            }

            if (cmd_write(argv[1], argv[2], text, (u32)strlen(text)) != 0)
                fprintf(stderr, "write failed\n");

        } else {
            fprintf(stderr, "unknown command: %s (try 'help')\n", cmd);
        }
    }

    return 0;
}