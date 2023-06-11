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
    NODE_DECLARATION,
    NODE_TYPE_SPECIFIER,
    NODE_DECLARATOR,
    NODE_ARRAY_DECLARATOR,
    NODE_FUNCTION_DECLARATOR,
    NODE_FUNCTION_DEFINITION,
    NODE_STATIC_ASSERT,
    NODE_BLOCK,
    NODE_LABEL,
    NODE_RETURN,
    NODE_IF,
    NODE_WHILE,
    NODE_DO,
    NODE_FOR,
    NODE_GOTO,
    NODE_SWITCH,
    NODE_CASE,
};

#define MAX_BLOCK_MEMBERS 10
#define MAX_FUNCTION_ARGS 9
#define MAX_DECLARATORS 9

struct node {
    struct token *token;
    enum node_type type;
    union {
        struct {
            int children[MAX_BLOCK_MEMBERS];
        } root;
        struct {
            int children[MAX_BLOCK_MEMBERS];
        } block;
        struct {
            int scope_id;
        } ident;
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
        } funcall;
        struct {
            int inner;
            int initializer;
            bool full;
            struct token *name;
            union {
                struct {
                    int subscript;
                } arr;
                struct {
                    int args[MAX_FUNCTION_ARGS];
                } fun;
            };
        } d;
        struct {
            int type;
            int declarators[MAX_DECLARATORS];
        } decl;
        struct {
            int expr;
            int message;
        } st_assert;
        struct {
            int expr;
        } ret;
        struct {
            int ret_type;
            int name;
            int body;
        } fun;
        struct {
            int name;
        } label;
    };
};


struct tu;

int parse(struct tu *);
void print_ast(struct tu *);

#endif //COMPILER_PARSE_H
