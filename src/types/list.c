#include "list.h"
#include "cons.h"
#include "value.h"
#include "../gc.h"

Value NIL_VALUE = {.type = CONS, .data = {.as_cons = NULL}};
Value *NIL = &NIL_VALUE;

int list_length(Value *list) {
    int length = 0;
    Value *current = list;

    while (current->data.as_cons != NULL) {
        length += 1;
        current = current->data.as_cons->cdr;
    }

    return length;
}

bool is_nil(Value *val) {
    return val == &NIL_VALUE;
}

Value *list_append(GC *gc, Value *item, Value *list) {
    Cons *cons = gc_alloc_cons(gc);

    cons->car = item;
    cons->cdr = list;

    Value *val = gc_alloc_value(gc, CONS, (ValueData){.as_cons = cons});

    return val;
}

Value *list_range(GC *gc, int stop) {
    Value *result = NIL;
    for (int i = stop - 1; i >= 0; i--) {
        result = list_append(gc, gc_alloc_value(gc, INT, (ValueData){.as_int = i}), result);
    }

    return result;
}

Value *make_list(GC *gc, int length) {
    Value *result = NIL;

    for(int i = 0; i < length; i++) {
        result = list_append(gc, NIL, result);
    }

    return result;
}

Value *list_head(Value *list) {
    if (is_nil(list)) {
        return NIL;
    }

    return list->data.as_cons->car;
}

Value *list_tail(Value *list) {
    if (is_nil(list)) {
        return list;
    }

    return list->data.as_cons->cdr;
}

Value *list_drop(Value *list, int n) {
    Value *result = list;
    
    for (int i = 0; i < n && !is_nil(result); i++) {
        result = list_tail(result);
    }

    return result;
}

// Internal helper: shallow copy up to max_items elements (or all if max_items < 0)
// Builds list in single forward pass for O(n) performance
static ListWithEnd list_copy_internal(GC *gc, Value *list, int max_items) {
    if (is_nil(list)) {
        return (ListWithEnd){
            .list = list,
            .end = list
        };
    }

    if (max_items == 0) {
        Value *nil = NIL;
        return (ListWithEnd){
            .list = nil,
            .end = nil
        };
    }

    Value *result = NULL;
    Value *end = NULL;
    Value *current = list;
    int count = 0;

    do {
        Cons *cons = gc_alloc_cons(gc);
        cons->car = current->data.as_cons->car;
        Value *val = gc_alloc_value(gc, CONS, (ValueData){.as_cons = cons});

        if (result == NULL) {
            result = end = val;
        } else {
            end->data.as_cons->cdr = val;
            end = val;
        }
        current = current->data.as_cons->cdr;
        count++;
    } while (!is_nil(current) && (max_items < 0 || count < max_items));

    end->data.as_cons->cdr = NIL;

    return (ListWithEnd){
        .list = result,
        .end = end
    };
}

Value *list_keep(GC *gc, Value *list, int n) {
    return list_copy_internal(gc, list, n).list;
}

Value *list_copy(GC *gc, Value *list) {
    return list_copy_internal(gc, list, -1).list;
}

Value *list_index(Value *list, int index) {
    Value *current = list;

    for (int i = 0; i < index; i++) {
        if (is_nil(current)) {
            break;
        }
        current = current->data.as_cons->cdr;
    }

    return list_head(current);
}

Value *list_concat(GC *gc, Value *left, Value *right) {
    if (is_nil(right)) {
        return left;
    }

    if (is_nil(left)) {
        return right;
    }

    ListWithEnd left_copy = list_copy_internal(gc, left, -1);
    left_copy.end->data.as_cons->cdr = right;

    return left_copy.list;
}

Value *list_reverse(GC *gc, Value *list) {
    Value *result = NIL;
    Value *current = list;

    while (!is_nil(current)) {
        result = list_append(gc, list_head(current), result);
        current = list_tail(current);
    }

    return result;
}

Value *list_map(GC *gc, Value *(*func)(Value *item), Value *list) {
    Value *copy = list_copy(gc, list);

    for (Value **item = &copy->data.as_cons->car; !is_nil(copy); copy = list_tail(copy), item = &copy->data.as_cons->car) {
        *item = func(*item);
    }

    return copy;
}

Value *list_fold(GC *gc, Value *(*func)(GC *gc, Value *accumulator, Value *item), Value *list, Value *start) {
    Value *accumulator = start;
    Value *current = list;

    while(!is_nil(current)) {
        accumulator = func(gc, accumulator, list_head(current));
        current = list_tail(current);
    }

    return accumulator;
}

Value *list_filter(GC *gc, bool (*func)(Value *item), Value *list) {

}
