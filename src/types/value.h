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

/// @struct Cons
/// @brief Cons is a single link of an immutable List. 
/// 
/// Implemented here in Lisp tradition, with the convention that
/// `car` point toward the head item of the list, while `cdr` point toward the next
/// cons cell in the list. This is terminated when `cdr` points to nil.
struct Cons {
    /// @brief Pointer to head of list
    Value *car;
    /// @brief Pointer to tail of list
    Value *cdr;
};

/// @struct Variable
/// @brief Variable is an item in a Closure's hash table.
struct Variable {
    /// @brief The variable's name. 
    UT_string *key;
    /// @brief The variable's value
    Value *value;
    /// @brief Mandatory struct member for uthash items
    UT_hash_handle hh;
};

/// @struct Closure
/// @brief Storage mechanism for variable definitions.
///
/// Each Function call creates a Closure capturing the definitions inside the function.
/// The closures form a linked list back to the global closure with lexical binding semantics.
struct Closure {
    Variable *defs;
    Closure *parent;
};

/// @brief C lang definition of a function or macro. 
///
/// @param gc The interpreter's garbage collector
/// @param closure The closure in which it is evaluated
/// @param args The arguments provided when the intrinsic was called
typedef Value *(*Intrinsic)(GC *gc, Closure *closure, Value *args);

/// @brief A Lyra function or macro.
struct Derived {
    /// @brief The function's environment of definition. This is used to implement lexical variable binding.
    Closure *closure;
    /// @brief A List of parameter tokens.
    Value *params;
    /// @brief S-expression function definition.
    Value *definition;
};

/// @brief Discriminated union over CallableType.
struct Callable {
    CallableType type;
    union {
        Intrinsic as_intrinsic;
        Derived as_derived;
    } data;
};

/// @brief Union accessors for Value.
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

/// @brief 
struct Value {
    ValueType type;
    ValueData data;
};