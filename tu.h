#pragma once
#ifndef COMPILER_TU_H
#define COMPILER_TU_H

#include <stdlib.h>

#include "list.h"
#include "token.h"
#include "parse.h"
#include "type.h"

typedef list(struct token) token_list_t;
typedef list(struct scope) scope_list_t;
typedef list(struct function) function_list_t;

struct tu {
    const char *filename;

    const char *source;
    size_t source_len;

    struct token *tokens;
    size_t tokens_len;

    struct node *ast_root;

    scope_list_t scopes;
    type_list_t types;
    function_list_t functions;

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

#endif //COMPILER_TU_H
