#pragma once

#include "types/value.h"

Value *call(GC *gc, Closure *closure, Callable *callable, Value *arguments);

