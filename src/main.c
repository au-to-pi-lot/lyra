#define _POSIX_C_SOURCE 200809L
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <replxx.h>
#include "eval.h"
#include "parser.h"
#include "repr.h"
#include "prelude.h"
#include "gc.h"
#include "types/list.h"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO _fileno(stdin)
#else
#include <unistd.h>
#endif

void repl(GC *gc, Closure *global) {
    int is_tty = isatty(STDIN_FILENO);

    Replxx *replxx = replxx_init();
    replxx_set_max_history_size(replxx, 800);

    if (is_tty) {
        printf("Welcome to Lyra REPL. Press Ctrl+D to exit.\n");
    }

    const char *line;
    while ((line = replxx_input(replxx, "> ")) != NULL) {
        // Skip empty lines
        if (line[0] == '\0') continue;

        // Add to history
        replxx_history_add(replxx, line);

        Value *ast = parse(gc, line);
        if (ast == NULL) {
            printf("Parse error\n");
            continue;
        }

        Value *result = eval_s_expr(gc, global, ast);
        if (result == NULL) {
            printf("Evaluation error\n");
            continue;
        }

        printf("%s\n", utstring_body(repr(gc, result)));

        // Run garbage collection after each REPL expression
        gc_collect(gc, global);
    }

    printf("\n");
    replxx_end(replxx);
}

void run_file(GC *gc, Closure *global, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    // Parse and evaluate all expressions in the file
    const char *input = string;
    Value *result = NIL;

    while (*input != '\0') {
        size_t consumed = 0;
        Value *ast = parse_with_pos(gc, input, &consumed);

        if (ast == NULL) {
            // Check if we've reached the end (only whitespace/comments left)
            int only_whitespace = 1;
            for (const char *p = input; *p != '\0'; p++) {
                if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                    only_whitespace = 0;
                    break;
                }
            }

            if (only_whitespace) {
                break;
            }

            fprintf(stderr, "Parse error in file '%s'\n", filename);
            free(string);
            exit(1);
        }

        result = eval_s_expr(gc, global, ast);
        if (result == NULL) {
            fprintf(stderr, "Evaluation error in file '%s'\n", filename);
            free(string);
            exit(1);
        }

        input += consumed;
    }

    // Print only the final result
    printf("%s\n", utstring_body(repr(gc, result)));
    free(string);

    // Run garbage collection after file evaluation
    gc_collect(gc, global);
}

void print_help(const char *program_name) {
    printf("Usage: %s [options] [file]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -c CODE    Execute CODE and exit\n");
    printf("  -h         Show this help message\n");
    printf("\n");
    printf("If no options are provided:\n");
    printf("  %s         Start interactive REPL\n", program_name);
    printf("  %s FILE    Execute FILE and exit\n", program_name);
    printf("\n");
}

void run_command(GC *gc, Closure *global, const char *code) {
    // Parse and evaluate all expressions in the code string
    const char *input = code;
    Value *result = NIL;

    while (*input != '\0') {
        size_t consumed = 0;
        Value *ast = parse_with_pos(gc, input, &consumed);

        if (ast == NULL) {
            // Check if we've reached the end (only whitespace left)
            int only_whitespace = 1;
            for (const char *p = input; *p != '\0'; p++) {
                if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                    only_whitespace = 0;
                    break;
                }
            }

            if (only_whitespace) {
                break;
            }

            fprintf(stderr, "Parse error\n");
            exit(1);
        }

        result = eval_s_expr(gc, global, ast);
        if (result == NULL) {
            fprintf(stderr, "Evaluation error\n");
            exit(1);
        }

        input += consumed;
    }

    // Print only the final result
    printf("%s\n", utstring_body(repr(gc, result)));

    // Run garbage collection after command evaluation
    gc_collect(gc, global);
}

int main(int argc, char **argv) {
    // Handle help flag before initializing GC
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    GC gc;
    gc_init(&gc);
    Closure *global = make_closure(&gc, NULL);
    prelude(&gc, global);

    // Parse command-line arguments
    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        // Command mode: lyra -c "code"
        run_command(&gc, global, argv[2]);
    } else if (argc >= 2) {
        // File mode: lyra file.lisp
        run_file(&gc, global, argv[1]);
    } else {
        // REPL mode: lyra
        repl(&gc, global);
    }

    // Clean up all GC-managed memory before exit
    gc_free_all(&gc);

    return 0;
}
