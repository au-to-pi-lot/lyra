#pragma once

#include "types/value.h"
#include "closure.h"

// Core intrinsic functions
Value *intrinsic_lambda(Closure *closure, Value *args);
Value *intrinsic_if(Closure *closure, Value *args);
Value *intrinsic_define(Closure *closure, Value *args);

// Arithmetic intrinsics
Value *intrinsic_add(Closure *closure, Value *args);
Value *intrinsic_sub(Closure *closure, Value *args);
Value *intrinsic_mul(Closure *closure, Value *args);
Value *intrinsic_div(Closure *closure, Value *args);
Value *intrinsic_divmod(Closure *closure, Value *args);

// Comparison intrinsics
Value *intrinsic_eq(Closure *closure, Value *args);
