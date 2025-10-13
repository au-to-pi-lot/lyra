#include "closure.h"
#include "gc.h"

Closure *make_closure(GC *gc, Closure *parent) {
    Closure *result = gc_alloc_closure(gc);
    result->parent = parent;

    return result;
}

void set_var(GC *gc, Closure *closure, UT_string *key, Value *value) {
    Variable *def;

    HASH_FIND(hh, closure->defs, utstring_body(key), utstring_len(key), def);
    if (def == NULL) {
        def = (Variable *)malloc(sizeof(Variable));
        def->key = key;

        HASH_ADD_KEYPTR(hh, closure->defs, utstring_body(key), utstring_len(key), def);
    }

    def->value = value;
}

Value *get_var(Closure *closure, UT_string *key) {
    if (closure == NULL) {
        return NULL;
    }

    Variable *def;

    HASH_FIND(hh, closure->defs, utstring_body(key), utstring_len(key), def);
    if (def == NULL) {
        return get_var(closure->parent, key);
    }

    return def->value;
}

void del_var(Closure *closure, UT_string *key) {
    Variable *def;
    HASH_FIND(hh, closure->defs, utstring_body(key), utstring_len(key), def);

    if (def == NULL) {
        return;
    }

    HASH_DELETE(hh, closure->defs, def);
}
