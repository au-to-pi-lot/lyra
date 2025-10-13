#pragma once

#include "types/value.h"

Closure *make_closure(GC *gc, Closure *parent);

void set_var(Closure *closure, UT_string *key, Value *value);

Value *get_var(Closure *closure, UT_string *key);

void del_var(Closure *closure, UT_string *key);
