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

void repl(Closure *global) {
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

        Value *ast = parse(line);
        if (ast == NULL) {
            printf("Parse error\n");
            continue;
        }

        Value *result = evaluate(global, ast);
        if (result == NULL) {
            printf("Evaluation error\n");
            continue;
        }

        printf("%s\n", utstring_body(repr(result, NULL)));
    }

    printf("\n");
    history_end(hist);
    el_end(el);
}

void run_file(Closure *global, const char *filename) {
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

    Value *ast = parse(string);
    if (ast == NULL) {
        fprintf(stderr, "Parse error in file '%s'\n", filename);
        free(string);
        exit(1);
    }

    Value *result = evaluate(global, ast);
    if (result == NULL) {
        fprintf(stderr, "Evaluation error in file '%s'\n", filename);
        free(string);
        exit(1);
    }

    printf("%s\n", utstring_body(repr(result, NULL)));
    free(string);
}

int main(int argc, char **argv) {
    GC gc;
    gc_init(&gc);
    Closure *global = make_closure(gc, NULL);
    prelude(global);

    if (argc < 2) {
        // No arguments: enter REPL mode
        repl(global);
    } else {
        // File mode
        run_file(global, argv[1]);
    }

    return 0;
}
