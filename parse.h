#pragma once
#ifndef COMPILER_PARSE_H
#define COMPILER_PARSE_H

#include "list.h"
#include "type.h"

enum node_type {
    NODE_NULL,
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
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_DEFAULT,
    NODE_STRUCT,
    NODE_ENUM,
    NODE_UNION,

    NODE_TYPE_COUNT,
};

extern const char *node_type_strings[NODE_TYPE_COUNT];

typedef list(struct node *) node_list_t;

struct node {
    struct token *token;
    struct token *token_end;
    struct token *attached_comment;
    enum node_type type;
    int c_type;
    union {
        struct {
            node_list_t children;
        } root;
        struct {
            node_list_t children;
        } block;
        struct {
            int scope_id;
        } ident;
        struct {
            struct node *lhs;
            struct node *rhs;
        } binop;
        struct {
            struct node *inner;
        } unary_op;
        struct {
            struct node *inner;
            struct node *ident;
        } member;
        struct {
            struct node *inner;
            struct node *subscript;
        } subscript;
        struct {
            struct node *condition;
            struct node *branch_true;
            struct node *branch_false;
        } ternary;
        struct {
            struct node *inner;
            node_list_t args;
        } funcall;
        struct {
            struct node *inner;
            struct node *initializer;
            bool full;
            struct token *name;
            bool nameless;
            int scope_id;
            union {
                struct {
                    struct node *subscript;
                } arr;
                struct {
                    node_list_t args;
                } fun;
            };
        } d;
        struct {
            int decl_spec_c_type;
            enum storage_class sc;
            node_list_t declarators;
        } decl;
        struct {
            struct node *expr;
            struct node *message;
        } st_assert;
        struct {
            struct node *expr;
        } ret;
        struct {
            struct node *decl;
            struct node *body;
            struct node *d;
        } fun;
        struct {
            struct node *name;
        } label;
        struct {
            struct node *cond;
            struct node *block_true;
            struct node *block_false;
        } if_;
        struct {
            struct node *cond;
            struct node *block;
        } while_;
        struct {
            struct node *cond;
            struct node *block;
        } do_;
        struct {
            struct node *init;
            struct node *next;
            struct node *cond;
            struct node *block;
        } for_;
        struct {
            struct node *label;
        } goto_;
        struct {
            struct node *value;
        } case_;
        struct {
            struct node *expr;
            struct node *block;
            node_list_t cases;
        } switch_;
        struct {
            struct node *name;
            node_list_t decls;
        } struct_;
        struct {
            struct node *breakable;
        } break_;
    };
};


struct tu;

int parse(struct tu *);
void print_ast(struct tu *);

struct token *node_begin(struct node *);
struct token *node_end(struct node *);

#endif //COMPILER_PARSE_H
