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
Value *intrinsic_unique(GC *gc, Closure *closure, Value *args);
Value *intrinsic_expand_macro_1(GC *gc, Closure *closure, Value *args);
Value *intrinsic_expand_macro(GC *gc, Closure *closure, Value *args);

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

// Output
Value *intrinsic_print(GC *gc, Closure *closure, Value *args);
Value *intrinsic_write(GC *gc, Closure *closure, Value *args);
void intrinsic_set_print_stream(FILE *stream);  // NULL restores stdout (used by tests)

// Panics (see panic.h)
Value *intrinsic_panic(GC *gc, Closure *closure, Value *args);
Value *intrinsic_panic_with_status(GC *gc, Closure *closure, Value *args);

// Variable access
Value *intrinsic_get_var(GC *gc, Closure *closure, Value *args);

// Sequencing
Value *intrinsic_begin(GC *gc, Closure *closure, Value *args);

// Type constructors
Value *intrinsic_int(GC *gc, Closure *closure, Value *args);
Value *intrinsic_truncate(GC *gc, Closure *closure, Value *args);
Value *intrinsic_floor(GC *gc, Closure *closure, Value *args);
Value *intrinsic_ceiling(GC *gc, Closure *closure, Value *args);
Value *intrinsic_round(GC *gc, Closure *closure, Value *args);
Value *intrinsic_float(GC *gc, Closure *closure, Value *args);
Value *intrinsic_string(GC *gc, Closure *closure, Value *args);
Value *intrinsic_symbol(GC *gc, Closure *closure, Value *args);

// Whole-file I/O
Value *intrinsic_read_file(GC *gc, Closure *closure, Value *args);
Value *intrinsic_write_file(GC *gc, Closure *closure, Value *args);

// Module system
Value *intrinsic_module(GC *gc, Closure *closure, Value *args);
Value *intrinsic_list_directory(GC *gc, Closure *closure, Value *args);

// Pattern matching
Value *intrinsic_match(GC *gc, Closure *closure, Value *args);

// Dicts and sets (intrinsics_dict.c)
extern Value *MISSING;  // the `missing` symbol: what `get` returns for an absent key
Value *intrinsic_dict(GC *gc, Closure *closure, Value *args);  // special form
Value *intrinsic_set(GC *gc, Closure *closure, Value *args);
Value *intrinsic_get(GC *gc, Closure *closure, Value *args);
Value *intrinsic_assoc(GC *gc, Closure *closure, Value *args);
Value *intrinsic_dissoc(GC *gc, Closure *closure, Value *args);
Value *intrinsic_contains(GC *gc, Closure *closure, Value *args);
Value *intrinsic_count(GC *gc, Closure *closure, Value *args);
Value *intrinsic_set_add(GC *gc, Closure *closure, Value *args);
Value *intrinsic_set_remove(GC *gc, Closure *closure, Value *args);
Value *intrinsic_union(GC *gc, Closure *closure, Value *args);
Value *intrinsic_intersection(GC *gc, Closure *closure, Value *args);
Value *intrinsic_difference(GC *gc, Closure *closure, Value *args);
Value *intrinsic_keys(GC *gc, Closure *closure, Value *args);
Value *intrinsic_values(GC *gc, Closure *closure, Value *args);
Value *intrinsic_entries(GC *gc, Closure *closure, Value *args);
Value *intrinsic_members(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_dict(GC *gc, Closure *closure, Value *args);
Value *intrinsic_is_set(GC *gc, Closure *closure, Value *args);

// Strings (intrinsics_string.c)
int string_compare(Value *a, Value *b);  // bytewise: -1, 0 or 1
Value *intrinsic_byte(GC *gc, Closure *closure, Value *args);
Value *intrinsic_slice(GC *gc, Closure *closure, Value *args);
Value *intrinsic_find(GC *gc, Closure *closure, Value *args);
Value *intrinsic_starts_with(GC *gc, Closure *closure, Value *args);
Value *intrinsic_ends_with(GC *gc, Closure *closure, Value *args);
Value *intrinsic_parse_int(GC *gc, Closure *closure, Value *args);
Value *intrinsic_bytes(GC *gc, Closure *closure, Value *args);
Value *intrinsic_from_bytes(GC *gc, Closure *closure, Value *args);
Value *intrinsic_join(GC *gc, Closure *closure, Value *args);
