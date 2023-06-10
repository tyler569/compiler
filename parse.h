#pragma once
#ifndef COMPILER_PARSE_H
#define COMPILER_PARSE_H

enum node_type {
    NODE_ROOT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_POSTFIX_OP,
    NODE_IDENT,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_ERROR,
    NODE_MEMBER,
    NODE_SUBSCRIPT,
    NODE_TERNARY,
    NODE_FUNCTION_CALL,
};

#define MAX_BLOCK_MEMBERS 10
#define MAX_FUNCTION_ARGS 9

struct node {
    enum node_type type;
    struct token *token;
    union {
        struct {
            int children[MAX_BLOCK_MEMBERS];
        } root;
        struct {
            int left;
            int right;
        } binop;
        struct {
            int inner;
        } unary_op;
        struct {
            int inner;
            struct token *ident;
        } member;
        struct {
            int inner;
            int subscript;
        } subscript;
        struct {
            int condition;
            int branch_true;
            int branch_false;
        } ternary;
        struct {
            int inner;
            int args[MAX_FUNCTION_ARGS];
        } function_call;
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
