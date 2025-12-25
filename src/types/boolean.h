#pragma once

#include "value.h"

extern Value TRUE_VALUE;
extern Value FALSE_VALUE;

extern Value *TRUE;
extern Value *FALSE;

Value *truthiness(Value *value);

Value *boolean_not(Value *boolean);

Value *boolean_and(Value *left, Value *right);

Value *boolean_or(Value *left, Value *right);