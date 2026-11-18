#include "gc.h"
#include <stdlib.h>
#include <string.h>
#include "types/list.h"
#include "types/champ.h"
#include "types/boolean.h"
#include "intrinsics.h"

#define GC_INITIAL_THRESHOLD 1000

static void gc_mark_from_closure(GC *gc, Closure *closure);
void gc_mark_from_value(GC *gc, Value *value);

// Initialize the garbage collector
void gc_init(GC *gc) {
    gc->head = NULL;
    gc->num_objects = 0;
    gc->max_objects = GC_INITIAL_THRESHOLD;
    gc->gensym_counter = 0;
    gc->uninterned_counter = 0;
}

// Every GC object is one malloc block: a GCObject header immediately followed
// by the object itself, so the header of object `p` is `(GCObject *)p - 1`.
static GCObject *header_of(void *p) {
    return (GCObject *)p - 1;
}

// Allocate `size` bytes of GC object, registered under `type`. Returns the
// object, which starts right after its header.
static void *gc_alloc_block(GC *gc, size_t size, GCObjectType type) {
    GCObject *obj = malloc(sizeof(GCObject) + size);
    obj->next = gc->head;
    obj->marked = false;
    obj->size = size;
    obj->type = type;
    gc->head = obj;
    gc->num_objects++;
    return obj + 1;
}

// Allocate a new Value through the GC
Value *gc_alloc_value(GC *gc, ValueType type, ValueData data) {
    Value *value = gc_alloc_block(gc, sizeof(Value), GC_VALUE);
    value->type = type;
    value->data = data;
    return value;
}

Cons *gc_alloc_cons(GC *gc) {
    Cons *cons = gc_alloc_block(gc, sizeof(Cons), GC_CONS);
    // Start as (nil . nil) so a cons whose car is still being computed when
    // a panic unwinds never holds garbage pointers.
    cons->car = NIL;
    cons->cdr = NIL;
    return cons;
}

Closure *gc_alloc_closure(GC *gc) {
    Closure *closure = gc_alloc_block(gc, sizeof(Closure), GC_CLOSURE);
    closure->defs = NULL;
    closure->parent = NULL;
    return closure;
}

Callable *gc_alloc_callable(GC *gc) {
    return gc_alloc_block(gc, sizeof(Callable), GC_CALLABLE);
}

Variable *gc_alloc_variable(GC *gc) {
    return gc_alloc_block(gc, sizeof(Variable), GC_VARIABLE);
}

UT_string *gc_alloc_string(GC *gc) {
    // Not utstring_new: that mallocs the UT_string itself. Initialize it in
    // place instead; sweep releases its buffer with utstring_done.
    UT_string *str = gc_alloc_block(gc, sizeof(UT_string), GC_UTSTRING);
    utstring_init(str);
    return str;
}

ChampNode *gc_alloc_champ_node(GC *gc, size_t nslots) {
    return gc_alloc_block(gc, sizeof(ChampNode) + nslots * sizeof(void *), GC_CHAMP_NODE);
}

// The few Values that are statics rather than GC objects (no header in front):
// nil, true, false and `missing`. They hold no references to GC objects.
static bool is_static_value(const Value *v) {
    return v == NIL || v == TRUE || v == FALSE || v == MISSING;
}

// Mark `p` (a GC object, not a static); true if it was newly marked
static bool gc_mark_object(void *p) {
    if (!p) return false;
    GCObject *obj = header_of(p);
    if (obj->marked) return false;
    obj->marked = true;
    return true;
}

// Mark a dict/set trie node, its entries and its children. The static empty
// tries have no header and nothing to trace.
static void gc_mark_champ(GC *gc, ChampNode *node) {
    if (!node || champ_is_static(node) || !gc_mark_object(node)) return;

    size_t entry_slots = champ_payload_arity(node) * node->stride;
    size_t children = champ_node_arity(node);
    for (size_t i = 0; i < entry_slots; i++) {
        gc_mark_from_value(gc, node->slots[i]);
    }
    for (size_t i = 0; i < children; i++) {
        gc_mark_champ(gc, node->slots[entry_slots + i]);
    }
}

// Mark all objects reachable from a value
void gc_mark_from_value(GC *gc, Value *value) {
    if (!value) return;

    // Mark this value; if already marked, stop recursing
    if (is_static_value(value) || !gc_mark_object(value)) {
        return;
    }

    switch (value->type) {
        case CONS:
            if (value->data.as_cons) {
                gc_mark_object(value->data.as_cons);
                gc_mark_from_value(gc, value->data.as_cons->car);
                gc_mark_from_value(gc, value->data.as_cons->cdr);
            }
            break;
        case STRING:
            gc_mark_object(value->data.as_string);
            break;
        case SYMBOL:
        case VARIADIC_MARKER:  // a `*name` parameter; holds its name like a symbol
            gc_mark_object(value->data.as_symbol);
            break;
        case FUNCTION:
        case MACRO:
            if (value->data.as_function) {
                gc_mark_object(value->data.as_function);
                Callable *callable = value->data.as_function;
                if (callable->type == DERIVED_FUNCTION || callable->type == DERIVED_MACRO) {
                    Derived *derived = &callable->data.as_derived;
                    gc_mark_from_closure(gc, derived->closure);
                    gc_mark_from_value(gc, derived->params);
                    gc_mark_from_value(gc, derived->definition);
                }
            }
            break;
        case DICT:
        case SET:
            gc_mark_champ(gc, value->data.as_champ);
            break;
        default:
            break;
    }
}

// Mark all objects reachable from a closure
static void gc_mark_from_closure(GC *gc, Closure *closure) {
    if (!closure) return;

    // Mark this closure; if already marked, stop recursing
    if (!gc_mark_object(closure)) {
        return;
    }

    Variable *var, *tmp;
    HASH_ITER(hh, closure->defs, var, tmp) {
        gc_mark_object(var);
        gc_mark_object(var->key);
        gc_mark_from_value(gc, var->value);
    }

    gc_mark_from_closure(gc, closure->parent);
}

// Release an object and its header. Only UT_string owns anything beyond its
// own block (its character buffer, released by utstring_done).
static void gc_free_object(GCObject *obj) {
    if (obj->type == GC_UTSTRING) utstring_done((UT_string *)(obj + 1));
    free(obj);
}

// Sweep phase: free all unmarked objects
void gc_sweep(GC *gc) {
    GCObject **obj_ptr = &gc->head;
    while (*obj_ptr) {
        GCObject *obj = *obj_ptr;
        if (!obj->marked) {
            *obj_ptr = obj->next;
            gc_free_object(obj);
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
    // First pass: clean up hash tables in closures (uthash internal memory)
    for (GCObject *obj = gc->head; obj; obj = obj->next) {
        if (obj->type == GC_CLOSURE) {
            Closure *closure = (Closure *)(obj + 1);
            Variable *var, *tmp;
            HASH_ITER(hh, closure->defs, var, tmp) {
                HASH_DEL(closure->defs, var);
            }
        }
    }

    // Second pass: free all objects
    GCObject *obj = gc->head;
    while (obj) {
        GCObject *next = obj->next;
        gc_free_object(obj);
        obj = next;
    }
    gc->head = NULL;
    gc->num_objects = 0;
}
