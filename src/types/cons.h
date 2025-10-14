#pragma once

#include "value.h"
#include "../gc.h"

Cons *make_cons(GC *gc, Value *car, Value *cdr);
