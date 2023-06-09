#include "parse.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

typedef int node_pos;

#define NODE(context, n) (context->at.nodes[n])
#define TOKEN(context) (&context->tokens[context->position])
#define PEEK(context) (&context->tokens[context->position + 1])

struct context {
    struct token *tokens;
    int position;
    const char *source;
    int errors;

    struct {
        struct node *nodes;
        size_t len;
        size_t capacity;
    } ta;
};

// static functions
static bool is_typename(struct context *, struct token *first, size_t count);
static const char *tokenstring(struct context *, struct token *);
static bool tokenmatch(struct context *, struct token *, const char *word);
static bool more_data(struct context *);
static struct node *new(struct context *);
static int id(struct context *, struct node *);
static void report_error(struct context *, const char *message);
static int report_error_node(struct context *, const char *message);
static void pass(struct context *);
static void eat(struct context *, int token_type);
static struct token *pull(struct context *, int token_type);

static int parse_or(struct context *);

struct node *parse(struct token *tokens, const char *source) {
    struct context *context = &(struct context){
        .tokens = tokens,
        .source = source,
    };

    struct node *root = new(context);
    memset(root, 0, sizeof(*root));

    root->token = &(struct token){};
    root->type = NODE_ROOT;
    int n = 0;

    while (more_data(context) && context->errors == 0) {
        // parse_external_declaration(context);
        root->root.children[n++] = parse_or(context);
    }

    return root;
}

// C23(N3096) 6.7.2.1
static const char *typelist[] = {
    "void",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "signed",
    "unsigned",
};

static bool is_typename(struct context *context, struct token *first, size_t count) {
    if (count > 1) return false;
    if (first->type != TOKEN_IDENT) return false;

    for (int i = 0; i < ARRAY_LEN(typelist); i += 1) {
        if (tokenmatch(context, first, typelist[i])) return true;
    }
    return false;
}

static const char *tokenstring(struct context *context, struct token *token) {
    return context->source + token->index;
}

static bool tokenmatch(struct context *context, struct token *token, const char *word) {
    if (token->type != TOKEN_IDENT) return false;

    return strncmp(tokenstring(context, token), word, token->len);
}

static bool more_data(struct context *context) {
    return context->tokens[context->position].type != TOKEN_EOF;
}

static struct node *new(struct context *context) {
    if (context->ta.capacity <= context->ta.len) {
        size_t new_capacity = context->ta.capacity ? context->ta.capacity * 2 : 512;
        struct node *new_ta = realloc(context->ta.nodes, new_capacity * sizeof(struct node));
        if (!new_ta) {
            report_error(context, "memory allocation failed");
            return nullptr;
        }
        context->ta.nodes = new_ta;
        context->ta.capacity = new_capacity;
    }

    struct node *node = &context->ta.nodes[context->ta.len++];
    return node;
}

static int id(struct context *context, struct node *node) {
    return (int)(node - context->ta.nodes);
}

static void report_error(struct context *context, const char *message) {
    fprintf(stderr, "ast error: %s\n", message);
    context->errors += 1;
}

static int report_error_node(struct context *context, const char *message) {
    struct node *node = new(context);
    node->type = NODE_ERROR;
    node->token = TOKEN(context);
    return id(context, node);
}

static void pass(struct context *context) {
    context->position += 1;
}

static void eat(struct context *context, int token_type) {
    if (TOKEN(context)->type != token_type) {
        report_error(context, "expected eat, found wrong");
    }
    pass(context);
}

static struct token *pull(struct context *context, int token_type) {
    struct token *token = TOKEN(context);
    if (token->type == token_type) {
        pass(context);
        return token;
    } else {
        return nullptr;
    }
}

static void print_space(int level) {
    for (int i = 0; i < level; i++) fputs("  ", stdout);
}

#define GET(n) &root[(n)]
static void print_ast_recursive(struct node *root, struct node *node, const char *source, int level) {
    print_space(level);
    struct token *token = node->token;

    switch (node->type) {
    case NODE_ROOT:
        printf("root:\n");
        for (int i = 0; i < 10 && node->root.children[i]; i++) {
            print_ast_recursive(root, GET(node->root.children[i]), source, level + 1);
        }
        break;
    case NODE_INT_LITERAL:
        printf("int: %.*s\n", token->len, &source[token->index]);
        break;
    case NODE_BINOP:
        printf("binop: %.*s\n", token->len, &source[token->index]);
        struct node *left = GET(node->binop.left);
        struct node *right = GET(node->binop.right);
        print_ast_recursive(root, left, source, level + 1);
        print_ast_recursive(root, right, source, level + 1);
        break;
    case NODE_ERROR:
        printf("error: %.*s\n", token->len, &source[token->index]);
        break;
    default:
        printf("unknown\n");
    }
}
#undef GET

void print_ast(struct node *root, const char *source) {
    print_ast_recursive(root, root, source, 0);
}

static int parse_number(struct context *context) {
    struct token *token = TOKEN(context);
    pass(context);

    if (token->type != TOKEN_INT) {
        return report_error_node(context, "expected number, didn't find it");
    }

    struct node *node = new(context);
    node->type = NODE_INT_LITERAL;
    node->token = token;

    return id(context, node);
}

#define PARSE_BINOP(name, upstream, tt_expr) \
static int name(struct context *context) { \
    int result = upstream(context); \
    struct token *token = TOKEN(context); \
    while (tt_expr) { \
        pass(context); \
        struct node *node = new(context); \
        node->type = NODE_BINOP; \
        node->token = token; \
        node->binop.left = result; \
        node->binop.right = upstream(context); \
        result = id(context, node); \
        token = TOKEN(context); \
    } \
    return result; \
}

PARSE_BINOP(parse_mul, parse_number, token->type == '*' || token->type == '/' || token->type == '%')
PARSE_BINOP(parse_add, parse_mul, token->type == '+' || token->type == '-')
PARSE_BINOP(parse_shift, parse_add, token->type == TOKEN_SHIFT_LEFT || token->type == TOKEN_SHIFT_RIGHT)
PARSE_BINOP(parse_rel, parse_shift, token->type == '<' || token->type == '>' || token->type == TOKEN_GREATER_EQUAL || token->type == TOKEN_LESS_EQUAL)
PARSE_BINOP(parse_eq, parse_rel, token->type == TOKEN_EQUAL_EQUAL || token->type == TOKEN_NOT_EQUAL)
PARSE_BINOP(parse_bitand, parse_eq, token->type == '&')
PARSE_BINOP(parse_bitxor, parse_bitand, token->type == '^')
PARSE_BINOP(parse_bitor, parse_bitxor, token->type == '|')
PARSE_BINOP(parse_and, parse_bitor, token->type == TOKEN_AND)
PARSE_BINOP(parse_or, parse_and, token->type == TOKEN_OR)



// static int parse_mul(struct context *context) {
//     int result = parse_number(context);
//
//     struct token *token = TOKEN(context);
//     while (token->type == '*' || token->type == '/') {
//         pass(context);
//
//         struct node *node = new(context);
//         node->type = NODE_BINOP;
//         node->token = token;
//         node->binop.left = result;
//         node->binop.right = parse_number(context);
//
//         result = id(context, node);
//         token = TOKEN(context);
//     }
//
//     return result;
// }
//
// static int parse_add(struct context *context) {
//     int result = parse_mul(context);
//
//     struct token *token = TOKEN(context);
//     while (token->type == '+' || token->type == '-') {
//         pass(context);
//
//         struct node *node = new(context);
//         node->type = NODE_BINOP;
//         node->token = token;
//         node->binop.left = result;
//         node->binop.right = parse_mul(context);
//
//         result = id(context, node);
//         token = TOKEN(context);
//     }
//
//     return result;
// }
