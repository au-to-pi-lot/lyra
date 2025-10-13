#pragma once

#include "types/value.h"

Callable *make_macro(Closure *parent_closure, Cons *params, Cons *body);

Value *exec_macro(Callable *macro, Cons *args);