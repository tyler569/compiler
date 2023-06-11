#pragma once
#ifndef COMPILER_TU_H
#define COMPILER_TU_H

#include <stdlib.h>

struct token;
struct node;

struct tu {
    const char *filename;

    const char *source;
    size_t source_len;

    struct token *tokens;
    size_t tokens_len;

    struct node *nodes;
    size_t nodes_len;

    struct {
        int capacity;
        int len;
        char *strtab;
    } strtab;

    bool abort;
};

struct token *tu_token(struct tu *, int token_id);
size_t tu_token_len(struct tu *, int token_id);
const char *tu_token_str(struct tu *, int token_id);

struct node *tu_node(struct tu *, int node_id);

#endif //COMPILER_TU_H
