#pragma once

#include "types/value.h"
#include "closure.h"
#include "gc.h"

Value *evaluate(GC *gc, Closure *closure, Value *expr);