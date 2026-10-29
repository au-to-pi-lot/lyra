#pragma once

// Panics: Lyra's mechanism for unrecoverable (programmer) errors -- wrong
// types, wrong arity, undefined variables, calling a non-callable, and
// explicit `(panic ...)`. A panic unwinds straight out of evaluation to the
// innermost installed handler via longjmp; there is no way to catch one from
// Lyra code. Recoverable failures (e.g. file not found) will be ok/err result
// values instead.
//
// Usage (C side; see main.c and tests/test_eval.c):
//
//     PanicHandler handler;
//     panic_push(&handler);
//     if (setjmp(handler.env) == 0) {
//         ... evaluate ...
//         panic_pop(&handler);
//     } else {
//         // A panic happened. The handler has already been popped.
//         fprintf(stderr, "panic: %s\n", panic_message());
//     }
//
// setjmp has to be called in the caller's own frame (it can't be wrapped in
// a helper function), which is why this is a push/setjmp/pop protocol rather
// than a single call. Locals modified after setjmp and read in the panic
// branch must be `volatile`.
//
// GC safety: collection only runs at top-level safe points (gc_collect in
// main.c), never during evaluation, so a panic can't interrupt a collection.
// Objects allocated by the aborted evaluation stay registered with the GC and
// are swept by the next collection if they are unreachable from the root
// closure. Cons cells are allocated with car/cdr = NIL, so a half-built list
// is never traversed through garbage pointers.
//
// The parser never panics: it reports errors by returning NULL, so its
// manually managed tokens can't leak through a longjmp. Callers that turn a
// parse failure into a panic (eval-string) do so after parse() has returned.

#include <setjmp.h>

#if defined(_MSC_VER)
#define LYRA_NORETURN __declspec(noreturn)
#elif defined(__GNUC__) || defined(__clang__)
#define LYRA_NORETURN __attribute__((noreturn))
#else
#define LYRA_NORETURN
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LYRA_PRINTF_FORMAT(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define LYRA_PRINTF_FORMAT(fmt_idx, args_idx)
#endif

typedef struct PanicHandler {
    jmp_buf env;
    struct PanicHandler *prev;
} PanicHandler;

// Install `handler` as the innermost panic handler.
void panic_push(PanicHandler *handler);

// Remove `handler` after its protected region finished without panicking.
// It must be the innermost handler.
void panic_pop(PanicHandler *handler);

// Raise a panic with a printf-style message. Pops the innermost handler and
// longjmps to it. With no handler installed, prints the message to stderr
// and exits with status 1.
LYRA_NORETURN void lyra_panic(const char *fmt, ...) LYRA_PRINTF_FORMAT(1, 2);

// The message of the most recent panic (without any "panic: " prefix).
const char *panic_message(void);

// Print the most recent panic's message in the user-facing format. Flushes
// stdout first so the message lands after any output the program produced.
void panic_report(void);
