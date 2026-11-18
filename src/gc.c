#include "gc.h"
#include <stdlib.h>
#include <string.h>
#include "types/list.h"
#include "types/champ.h"

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

// Internal helper: register an object with the GC
GCObject *gc_register(GC *gc, void *data, size_t size, GCObjectType type) {
    GCObject *obj = malloc(sizeof(GCObject));
    obj->next = gc->head;
    obj->marked = false;
    obj->size = size;
    obj->type = type;
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
    gc_register(gc, value, sizeof(Value), GC_VALUE);
    return value;
}

Cons *gc_alloc_cons(GC *gc) {
    Cons *cons = malloc(sizeof(Cons));
    // Start as (nil . nil) so a cons whose car is still being computed when
    // a panic unwinds never holds garbage pointers.
    cons->car = NIL;
    cons->cdr = NIL;
    gc_register(gc, cons, sizeof(Cons), GC_CONS);
    return cons;
}

Closure *gc_alloc_closure(GC *gc) {
    Closure *closure = malloc(sizeof(Closure));
    closure->defs = NULL;
    closure->parent = NULL;
    gc_register(gc, closure, sizeof(Closure), GC_CLOSURE);
    return closure;
}

Callable *gc_alloc_callable(GC *gc) {
    Callable *callable = malloc(sizeof(Callable));
    gc_register(gc, callable, sizeof(Callable), GC_CALLABLE);
    return callable;
}

Variable *gc_alloc_variable(GC *gc) {
    Variable *var = malloc(sizeof(Variable));
    gc_register(gc, var, sizeof(Variable), GC_VARIABLE);
    return var;
}

UT_string *gc_alloc_string(GC *gc) {
    UT_string *str = NULL;
    utstring_new(str);
    gc_register(gc, str, sizeof(UT_string), GC_UTSTRING);
    return str;
}

ChampNode *gc_alloc_champ_node(GC *gc, size_t nslots) {
    size_t size = sizeof(ChampNode) + nslots * sizeof(void *);
    ChampNode *node = malloc(size);
    // The node keeps a pointer to its own header, so marking a trie doesn't
    // search the object list once per node (see gc_mark_champ).
    node->header = gc_register(gc, node, size, GC_CHAMP_NODE);
    return node;
}

// Helper to mark a specific GCObject
// Returns true if the object was newly marked, false if already marked
static bool gc_mark_object(GC *gc, void *data) {
    if (!data) return false;

    GCObject *obj = gc->head;
    while (obj != NULL) {
        if (obj->data == data) {
            if (obj->marked) {
                return false;  // Already marked, don't recurse
            }
            obj->marked = true;
            return true;  // Newly marked, should recurse
        }
        obj = obj->next;
    }
    return false;  // Not found in GC
}

// Mark a dict/set trie node, its entries and its children. The static empty
// tries have no header and nothing to trace.
static void gc_mark_champ(GC *gc, ChampNode *node) {
    if (!node || !node->header || node->header->marked) return;
    node->header->marked = true;

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
    if (!gc_mark_object(gc, value)) {
        return;
    }

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
        case VARIADIC_MARKER:  // a `*name` parameter; holds its name like a symbol
            gc_mark_object(gc, value->data.as_symbol);
            break;
        case FUNCTION:
        case MACRO:
            if (value->data.as_function) {
                gc_mark_object(gc, value->data.as_function);
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
    if (!gc_mark_object(gc, closure)) {
        return;
    }

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

            // Call appropriate destructor based on type
            switch (obj->type) {
                case GC_UTSTRING:
                    // utstring manages its own internal buffer - need to free it properly
                    utstring_free((UT_string*)obj->data);
                    break;
                case GC_VARIABLE:
                case GC_VALUE:
                case GC_CONS:
                case GC_CLOSURE:
                case GC_CALLABLE:
                case GC_CHAMP_NODE:  // one malloc block, slots included
                    // These types don't have special cleanup requirements
                    free(obj->data);
                    break;
            }

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
    // First pass: clean up hash tables in closures (uthash internal memory)
    GCObject *obj = gc->head;
    while (obj) {
        if (obj->type == GC_CLOSURE) {
            Closure *closure = (Closure*)obj->data;
            Variable *var, *tmp;
            HASH_ITER(hh, closure->defs, var, tmp) {
                HASH_DEL(closure->defs, var);
            }
        }
        obj = obj->next;
    }

    // Second pass: free all objects
    obj = gc->head;
    while (obj) {
        GCObject *next = obj->next;

        // Call appropriate destructor based on type
        switch (obj->type) {
            case GC_UTSTRING:
                utstring_free((UT_string*)obj->data);
                break;
            case GC_CLOSURE:
                // Hash table already cleaned up in first pass
                free(obj->data);
                break;
            case GC_VALUE:
            case GC_CONS:
            case GC_CALLABLE:
            case GC_VARIABLE:
            case GC_CHAMP_NODE:
                free(obj->data);
                break;
        }

        free(obj);
        obj = next;
    }
    gc->head = NULL;
    gc->num_objects = 0;
}
