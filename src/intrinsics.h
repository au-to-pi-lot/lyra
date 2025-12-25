#pragma once

#include "types/value.h"
#include "closure.h"

// Core intrinsic functions
Value *intrinsic_lambda(GC *gc, Closure *closure, Value *args);
Value *intrinsic_if(GC *gc, Closure *closure, Value *args);
Value *intrinsic_define(GC *gc, Closure *closure, Value *args);
Value *intrinsic_define_macro(GC *gc, Closure *closure, Value *args);

// Boolean operators
Value *intrinsic_and(GC *gc, Closure *closure, Value *args);
Value *intrinsic_or(GC *gc, Closure *closure, Value *args);
Value *intrinsic_not(GC *gc, Closure *closure, Value *args);

// Arithmetic intrinsics
Value *intrinsic_add(GC *gc, Closure *closure, Value *args);
Value *intrinsic_sub(GC *gc, Closure *closure, Value *args);
Value *intrinsic_mul(GC *gc, Closure *closure, Value *args);
Value *intrinsic_div(GC *gc, Closure *closure, Value *args);
Value *intrinsic_divmod(GC *gc, Closure *closure, Value *args);

// Comparison intrinsics
Value *intrinsic_eq(GC *gc, Closure *closure, Value *args);
Value *intrinsic_lt(GC *gc, Closure *closure, Value *args);
Value *intrinsic_lt_eq(GC *gc, Closure *closure, Value *args);
Value *intrinsic_gt(GC *gc, Closure *closure, Value *args);
Value *intrinsic_gt_eq(GC *gc, Closure *closure, Value *args);

// List intrinsics
Value *intrinsic_car(GC *gc, Closure *closure, Value *args);
Value *intrinsic_cdr(GC *gc, Closure *closure, Value *args);
Value *intrinsic_cons(GC *gc, Closure *closure, Value *args);
Value *intrinsic_apply(GC *gc, Closure *closure, Value *args);

// Symbol generation
Value *intrinsic_gensym(GC *gc, Closure *closure, Value *args);

// Type predicates
Value *intrinsic_is_cons(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_int(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_float(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_boolean(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_string(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_symbol(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_function(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_macro(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_nil(GC * gc, Closure *closure, Value *args);

// File I/O
Value *intrinsic_load_file(GC *gc, Closure *closure, Value *args);
Value *intrinsic_eval_string(GC *gc, Closure *closure, Value *args);

// Variable access
Value *intrinsic_get_var(GC *gc, Closure *closure, Value *args);

// Purity checking
Value *intrinsic_pure_p(GC *gc, Closure *closure, Value *args);

// Sequencing
Value *intrinsic_begin(GC *gc, Closure *closure, Value *args);

// Type constructors
Value *intrinsic_string(GC *gc, Closure *closure, Value *args);
Value *intrinsic_symbol(GC *gc, Closure *closure, Value *args);

// Module system
Value *intrinsic_module(GC *gc, Closure *closure, Value *args);
Value *intrinsic_list_directory(GC *gc, Closure *closure, Value *args);

// Pattern matching
Value *intrinsic_match(GC *gc, Closure *closure, Value *args);
