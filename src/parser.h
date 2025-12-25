#pragma once

#include <utstring.h>
#include "types/value.h"

// Token types
typedef enum TokenType {
    TOKEN_LPAREN,                    // (
    TOKEN_RPAREN,                    // )
    TOKEN_NUMBER,                    // 123, 3.14
    TOKEN_STRING,                    // "hello"
    TOKEN_SYMBOL,                    // foo, +, lambda
    TOKEN_QUOTE,                     // '
    TOKEN_QUASIQUOTE_BACKTICK,       // `
    TOKEN_UNQUOTE_COMMA,             // ,
    TOKEN_UNQUOTE_SPLICING_COMMA_AT, // ,@
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
// If consumed is not NULL, it will be set to the number of characters consumed
Value* parse(GC *gc, const char* input);
Value* parse_with_pos(GC *gc, const char* input, size_t *consumed);

// Parse from UT_string for compatibility
Value* parse_utstring(GC *gc, UT_string* str);
