#include "parser.h"
#include "types/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Helper to create tokens
static Token make_token(TokenType type, UT_string *value, size_t line, size_t column) {
    Token tok;
    tok.type = type;
    if (value != NULL) {
        utstring_new(tok.value);
        utstring_concat(tok.value, value);
    } else {
        tok.value = NULL;
    }
    tok.line = line;
    tok.column = column;
    return tok;
}

static void free_token(Token* tok) {
    if (tok->value != NULL) {
        utstring_free(tok->value);
        tok->value = NULL;
    }
}

// Initialize lexer
static Lexer make_lexer(const char* input) {
    Lexer lex;
    lex.input = input;
    lex.pos = 0;
    lex.line = 1;
    lex.column = 1;
    return lex;
}

// Peek current character
static char peek(Lexer* lex) {
    return lex->input[lex->pos];
}

// Advance one character
static char advance(Lexer* lex) {
    char c = lex->input[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

// Skip whitespace and comments
static void skip_whitespace(Lexer* lex) {
    while (true) {
        char c = peek(lex);
        if (isspace(c)) {
            advance(lex);
        } else if (c == ';') {
            // Line comment - skip until newline
            while (peek(lex) != '\0' && peek(lex) != '\n') {
                advance(lex);
            }
        } else {
            break;
        }
    }
}

// Lex a string literal
static Token lex_string(Lexer* lex) {
    size_t start_line = lex->line;
    size_t start_col = lex->column;

    advance(lex); // Skip opening "

    UT_string *buffer;
    utstring_new(buffer);

    while (peek(lex) != '"' && peek(lex) != '\0') {
        char c = advance(lex);
        if (c == '\\' && peek(lex) != '\0') {
            // Handle escape sequences
            char next = advance(lex);
            switch (next) {
                case 'n': utstring_printf(buffer, "\n"); break;
                case 't': utstring_printf(buffer, "\t"); break;
                case 'r': utstring_printf(buffer, "\r"); break;
                case '\\': utstring_printf(buffer, "\\"); break;
                case '"': utstring_printf(buffer, "\""); break;
                default: utstring_printf(buffer, "%c", c); break;
            }
        } else {
            utstring_printf(buffer, "%c", c);
        }
    }

    if (peek(lex) == '"') {
        advance(lex); // Skip closing "
    }

    return make_token(TOKEN_STRING, buffer, start_line, start_col);
}

// Lex a number
static Token lex_number(Lexer* lex) {
    size_t start_line = lex->line;
    size_t start_col = lex->column;

    UT_string *buffer;
    utstring_new(buffer);

    // Handle negative numbers
    if (peek(lex) == '-') {
        utstring_printf(buffer, "%c", advance(lex));
    }

    while (isdigit(peek(lex)) || peek(lex) == '.') {
        utstring_printf(buffer, "%c", advance(lex));
    }

    return make_token(TOKEN_NUMBER, buffer, start_line, start_col);
}

// Lex a symbol/identifier
static Token lex_symbol(Lexer* lex) {
    size_t start_line = lex->line;
    size_t start_col = lex->column;

    UT_string *buffer;
    utstring_new(buffer);


    // Symbols can contain letters, digits, and special chars like +, -, *, /, etc.
    while (peek(lex) != '\0' && !isspace(peek(lex)) &&
           peek(lex) != '(' && peek(lex) != ')' && peek(lex) != '"') {
        utstring_printf(buffer, "%c", advance(lex));
    }

    return make_token(TOKEN_SYMBOL, buffer, start_line, start_col);
}

// Get next token
static Token next_token(Lexer* lex) {
    skip_whitespace(lex);

    size_t line = lex->line;
    size_t col = lex->column;
    char c = peek(lex);

    if (c == '\0') {
        return make_token(TOKEN_EOF, NULL, line, col);
    }

    if (c == '(') {
        advance(lex);
        UT_string *data;
        utstring_new(data);
        utstring_printf(data, "(");
        return make_token(TOKEN_LPAREN, data, line, col);
    }

    if (c == ')') {
        advance(lex);
        UT_string *data;
        utstring_new(data);
        utstring_printf(data, ")");
        return make_token(TOKEN_RPAREN, data, line, col);
    }

    if (c == '"') {
        return lex_string(lex);
    }

    // Number: starts with digit or negative sign followed by digit
    if (isdigit(c) || (c == '-' && isdigit(lex->input[lex->pos + 1]))) {
        return lex_number(lex);
    }

    // Otherwise it's a symbol
    return lex_symbol(lex);
}

// Forward declarations for parser
static Value* parse_expr(Lexer* lex, Token* current);
static Value* parse_list(Lexer* lex);

// Parse an atom (number, string, or symbol)
static Value* parse_atom(Token* tok) {
    Value* val = malloc(sizeof(Value));

    switch (tok->type) {
        case TOKEN_NUMBER: {
            // Check if it's a float or int
            if (utstring_find(tok->value, 0, ".", 1) >= 0) {
                val->type = FLOAT;
                val->data.as_float = atof(utstring_body(tok->value));
            } else {
                val->type = INT;
                val->data.as_int = atoi(utstring_body(tok->value));
            }
            break;
        }

        case TOKEN_STRING:
            val->type = STRING;
            utstring_new(val->data.as_string);
            utstring_concat(val->data.as_string, tok->value);
            break;

        case TOKEN_SYMBOL:
            val->type = SYMBOL;
            utstring_new(val->data.as_symbol);
            utstring_concat(val->data.as_symbol, tok->value);
            break;

        default:
            fprintf(stderr, "Unexpected token type in parse_atom\n");
            free(val);
            return NULL;
    }

    return val;
}

// Parse a list: ( expr1 expr2 ... )
static Value* parse_list(Lexer* lex) {
    // We've already seen the '(', build a list of values then reverse it
    Value* result = NIL;

    while (true) {
        Token tok = next_token(lex);

        if (tok.type == TOKEN_RPAREN) {
            free_token(&tok);
            break;
        }

        if (tok.type == TOKEN_EOF) {
            free_token(&tok);
            fprintf(stderr, "Unexpected EOF while parsing list\n");
            return NULL;
        }

        Value* elem = parse_expr(lex, &tok);
        if (!elem) {
            return NULL;
        }

        result = list_append(elem, result);
    }

    return list_reverse(result);
}

// Parse a single expression
static Value* parse_expr(Lexer* lex, Token* current) {
    if (current->type == TOKEN_LPAREN) {
        free_token(current);
        return parse_list(lex);
    } else {
        Value* val = parse_atom(current);
        free_token(current);
        return val;
    }
}

// Main parse function
Value* parse(const char* input) {
    Lexer lex = make_lexer(input);
    Token tok = next_token(&lex);

    if (tok.type == TOKEN_EOF) {
        free_token(&tok);
        return NULL;
    }

    return parse_expr(&lex, &tok);
}

// Parse from UT_string
Value* parse_utstring(UT_string* str) {
    return parse(utstring_body(str));
}
