#include "value.h"

Value *make_symbol(GC *gc, UT_string *name);

Value *symbol_from_char(GC *gc, char *name);

Value *make_variadic_marker(GC *gc, UT_string *name);