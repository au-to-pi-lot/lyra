#pragma once

#include "types/value.h"
#include "closure.h"

// Core intrinsic functions
Value *intrinsic_lambda(GC *gc, Closure *closure, Value *args);
Value *intrinsic_if(GC *gc, Closure *closure, Value *args);
Value *intrinsic_define(GC *gc, Closure *closure, Value *args);

// Arithmetic intrinsics
Value *intrinsic_add(GC *gc, Closure *closure, Value *args);
Value *intrinsic_sub(GC *gc, Closure *closure, Value *args);
Value *intrinsic_mul(GC *gc, Closure *closure, Value *args);
Value *intrinsic_div(GC *gc, Closure *closure, Value *args);
Value *intrinsic_divmod(GC *gc, Closure *closure, Value *args);

// Comparison intrinsics
Value *intrinsic_eq(GC *gc, Closure *closure, Value *args);
