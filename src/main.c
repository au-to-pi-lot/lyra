#define _POSIX_C_SOURCE 200809L
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <histedit.h>
#include "eval.h"
#include "parser.h"
#include "repr.h"
#include "prelude.h"
#include "gc.h"

static char *prompt(EditLine *el) {
    (void)el;
    return "> ";
}

void repl(GC *gc, Closure *global) {
    EditLine *el;
    History *hist;
    HistEvent ev;
    const char *line;
    int num;
    int is_tty = isatty(STDIN_FILENO);

    // Initialize editline
    el = el_init("lyra", stdin, stdout, stderr);
    el_set(el, EL_PROMPT, &prompt);
    el_set(el, EL_EDITOR, "emacs");

    // Initialize history
    hist = history_init();
    history(hist, &ev, H_SETSIZE, 800);
    el_set(el, EL_HIST, history, hist);

    if (is_tty) {
        printf("Welcome to Lyra REPL. Press Ctrl+D to exit.\n");
    }

    while ((line = el_gets(el, &num)) != NULL && num > 0) {
        // Skip empty lines
        if (num <= 1) continue;

        // Add to history
        history(hist, &ev, H_ENTER, line);

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
    history_end(hist);
    el_end(el);
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

    Value *ast = parse(gc, string);
    if (ast == NULL) {
        fprintf(stderr, "Parse error in file '%s'\n", filename);
        free(string);
        exit(1);
    }

    Value *result = eval_s_expr(gc, global, ast);
    if (result == NULL) {
        fprintf(stderr, "Evaluation error in file '%s'\n", filename);
        free(string);
        exit(1);
    }

    printf("%s\n", utstring_body(repr(gc, result)));
    free(string);

    // Run garbage collection after file evaluation
    gc_collect(gc, global);
}

int main(int argc, char **argv) {
    GC gc;
    gc_init(&gc);
    Closure *global = make_closure(&gc, NULL);
    prelude(&gc, global);

    if (argc < 2) {
        // No arguments: enter REPL mode
        repl(&gc, global);
    } else {
        // File mode
        run_file(&gc, global, argv[1]);
    }

    // Clean up all GC-managed memory before exit
    gc_free_all(&gc);

    return 0;
}
