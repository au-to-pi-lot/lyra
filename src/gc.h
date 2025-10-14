#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "types/value.h"



// Initialize the garbage collector
void gc_init(GC *gc);

// Allocate a new Value through the GC
Value *gc_alloc_value(GC *gc, ValueType type, ValueData data);
Cons *gc_alloc_cons(GC *gc);
Closure *gc_alloc_closure(GC *gc);
Callable *gc_alloc_callable(GC *gc);
Variable *gc_alloc_variable(GC *gc);
UT_string *gc_alloc_string(GC *gc);

// Sweep phase: free all unmarked objects
void gc_sweep(GC *gc);

// Run a full mark-and-sweep cycle
void gc_collect(GC *gc, Closure *root_closure);

// Clean up all GC-managed memory
void gc_free_all(GC *gc);
