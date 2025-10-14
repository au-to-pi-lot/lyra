#include "cons.h"

Cons *make_cons(GC *gc, Value *car, Value *cdr) {
    Cons *result = gc_alloc_cons(gc);
    result -> car = car;
    result -> cdr = cdr;
    return result;
}