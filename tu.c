#include "tu.h"
#include "token.h"
#include "parse.h"

struct token *tu_token(struct tu *tu, int token_id) {
    return &tu->tokens[token_id];
}

size_t tu_token_len(struct tu *tu, int token_id) {
    return tu->tokens[token_id].len;
}

const char *tu_token_str(struct tu *tu, int token_id) {
    return &tu->source[tu->tokens[token_id].len];
}

struct node *tu_node(struct tu *tu, int node_id) {
    return &tu->nodes[node_id];
}
