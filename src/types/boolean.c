#include "boolean.h"
#include <stdbool.h>
#include "value.h"

Value TRUE_VALUE = {.type = BOOLEAN, .data = {.as_boolean = true}};
Value FALSE_VALUE = {.type = BOOLEAN, .data = {.as_boolean = false}};

Value *TRUE = &TRUE_VALUE;
Value *FALSE = &FALSE_VALUE;

Value *boolean_not(Value *boolean) {
    return boolean->data.as_boolean ? FALSE : TRUE;
}

Value *boolean_and(Value *left, Value *right) {
    return left->data.as_boolean ? right : FALSE;
}

Value *boolean_or(Value *left, Value *right) {
    return !left->data.as_boolean ? right : TRUE;
}
