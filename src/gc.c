// utarray's out-of-memory hook (default: a silent exit(-1)). It is expanded at
// each use, so it only has to be defined before the utarray macros are used.
#include <stdio.h>
#include <stdlib.h>
#ifndef utarray_oom
#define utarray_oom() do { \
        fputs("lyra: out of memory growing the GC mark stack\n", stderr); \
        abort(); \
    } while (0)
#endif

#include "gc.h"
#include <string.h>
#include "types/list.h"
#include "types/champ.h"
#include "types/boolean.h"
#include "intrinsics.h"

#define GC_INITIAL_THRESHOLD 1000

// Initialize the garbage collector
void gc_init(GC *gc) {
    gc->head = NULL;
    gc->num_objects = 0;
    gc->max_objects = GC_INITIAL_THRESHOLD;
    gc->gensym_counter = 0;
    gc->uninterned_counter = 0;
    static const UT_icd ptr_icd = {sizeof(void *), NULL, NULL, NULL};
    utarray_new(gc->mark_stack, &ptr_icd);
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

// ---- Marking ---------------------------------------------------------------
//
// Marking is an explicit worklist, not recursion, so a long list or a deeply
// nested structure can't overflow the C stack. An object is marked when it is
// pushed, so it enters the stack at most once; the stack's memory belongs to
// the GC and is reused by every collection.

// The few Values that are statics rather than GC objects (no header in front):
// nil, true, false and `missing`. They reference no GC objects.
static bool is_static_value(const Value *v) {
    return v == NIL || v == TRUE || v == FALSE || v == MISSING;
}

// Mark `p` (a GC object, never a static) and queue it for tracing, unless it
// is NULL or already marked.
static void push_object(GC *gc, void *p) {
    if (!p) return;
    GCObject *obj = header_of(p);
    if (obj->marked) return;
    obj->marked = true;

    utarray_push_back(gc->mark_stack, &p);
}

static void push_value(GC *gc, Value *value) {
    if (value && !is_static_value(value)) push_object(gc, value);
}

static void push_champ(GC *gc, ChampNode *node) {
    if (node && !champ_is_static(node)) push_object(gc, node);
}

// Push every object `obj` directly references. This is the one place that
// knows each heap type's layout: a new GC object type needs a case here.
static void gc_trace(GC *gc, void *obj, GCObjectType type) {
    switch (type) {
        case GC_VALUE: {
            Value *value = obj;
            switch (value->type) {
                case CONS:
                    push_object(gc, value->data.as_cons);
                    break;
                case STRING:
                    push_object(gc, value->data.as_string);
                    break;
                case SYMBOL:
                case VARIADIC_MARKER:  // a `*name` parameter; holds its name like a symbol
                    push_object(gc, value->data.as_symbol);
                    break;
                case FUNCTION:
                case MACRO:
                    push_object(gc, value->data.as_function);
                    break;
                case DICT:
                case SET:
                    push_champ(gc, value->data.as_champ);
                    break;
                default:
                    break;
            }
            break;
        }
        case GC_CONS: {
            // cdr first: the stack is LIFO, so the car (usually small) is
            // traced next and a long list's spine doesn't pile up on the stack.
            Cons *cons = obj;
            push_value(gc, cons->cdr);
            push_value(gc, cons->car);
            break;
        }
        case GC_CLOSURE: {
            Closure *closure = obj;
            push_object(gc, closure->parent);
            Variable *var, *tmp;
            HASH_ITER(hh, closure->defs, var, tmp) {
                push_object(gc, var);
            }
            break;
        }
        case GC_VARIABLE: {
            Variable *var = obj;
            push_object(gc, var->key);
            push_value(gc, var->value);
            break;
        }
        case GC_CALLABLE: {
            Callable *callable = obj;
            if (callable->type == DERIVED_FUNCTION || callable->type == DERIVED_MACRO) {
                Derived *derived = &callable->data.as_derived;
                push_object(gc, derived->closure);
                push_value(gc, derived->params);
                push_value(gc, derived->definition);
            }
            break;
        }
        case GC_CHAMP_NODE: {
            ChampNode *node = obj;
            size_t entry_slots = champ_payload_arity(node) * node->stride;
            size_t children = champ_node_arity(node);
            for (size_t i = 0; i < entry_slots; i++) push_value(gc, node->slots[i]);
            for (size_t i = 0; i < children; i++) push_champ(gc, node->slots[entry_slots + i]);
            break;
        }
        case GC_UTSTRING:
            break;  // a leaf
    }
}

// Mark everything reachable from the root closure
static void gc_mark(GC *gc, Closure *root) {
    push_object(gc, root);
    void **top;
    while ((top = (void **)utarray_back(gc->mark_stack)) != NULL) {
        void *obj = *top;
        utarray_pop_back(gc->mark_stack);
        gc_trace(gc, obj, header_of(obj)->type);
    }
    utarray_clear(gc->mark_stack);  // already empty; keeps the buffer for the next collection
}

// Release an object and its header. Only UT_string owns anything beyond its
// own block (its character buffer, released by utstring_done).
static void gc_free_object(GCObject *obj) {
    if (obj->type == GC_UTSTRING) utstring_done((UT_string *)(obj + 1));
    free(obj);
}

// Release the uthash table (buckets, not entries) of every closure that is
// unmarked, or of every closure if `all`. HASH_CLEAR reads the first Variable
// to find the table, and Variables are separate GC objects that can be swept
// before their closure, so this must run while every object is still alive.
static void release_closure_tables(GC *gc, bool all) {
    for (GCObject *obj = gc->head; obj; obj = obj->next) {
        if (obj->type == GC_CLOSURE && (all || !obj->marked)) {
            Closure *closure = (Closure *)(obj + 1);
            HASH_CLEAR(hh, closure->defs);
        }
    }
}

// Sweep phase: free all unmarked objects
void gc_sweep(GC *gc) {
    release_closure_tables(gc, false);
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
    gc_mark(gc, root_closure);

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
    // First pass: release every closure's hash table while all Variables are alive
    release_closure_tables(gc, true);

    // Second pass: free all objects
    GCObject *obj = gc->head;
    while (obj) {
        GCObject *next = obj->next;
        gc_free_object(obj);
        obj = next;
    }
    gc->head = NULL;
    gc->num_objects = 0;
    utarray_free(gc->mark_stack);
    gc->mark_stack = NULL;
}
