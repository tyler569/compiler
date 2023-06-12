#pragma once
#ifndef COMPILER_TU_H
#define COMPILER_TU_H

#include <stdlib.h>

struct token;
struct node;
struct ir_instr;
struct scope;

struct tu {
    const char *filename;

    const char *source;
    size_t source_len;

    struct token *tokens;
    size_t tokens_len;

    struct node *nodes;
    size_t nodes_len;

    struct scope *scopes;
    size_t scopes_len;

    struct ir_instr *ir;
    size_t ir_len;

    struct {
        int capacity;
        int len;
        char *strtab;
    } strtab;

    bool abort;
};

#include "token.h"
#include "parse.h"


static inline struct token *tu_token(struct tu *tu, int token_id) {
    return &tu->tokens[token_id];
}

static inline size_t tu_token_len(struct tu *tu, int token_id) {
    return tu->tokens[token_id].len;
}

static inline const char *tu_token_str(struct tu *tu, int token_id) {
    return &tu->source[tu->tokens[token_id].len];
}

static inline struct node *tu_node(struct tu *tu, int node_id) {
    return &tu->nodes[node_id];
}

#endif //COMPILER_TU_H
