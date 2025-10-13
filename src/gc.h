#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "types/value.h"

// GC-managed object header
typedef struct GCObject {
    struct GCObject *next;
    bool marked;
    size_t size;
    void *data;  // pointer to the actual Value/Cons/Closure/etc
} GCObject;

// GC state
typedef struct GC {
    GCObject *head;
    size_t num_objects;
    size_t max_objects;  // trigger GC when we hit this
} GC;

// Initialize the garbage collector
void gc_init(GC *gc);

// Allocate a new Value through the GC
Value *gc_alloc_value(GC *gc, ValueType type, ValueData data);

// Allocate other types through GC
Cons *gc_alloc_cons(GC *gc);
Closure *gc_alloc_closure(GC *gc);
Callable *gc_alloc_callable(GC *gc);
UT_string *gc_alloc_string(GC *gc);

// Mark phase: mark all reachable objects starting from roots
void gc_mark_value(Value *value);
void gc_mark_closure(Closure *closure);

// Sweep phase: free all unmarked objects
void gc_sweep(GC *gc);

// Run a full mark-and-sweep cycle
void gc_collect(GC *gc, Closure *root_closure);

// Clean up all GC-managed memory
void gc_free_all(GC *gc);
