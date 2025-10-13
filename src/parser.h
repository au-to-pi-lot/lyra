#pragma once

#include <utstring.h>

#include "types/value.h"

// Token types
typedef enum {
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_NUMBER,      // 123, 3.14
    TOKEN_STRING,      // "hello"
    TOKEN_SYMBOL,      // foo, +, lambda
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    UT_string *value;       // Owned string (must be freed)
    size_t line;
    size_t column;
} Token;

// Lexer state
typedef struct {
    const char *input;
    size_t pos;
    size_t line;
    size_t column;
} Lexer;

// Parse a string into a Value
Value* parse(const char* input);

// Parse from UT_string for compatibility
Value* parse_utstring(UT_string* str);
