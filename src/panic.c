#include "panic.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Messages longer than this are truncated. A fixed static buffer keeps the
// panic path free of allocation (nothing to leak or free after the jump).
#define PANIC_MESSAGE_MAX 1024

static PanicHandler *innermost = NULL;
static char message[PANIC_MESSAGE_MAX];

void panic_push(PanicHandler *handler) {
    handler->prev = innermost;
    innermost = handler;
}

void panic_pop(PanicHandler *handler) {
    // Tolerate a mismatched pop rather than corrupting the chain; it can only
    // happen through a C-side bug, and the handler being popped is then
    // already gone.
    if (innermost == handler) {
        innermost = handler->prev;
    }
}

const char *panic_message(void) {
    return message;
}

void panic_report(void) {
    fflush(stdout);
    fprintf(stderr, "panic: %s\n", message);
    fflush(stderr);
}

void lyra_panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    PanicHandler *handler = innermost;
    if (handler == NULL) {
        panic_report();
        exit(1);
    }

    innermost = handler->prev;
    longjmp(handler->env, 1);
}
