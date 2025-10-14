#pragma once

#include <stdbool.h>
#include <utstring.h>
#include <uthash.h>

typedef struct Value Value;
typedef struct Cons Cons;
typedef struct Closure Closure;
typedef struct Variable Variable;
typedef struct Derived Derived;
typedef struct Callable Callable;

typedef enum ValueType {
    CONS,
    INT,
    FLOAT,
    BOOLEAN,
    STRING,
    SYMBOL,
    FUNCTION,
    MACRO
} ValueType;

typedef enum CallableType{
    INTRINSIC_FUNCTION,
    INTRINSIC_MACRO,
    DERIVED_FUNCTION,
    DERIVED_MACRO
} CallableType;

// Object types for GC (so we know how to free them properly)
typedef enum GCObjectType {
    GC_VALUE,
    GC_CONS,
    GC_CLOSURE,
    GC_CALLABLE,
    GC_VARIABLE,
    GC_UTSTRING
} GCObjectType;

// GC-managed object header
typedef struct GCObject {
    struct GCObject *next;
    bool marked;
    size_t size;
    GCObjectType type;
    void *data;  // pointer to the actual Value/Cons/Closure/etc
} GCObject;

// GC state
typedef struct GC {
    GCObject *head;
    size_t num_objects;
    size_t max_objects;  // trigger GC when we hit this
} GC;


struct Cons {
    Value *car;
    Value *cdr;
};

struct Variable {
    UT_string *key;
    Value *value;
    UT_hash_handle hh;
};

struct Closure {
    Variable *defs;
    Closure *parent;
};

typedef Value *(*Intrinsic)(GC *gc, Closure *closure, Value *args);

struct Derived {
    Closure *closure;
    Value *params;
    Value *definition;
};

struct Callable {
    CallableType type;
    union {
        Intrinsic as_intrinsic;
        Derived as_derived;
    } data;
};

typedef union ValueData {
    Cons *as_cons;
    int as_int;
    float as_float;
    bool as_boolean;
    UT_string *as_string;
    UT_string *as_symbol;
    Callable *as_function;
    Callable *as_macro;
} ValueData;

struct Value {
    ValueType type;
    ValueData data;
};