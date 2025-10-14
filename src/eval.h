#pragma once

#include "types/value.h"
#include "closure.h"
#include "gc.h"

Value *eval_s_expr(GC *gc, Closure *closure, Value *expr);

Value *eval(GC *gc, Closure *closure, char *lyra);
