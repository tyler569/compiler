#pragma once
#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#include "list.h"

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

    TYPE_AUTO,

    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_ENUM,
    TYPE_STRUCT,
    TYPE_UNION,
};

#define TF_ALIGNAS_BIT 6

enum type_flags {
    TF_ATOMIC = (1 << 0),
    TF_CONST = (1 << 1),
    TF_VOLATILE = (1 << 2),
    TF_RESTRICT = (1 << 3),
    TF_INLINE = (1 << 4),
    TF_NORETURN = (1 << 5),
    // 4 bits representing log2(alignas value)
    // alignas(32) is represented as (5 << TF_ALIGNAS_BIT)
    TF_ALIGNAS_0 = (1 << (TF_ALIGNAS_BIT + 0)),
    TF_ALIGNAS_1 = (1 << (TF_ALIGNAS_BIT + 1)),
    TF_ALIGNAS_2 = (1 << (TF_ALIGNAS_BIT + 2)),
    TF_ALIGNAS_3 = (1 << (TF_ALIGNAS_BIT + 3)),
};

enum storage_class {
    ST_AUTOMATIC,
    ST_CONSTEXPR,
    ST_EXTERNAL,
    ST_REGISTER,
    ST_STATIC,
    ST_THREAD_LOCAL,
    ST_TYPEDEF,
};

struct type;

typedef list(struct type) type_list_t;

struct type {
    enum layer_type layer;
    enum type_flags flags;

    int inner;

    union {
        struct {
            struct token *name;
            int bits;
        } field;
        struct {
            struct token *name;
        } enum_;
        struct {
            type_list_t fields;
        } struct_;
        struct {
            type_list_t args;
        } function;
    };
};

struct scope {
    struct token *token;
    struct node *decl;
    enum storage_class sc;
    bool ns_tag;
    bool is_global;
    int c_type;
    int parent;
    int block_depth;
    int ir_index;
    int frame_offset;
};

struct tu;

int find_or_create_type(struct tu *, int inner, enum layer_type base, enum type_flags flags);
void print_type(struct tu *tu, int type_id);
int type(struct tu *tu);

#endif //COMPILER_TYPE_H
