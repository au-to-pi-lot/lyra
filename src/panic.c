#include "panic.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Messages longer than this are truncated. A fixed static buffer keeps the
// panic path free of allocation (nothing to leak or free after the jump).
#define PANIC_MESSAGE_MAX 1024

static PanicHandler *innermost = NULL;
static char message[PANIC_MESSAGE_MAX];
static int exit_status = PANIC_DEFAULT_STATUS;

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

int panic_exit_status(void) {
    return exit_status;
}

void panic_report(void) {
    fflush(stdout);
    fprintf(stderr, "panic: %s\n", message);
    fflush(stderr);
}

LYRA_NORETURN static void vpanic(int status, const char *fmt, va_list args) {
    vsnprintf(message, sizeof(message), fmt, args);
    exit_status = status;

    PanicHandler *handler = innermost;
    if (handler == NULL) {
        panic_report();
        exit(status);
    }

    innermost = handler->prev;
    longjmp(handler->env, 1);
}

void lyra_panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vpanic(PANIC_DEFAULT_STATUS, fmt, args);
}

void lyra_panic_with_status(int status, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vpanic(status, fmt, args);
}
