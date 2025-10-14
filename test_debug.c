#include <stdio.h>
#include "src/eval.h"
#include "src/parser.h"
#include "src/prelude.h"
#include "src/closure.h"
#include "src/value.h"

int main() {
    Closure *closure = make_closure(NULL);
    prelude(closure);

    printf("Parsing: (define x 42)\n");
    Value *parsed1 = parse("(define x 42)");
    if (!parsed1) {
        printf("Parse failed\n");
        return 1;
    }
    printf("Parse successful\n");

    printf("Evaluating: (define x 42)\n");
    Value *result1 = eval_s_expr(closure, parsed1);
    if (!result1) {
        printf("Eval failed\n");
        return 1;
    }
    printf("Eval successful, result type: %d\n", result1->type);

    printf("Parsing: x\n");
    Value *parsed2 = parse("x");
    if (!parsed2) {
        printf("Parse failed\n");
        return 1;
    }
    printf("Parse successful, type: %d\n", parsed2->type);

    printf("Evaluating: x\n");
    Value *result2 = eval_s_expr(closure, parsed2);
    if (!result2) {
        printf("Eval failed - variable not found\n");
        return 1;
    }
    printf("Eval successful, result: %d\n", result2->data.as_int);

    return 0;
}
