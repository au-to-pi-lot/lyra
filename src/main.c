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
#include "panic.h"
#include "types/list.h"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO _fileno(stdin)
#else
#include <unistd.h>
#endif

// Evaluate one REPL expression and print its result. A panic abandons the
// expression and returns to the prompt. The session survives: definitions
// made before the panic are kept, and the caller's next gc_collect sweeps
// whatever the aborted evaluation allocated. (setjmp lives in this small
// function so its frame holds no locals that change after setjmp.)
static void repl_eval(GC *gc, Closure *global, Value *ast) {
    PanicHandler handler;
    panic_push(&handler);
    if (setjmp(handler.env) != 0) {
        panic_report();
        return;
    }

    Value *result = eval_s_expr(gc, global, ast);
    panic_pop(&handler);
    if (result == NULL) {
        printf("Evaluation error\n");
    } else {
        printf("%s\n", utstring_body(repr(gc, result)));
    }
}

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

        repl_eval(gc, global, ast);

        // Run garbage collection after each REPL expression
        gc_collect(gc, global);
    }

    printf("\n");
    replxx_end(replxx);
}

static int only_whitespace(const char *input) {
    for (const char *p = input; *p != '\0'; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            return 0;
        }
    }
    return 1;
}

// Parse and evaluate every expression in `source`, printing only the final
// result. Any failure (parse error, evaluation error, panic) is reported on
// stderr and exits with status 1. `context` (may be NULL) names the source in
// parse/evaluation error messages. `owned` (may be NULL) is freed before
// exiting or returning.
static void eval_source(GC *gc, Closure *global, const char *source, const char *context, char *owned) {
    const char *input = source;
    Value *result = NIL;

    while (*input != '\0') {
        size_t consumed = 0;
        Value *ast = parse_with_pos(gc, input, &consumed);

        if (ast == NULL) {
            // Reached the end (only whitespace/comments left)?
            if (only_whitespace(input)) {
                break;
            }

            if (context) {
                fprintf(stderr, "Parse error in file '%s'\n", context);
            } else {
                fprintf(stderr, "Parse error\n");
            }
            free(owned);
            exit(1);
        }

        result = eval_s_expr(gc, global, ast);
        if (result == NULL) {
            if (context) {
                fprintf(stderr, "Evaluation error in file '%s'\n", context);
            } else {
                fprintf(stderr, "Evaluation error\n");
            }
            free(owned);
            exit(1);
        }

        input += consumed;
    }

    // Print only the final result
    printf("%s\n", utstring_body(repr(gc, result)));
    free(owned);

    // Run garbage collection after evaluation
    gc_collect(gc, global);
}

// eval_source under a panic handler: a panic is reported on stderr and
// exits with status 1.
static void run_source(GC *gc, Closure *global, const char *source, const char *context, char *owned) {
    PanicHandler handler;
    panic_push(&handler);
    if (setjmp(handler.env) != 0) {
        panic_report();
        free(owned);
        exit(1);
    }
    eval_source(gc, global, source, context, owned);
    panic_pop(&handler);
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

    run_source(gc, global, string, filename, string);
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
    run_source(gc, global, code, NULL, NULL);
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
