#pragma once
#ifndef COMPILER_PARSE_H
#define COMPILER_PARSE_H

enum node_type {
    NODE_ROOT,
    NODE_BINOP,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_TYPE,
    NODE_DECLARATION,
    NODE_FUNCTION,
};

struct node {
    enum node_type type;
    struct token *token;
    union {
        struct {
            int children[10];
        } root;
        struct {
            int left;
            int right;
        } binop;
    };
};

enum type_type {
    TYPE_BASIC,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_ENUM,
};

enum type_flags {
    T_CONST,
    T_VOLATILE,
};

struct type {
    enum type_type type;
    enum type_flags flags;
    struct token *open_paren;
    struct token *close_paren;
    struct token *value;
    union {
        struct {
            int child_id;
        } pointer;
        struct {
            int length;
            int child_id;
        } array;
        struct {
            bool is_union;
            int *fields;
        } struct_;
        struct {
            int base_id;
        } enum_;
    };
};

struct node *parse(struct token *tokens, const char *source);
void print_ast(struct node *root, const char *source);

#endif //COMPILER_PARSE_H
