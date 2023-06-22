#pragma once
#ifndef COMPILER_TOKEN_H
#define COMPILER_TOKEN_H

#include <stdlib.h>

enum {
    TOKEN_NULL = 0,
    // elements
    TOKEN_IDENT = 128,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_EOF,
    // operators with more than 1 byte
    TOKEN_ARROW,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_DIVIDE_EQUAL,
    TOKEN_MOD_EQUAL,
    TOKEN_AND_EQUAL,
    TOKEN_OR_EQUAL,
    TOKEN_BITAND_EQUAL,
    TOKEN_BITOR_EQUAL,
    TOKEN_BITXOR_EQUAL,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_SHIFT_RIGHT,
    TOKEN_SHIFT_RIGHT_EQUAL,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_LEFT_EQUAL,
    TOKEN_ELLIPSES,
    TOKEN_COLON_COLON,
    // keywords
    TOKEN_FIRST_KEYWORD,
    TOKEN_ALIGNAS = TOKEN_FIRST_KEYWORD,
    TOKEN_ALIGNOF,
    TOKEN_AUTO,
    TOKEN_BOOL,
    TOKEN_BREAK,
    TOKEN_CASE,
    TOKEN_CHAR,
    TOKEN_CONST,
    TOKEN_CONSTEXPR,
    TOKEN_CONTINUE,
    TOKEN_DEFAULT,
    TOKEN_DO,
    TOKEN_DOUBLE,
    TOKEN_ELSE,
    TOKEN_ENUM,
    TOKEN_EXTERN,
    TOKEN_FALSE,
    TOKEN_FLOAT,
    TOKEN_FOR,
    TOKEN_GOTO,
    TOKEN_IF,
    TOKEN_INLINE,
    TOKEN_INT,
    TOKEN_LONG,
    TOKEN_NULLPTR,
    TOKEN_REGISTER,
    TOKEN_RESTRICT,
    TOKEN_RETURN,
    TOKEN_SHORT,
    TOKEN_SIGNED,
    TOKEN_SIZEOF,
    TOKEN_STATIC,
    TOKEN_STATIC_ASSERT,
    TOKEN_STRUCT,
    TOKEN_SWITCH,
    TOKEN_THREAD_LOCAL,
    TOKEN_TRUE,
    TOKEN_TYPEDEF,
    TOKEN_TYPEOF,
    TOKEN_TYPEOF_UNQUAL,
    TOKEN_UNION,
    TOKEN_UNSIGNED,
    TOKEN_VOID,
    TOKEN_VOLATILE,
    TOKEN_WHILE,
    TOKEN__ATOMIC,
    TOKEN__BITINT,
    TOKEN__COMPLEX,
    TOKEN__DECIMAL128,
    TOKEN__DECIMAL32,
    TOKEN__DECIMAL64,
    TOKEN__GENERIC,
    TOKEN__IMAGINARY,
    TOKEN__NORETURN,
    TOKEN_LAST_KEYWORD,
};

struct token {
    int type;
    int index;
    int len;

    int line;
    int column;

    union {
        struct {
            uint64_t value;
        } int_;
        struct {
            double value;
        } float_;
    };
};

struct tu;

int tokenize(struct tu *);

void print_tokens(struct tu *);

void print_token(struct tu *tu, struct token *token);
void print_token_type(struct token *);
const char *token_type_string(int token_type);

#endif //COMPILER_TOKEN_H
