#pragma once

#include "types/value.h"

Value *call(Closure *closure, Callable *callable, Value *arguments);

