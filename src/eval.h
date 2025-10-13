#pragma once

#include "types/value.h"
#include "closure.h"

Value *evaluate(Closure *closure, Value *expr);