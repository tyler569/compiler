#pragma once
#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

enum layer_type {
    TYPE_VOID,
    TYPE_SIGNED_CHAR,
    TYPE_SIGNED_SHORT,
    TYPE_SIGNED_INT,
    TYPE_SIGNED_LONG,
    TYPE_SIGNED_LONG_LONG,
    TYPE_UNSIGNED_CHAR,
    TYPE_UNSIGNED_SHORT,
    TYPE_UNSIGNED_INT,
    TYPE_UNSIGNED_LONG,
    TYPE_UNSIGNED_LONG_LONG,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LONG_DOUBLE,
    TYPE_COMPLEX_FLOAT,
    TYPE_COMPLEX_DOUBLE,
    TYPE_COMPLEX_LONG_DOUBLE,

    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_ENUM,
    TYPE_STRUCT,
    TYPE_UNION,

    TYPE_FIELD,
    TYPE_ARG,
};

enum type_flags {
    TF_ATOMIC = (1 << 0),
    TF_CONST = (1 << 1),
    TF_VOLATILE = (1 << 2),
    TF_RESTRICT = (1 << 3),
    TF_INLINE = (1 << 4),
    TF_NORETURN = (1 << 5),
    // 4 bits representing log2(alignas value)
    // alignas(32) is represented as (5 << TF_ALIGNAS_BIT)
    TF_ALIGNAS_0 = (1 << 6),
    TF_ALIGNAS_1 = (1 << 7),
    TF_ALIGNAS_2 = (1 << 8),
    TF_ALIGNAS_3 = (1 << 9),
};

#define TF_ALIGNAS_BIT 6

struct type {
    enum layer_type layer;
    enum type_flags flags;

    union {
        struct {
            struct token *name;
            int bits;
            int next;
        } field;
        struct {
            struct token *name;
        } enum_;
        struct {
            int next;
        } function_arg;
    };

    int inner;
};

struct tu;

int type(struct tu *tu);

#endif //COMPILER_TYPE_H
