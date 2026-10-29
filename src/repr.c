#include "repr.h"
#include "types/value.h"
#include "types/list.h"


static UT_string *repr_internal(Value *val, UT_string *out);

UT_string *repr_string(UT_string *s, UT_string *out) {
    utstring_printf(out, "\"");
    char *body = utstring_body(s);
    for (long unsigned int i = 0; i < utstring_len(s); i++) {
        char c = body[i];
        switch (c) {
            case '\n':
                utstring_printf(out, "\\n");
                break;
            case '\t':
                utstring_printf(out, "\\t");
                break;
            case '\r':
                utstring_printf(out, "\\r");
                break;
            case '\\':
                utstring_printf(out, "\\\\");
                break;
            case '"':
                utstring_printf(out, "\\\"");
                break;
            default:
                utstring_printf(out, "%c", c);
                break;
        }
    }

    utstring_printf(out, "\"");
    return out;
}

UT_string *repr_list(Value *list, UT_string *out) {
    utstring_printf(out, "(");
    
    for (Value *item = list_head(list); !is_nil(list); list = list_tail(list), item = list_head(list)) {
        repr_internal(item, out);
        if (!is_nil(list_tail(list))) {
            utstring_printf(out, " ");
        }
    }

    utstring_printf(out, ")");
    return out;
}

static UT_string *repr_internal(Value *val, UT_string *out) {
    switch (val->type) {
        case CONS:
            repr_list(val, out);
            break;
        case INT:
            utstring_printf(out, "%d", val->data.as_int);
            break;
        case FLOAT:
            // Whole numbers keep a ".0" so floats stay distinguishable from ints.
            // (The range check also keeps the comparison away from NaN/inf and
            // from values too large for an int, where the old cast was UB.)
            if (val->data.as_float > -1e9f && val->data.as_float < 1e9f && val->data.as_float == (float)(long long)val->data.as_float) {
                utstring_printf(out, "%.1f", val->data.as_float);  // "3.0"
            } else {
                utstring_printf(out, "%g", val->data.as_float);    // "3.14159" (strips trailing zeros)
            }
            break;
        case STRING:
            repr_string(val->data.as_string, out);
            break;
        case SYMBOL:
            utstring_concat(out, val->data.as_symbol);
            break;
        case BOOLEAN:
            utstring_printf(out, "%s", val->data.as_boolean ? "true" : "false");
            break;
        case FUNCTION:
            utstring_printf(out, "<<fn 0x%lx>>", (uintptr_t)val->data.as_function);
            break;
        case MACRO:
            utstring_printf(out, "<<macro 0x%lx>>", (uintptr_t)val->data.as_function);
            break;
        case VARIADIC_MARKER:
            utstring_printf(out, "*%s", utstring_body(val->data.as_symbol));
            break;
        default:
            break;
    }

    return out;
}

UT_string *repr(GC *gc, Value *val) {
    UT_string *result = gc_alloc_string(gc);
    return repr_internal(val, result);
}