#pragma once
#ifndef COMPILER_TOKEN_H
#define COMPILER_TOKEN_H

#include <stdlib.h>

enum {
    TOKEN_NULL = 0,
    TOKEN_IDENT = 128,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_EOF,

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

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,

    TOKEN_SHIFT_RIGHT,
    TOKEN_SHIFT_RIGHT_EQUAL,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_LEFT_EQUAL,
};

struct token {
    int type;
    int index;
    int len;

    int line;
    int column;
};

struct token *tokenize(size_t len, const char *source, const char *filename);
void print_token_type(struct token *);

#endif //COMPILER_TOKEN_H
