#include "gc.h"
#include <stdlib.h>
#include <string.h>

#define GC_INITIAL_THRESHOLD 1000

// Initialize the garbage collector
void gc_init(GC *gc) {
    gc->head = NULL;
    gc->num_objects = 0;
    gc->max_objects = GC_INITIAL_THRESHOLD;
}

// Internal helper: register an object with the GC
static GCObject *gc_register(GC *gc, void *data, size_t size) {
    GCObject *obj = malloc(sizeof(GCObject));
    obj->next = gc->head;
    obj->marked = false;
    obj->size = size;
    obj->data = data;
    gc->head = obj;
    gc->num_objects++;
    return obj;
}

// Allocate a new Value through the GC
Value *gc_alloc_value(GC *gc, ValueType type, ValueData data) {
    Value *value = malloc(sizeof(Value));
    value->type = type;
    value->data = data;
    gc_register(gc, value, sizeof(Value));
    return value;
}

// Allocate other types through GC
Cons *gc_alloc_cons(GC *gc) {
    Cons *cons = malloc(sizeof(Cons));
    cons->car = NULL;
    cons->cdr = NULL;
    gc_register(gc, cons, sizeof(Cons));
    return cons;
}

Closure *gc_alloc_closure(GC *gc) {
    Closure *closure = malloc(sizeof(Closure));
    closure->defs = NULL;
    closure->parent = NULL;
    gc_register(gc, closure, sizeof(Closure));
    return closure;
}

Callable *gc_alloc_callable(GC *gc) {
    Callable *callable = malloc(sizeof(Callable));
    gc_register(gc, callable, sizeof(Callable));
    return callable;
}

UT_string *gc_alloc_string(GC *gc) {
    UT_string *str = NULL;
    utstring_new(str);
    gc_register(gc, str, sizeof(UT_string));
    return str;
}

// Mark phase: recursively mark all reachable objects
void gc_mark_value(Value *value) {
    if (!value) return;

    // Find the GCObject for this value (linear search for now, can optimize later)
    // Note: in a real implementation, we'd store the GCObject pointer with the Value
    // For this simple version, we'll just mark based on pointer

    switch (value->type) {
        case CONS:
            if (value->data.as_cons) {
                gc_mark_value(value->data.as_cons->car);
                gc_mark_value(value->data.as_cons->cdr);
            }
            break;
        case FUNCTION:
        case MACRO:
            if (value->data.as_function) {
                Callable *callable = value->data.as_function;
                if (callable->type == DERIVED_FUNCTION || callable->type == DERIVED_MACRO) {
                    Derived *derived = &callable->data.as_derived;
                    gc_mark_closure(derived->closure);
                    gc_mark_value(derived->params);
                    gc_mark_value(derived->definition);
                }
            }
            break;
        // Other types (INT, FLOAT, BOOLEAN, STRING, SYMBOL) don't contain references
        default:
            break;
    }
}

void gc_mark_closure(Closure *closure) {
    if (!closure) return;

    // Mark all variables in the closure
    Variable *var, *tmp;
    HASH_ITER(hh, closure->defs, var, tmp) {
        gc_mark_value(var->value);
    }

    // Recursively mark parent closure
    gc_mark_closure(closure->parent);
}

// Helper to mark a specific GCObject
static void gc_mark_object(GC *gc, void *data) {
    if (!data) return;

    GCObject *obj = gc->head;
    while (obj) {
        if (obj->data == data) {
            obj->marked = true;
            return;
        }
        obj = obj->next;
    }
}

// Mark all objects reachable from a value
static void gc_mark_from_value(GC *gc, Value *value) {
    if (!value) return;

    gc_mark_object(gc, value);

    switch (value->type) {
        case CONS:
            if (value->data.as_cons) {
                gc_mark_object(gc, value->data.as_cons);
                gc_mark_from_value(gc, value->data.as_cons->car);
                gc_mark_from_value(gc, value->data.as_cons->cdr);
            }
            break;
        case STRING:
            gc_mark_object(gc, value->data.as_string);
            break;
        case SYMBOL:
            gc_mark_object(gc, value->data.as_symbol);
            break;
        case FUNCTION:
        case MACRO:
            if (value->data.as_function) {
                gc_mark_object(gc, value->data.as_function);
                Callable *callable = value->data.as_function;
                if (callable->type == DERIVED_FUNCTION || callable->type == DERIVED_MACRO) {
                    Derived *derived = &callable->data.as_derived;
                    gc_mark_object(gc, derived->closure);
                    gc_mark_from_closure(gc, derived->closure);
                    gc_mark_from_value(gc, derived->params);
                    gc_mark_from_value(gc, derived->definition);
                }
            }
            break;
        default:
            break;
    }
}

// Mark all objects reachable from a closure
static void gc_mark_from_closure(GC *gc, Closure *closure) {
    if (!closure) return;

    gc_mark_object(gc, closure);

    Variable *var, *tmp;
    HASH_ITER(hh, closure->defs, var, tmp) {
        gc_mark_object(gc, var);
        gc_mark_object(gc, var->key);
        gc_mark_from_value(gc, var->value);
    }

    gc_mark_from_closure(gc, closure->parent);
}

// Sweep phase: free all unmarked objects
void gc_sweep(GC *gc) {
    GCObject **obj_ptr = &gc->head;
    while (*obj_ptr) {
        GCObject *obj = *obj_ptr;
        if (!obj->marked) {
            // Remove from list
            *obj_ptr = obj->next;

            // Free the actual data
            free(obj->data);

            // Free the GCObject wrapper
            free(obj);
            gc->num_objects--;
        } else {
            // Unmark for next GC cycle
            obj->marked = false;
            obj_ptr = &obj->next;
        }
    }
}

// Run a full mark-and-sweep cycle
void gc_collect(GC *gc, Closure *root_closure) {
    // Mark phase: mark all reachable objects from roots
    gc_mark_from_closure(gc, root_closure);

    // Sweep phase: free unmarked objects
    gc_sweep(gc);

    // Adjust threshold for next collection
    gc->max_objects = gc->num_objects * 2;
    if (gc->max_objects < GC_INITIAL_THRESHOLD) {
        gc->max_objects = GC_INITIAL_THRESHOLD;
    }
}

// Clean up all GC-managed memory
void gc_free_all(GC *gc) {
    GCObject *obj = gc->head;
    while (obj) {
        GCObject *next = obj->next;
        free(obj->data);
        free(obj);
        obj = next;
    }
    gc->head = NULL;
    gc->num_objects = 0;
}
