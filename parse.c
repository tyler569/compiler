#include "parse.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

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
static struct node *new(struct context *, enum node_type);
static int id(struct context *, struct node *);
static void report_error(struct context *, const char *message);
static int report_error_node(struct context *, const char *message);
static void pass(struct context *);
static void eat(struct context *, int token_type);

static int parse_assignment_expression(struct context *);
static int parse_expression(struct context *);

struct node *parse(struct token *tokens, const char *source) {
    struct context *context = &(struct context){
        .tokens = tokens,
        .source = source,
    };

    struct node *root = new(context, NODE_ROOT);
    memset(root, 0, sizeof(*root));

    root->token = &(struct token){};
    root->type = NODE_ROOT;
    int n = 0;

    while (more_data(context) && context->errors == 0 && n < MAX_BLOCK_MEMBERS) {
        // parse_external_declaration(context);
        root->root.children[n++] = parse_expression(context);
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
    if (strlen(word) != token->len) return false;

    return strncmp(tokenstring(context, token), word, token->len) == 0;
}

static bool more_data(struct context *context) {
    return context->tokens[context->position].type != TOKEN_EOF;
}

static struct node *new(struct context *context, enum node_type type) {
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
    node->type = type;
    node->token = TOKEN(context);

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
    struct node *node = new(context, NODE_ERROR);
    pass(context);
    return id(context, node);
}

static void pass(struct context *context) {
    context->position += 1;
}

// This isn't very useful right now since it just advances no matter what.
// It should probably add an error node or something, but I'm not sure that
// can be genericized.
static void eat(struct context *context, int token_type) {
    if (TOKEN(context)->type != token_type) {
        report_error(context, "expected eat, found wrong");
    }
    pass(context);
}

static void print_space(int level) {
    for (int i = 0; i < level; i++) fputs("  ", stdout);
}

#define GET(n) &root[(n)]
static void print_ast_recursive(const char *info, struct node *root, struct node *node, const char *source, int level) {
    if (level > 10) return;
    print_space(level);
    if (info) printf("%s ", info);

    struct token *token = node->token;

    switch (node->type) {
    case NODE_ROOT: {
        printf("root:\n");
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i++) {
            print_ast_recursive(nullptr, root, GET(node->root.children[i]), source, level + 1);
        }
        break;
    }
    case NODE_INT_LITERAL: {
        printf("int: %.*s (%llu)\n", token->len, &source[token->index], token->int_.value);
        break;
    }
    case NODE_FLOAT_LITERAL: {
        printf("float: %.*s (%f)\n", token->len, &source[token->index], token->float_.value);
        break;
    }
    case NODE_IDENT: {
        printf("ident: %.*s\n", token->len, &source[token->index]);
        break;
    }
    case NODE_BINARY_OP: {
        printf("binop: %.*s\n", token->len, &source[token->index]);
        struct node *left = GET(node->binop.left);
        struct node *right = GET(node->binop.right);
        print_ast_recursive(nullptr, root, left, source, level + 1);
        print_ast_recursive(nullptr, root, right, source, level + 1);
        break;
    }
    case NODE_UNARY_OP: {
        printf("unop: %.*s\n", token->len, &source[token->index]);
        struct node *inner = GET(node->unary_op.inner);
        print_ast_recursive(nullptr, root, inner, source, level + 1);
        break;
    }
    case NODE_POSTFIX_OP: {
        printf("postfix: %.*s\n", token->len, &source[token->index]);
        struct node *inner = GET(node->unary_op.inner);
        print_ast_recursive(nullptr, root, inner, source, level + 1);
        break;
    }
    case NODE_SUBSCRIPT: {
        printf("subscript:\n");
        struct node *inner = GET(node->subscript.inner);
        struct node *subscript = GET(node->subscript.subscript);
        print_ast_recursive("arr:", root, inner, source, level + 1);
        print_ast_recursive("sub:", root, subscript, source, level + 1);
        break;
    }
    case NODE_TERNARY: {
        printf("ternary:\n");
        struct node *cond = GET(node->ternary.condition);
        struct node *b_true = GET(node->ternary.branch_true);
        struct node *b_false = GET(node->ternary.branch_false);
        print_ast_recursive("cnd:", root, cond, source, level + 1);
        print_ast_recursive("yes:", root, b_true, source, level + 1);
        print_ast_recursive(" no:", root, b_false, source, level + 1);
        break;
    }
    case NODE_FUNCTION_CALL: {
        printf("funcall:\n");
        struct node *function = GET(node->function_call.inner);
        print_ast_recursive("fun:", root, function, source, level + 1);
        for (int i = 0; i < MAX_FUNCTION_ARGS && node->function_call.args[i] != 0; i += 1) {
            print_ast_recursive("arg:", root, GET(node->function_call.args[i]), source, level + 1);
        }
        break;
    }
    case NODE_ERROR: {
        printf("error: %.*s\n", token->len, &source[token->index]);
        break;
    }
    default:
        printf("unknown\n");
    }
}
#undef GET

void print_ast(struct node *root, const char *source) {
    print_ast_recursive(nullptr, root, root, source, 0);
}

static int parse_primary_expression(struct context *context) {
    switch (TOKEN(context)->type) {
    case TOKEN_INT: {
        struct node *node = new(context, NODE_INT_LITERAL);
        pass(context);
        return id(context, node);
    }
    case TOKEN_FLOAT: {
        struct node *node = new(context, NODE_FLOAT_LITERAL);
        pass(context);
        return id(context, node);
    }
    case TOKEN_IDENT: {
        struct node *node = new(context, NODE_IDENT);
        pass(context);
        return id(context, node);
    }
    case TOKEN_STRING: {
        struct node *node = new(context, NODE_STRING_LITERAL);
        pass(context);
        return id(context, node);
    }
    case '(': {
        pass(context);
        int expr = parse_expression(context);
        eat(context, ')');
        return expr;
    }
    default:
        return report_error_node(context, "expected primary expression");
    }
}

static int parse_postfix_expression(struct context *context) {
    int inner = parse_primary_expression(context);
    bool cont = true;
    while (cont) {
        switch (TOKEN(context)->type) {
        case TOKEN_PLUS_PLUS:
        case TOKEN_MINUS_MINUS: {
            struct node *node = new(context, NODE_POSTFIX_OP);
            pass(context);
            node->unary_op.inner = inner;
            inner = id(context, node);
            break;
        }
        case '.':
        case TOKEN_ARROW: {
            if (PEEK(context)->type != TOKEN_IDENT) {
                return report_error_node(context, "need ident after member reference");
            }
            struct node *node = new(context, NODE_MEMBER);
            pass(context);
            node->member.inner = inner;
            node->member.ident = TOKEN(context);
            pass(context);
            inner = id(context, node);
            break;
        }
        case '(': {
            struct node *node = new(context, NODE_FUNCTION_CALL);
            pass(context);
            node->function_call.inner = inner;
            int i = 0;
            while (TOKEN(context)->type != ')' && i < MAX_FUNCTION_ARGS) {
                node->function_call.args[i++] = parse_assignment_expression(context);
                if (TOKEN(context)->type != ')') eat(context, ',');
            }
            eat(context, ')');
            inner = id(context, node);
            break;
        }
        case '[': {
            struct node *node = new(context, NODE_SUBSCRIPT);
            pass(context);
            node->subscript.inner = inner;
            node->subscript.subscript = parse_expression(context);
            eat(context, ']');
            inner = id(context, node);
            break;
        }
        default:
            cont = false;
        }
    }
    return inner;
}

static int parse_prefix_expression(struct context *context) {
    struct token *token = TOKEN(context);
    if (token->type == TOKEN_PLUS_PLUS || token->type == TOKEN_MINUS_EQUAL ||
        token->type == '+' || token->type == '-' || token->type == '*' ||
        token->type == '&' || token->type == '~' || token->type == '!' ||
        tokenmatch(context, token, "sizeof") || tokenmatch(context, token, "alignof")) {

        struct node *node = new(context, NODE_UNARY_OP);
        pass(context);
        node->unary_op.inner = parse_prefix_expression(context);
        return id(context, node);
    } else {
        return parse_postfix_expression(context);
    }
}

static int parse_cast_expression(struct context *context) {
    return parse_prefix_expression(context);
}

#define PARSE_BINOP(name, upstream, tt_expr) \
static int name(struct context *context) { \
    int result = upstream(context); \
    struct token *token = TOKEN(context); \
    while (tt_expr) { \
        struct node *node = new(context, NODE_BINARY_OP); \
        pass(context); \
        node->binop.left = result; \
        node->binop.right = upstream(context); \
        result = id(context, node); \
        token = TOKEN(context); \
    } \
    return result; \
}

PARSE_BINOP(parse_mul, parse_cast_expression, token->type == '*' || token->type == '/' || token->type == '%')
PARSE_BINOP(parse_add, parse_mul, token->type == '+' || token->type == '-')
PARSE_BINOP(parse_shift, parse_add, token->type == TOKEN_SHIFT_LEFT || token->type == TOKEN_SHIFT_RIGHT)
PARSE_BINOP(parse_rel, parse_shift, token->type == '<' || token->type == '>' || token->type == TOKEN_GREATER_EQUAL || token->type == TOKEN_LESS_EQUAL)
PARSE_BINOP(parse_eq, parse_rel, token->type == TOKEN_EQUAL_EQUAL || token->type == TOKEN_NOT_EQUAL)
PARSE_BINOP(parse_bitand, parse_eq, token->type == '&')
PARSE_BINOP(parse_bitxor, parse_bitand, token->type == '^')
PARSE_BINOP(parse_bitor, parse_bitxor, token->type == '|')
PARSE_BINOP(parse_and, parse_bitor, token->type == TOKEN_AND)
PARSE_BINOP(parse_or, parse_and, token->type == TOKEN_OR)

static int parse_ternary_expression(struct context *context) {
    int condition = parse_or(context);
    if (TOKEN(context)->type != '?') {
        return condition;
    }
    struct node *node = new(context, NODE_TERNARY);
    pass(context);

    int branch_true = parse_expression(context);
    eat(context, ':');
    int branch_false = parse_ternary_expression(context);

    node->ternary.condition = condition;
    node->ternary.branch_true = branch_true;
    node->ternary.branch_false = branch_false;
    return id(context, node);
}

static int parse_assignment_expression(struct context *context) {
    struct context saved = *context;

    int expr = parse_prefix_expression(context);
    struct token *token = TOKEN(context);

    if (token->type == '=' || token->type == TOKEN_STAR_EQUAL || token->type == TOKEN_DIVIDE_EQUAL ||
        token->type == TOKEN_MOD_EQUAL || token->type == TOKEN_PLUS_EQUAL || token->type == TOKEN_MINUS_EQUAL ||
        token->type == TOKEN_SHIFT_LEFT_EQUAL || token->type == TOKEN_SHIFT_RIGHT_EQUAL ||
        token->type == TOKEN_BITAND_EQUAL || token->type == TOKEN_BITXOR_EQUAL || token->type == TOKEN_BITOR_EQUAL) {

        struct node *node = new(context, NODE_BINARY_OP);
        pass(context);

        node->binop.left = expr;
        node->binop.right = parse_assignment_expression(context);
        return id(context, node);
    } else {
        assert(context->ta.nodes == saved.ta.nodes);
        *context = saved;
        return parse_ternary_expression(context);
    }
}

PARSE_BINOP(parse_expression, parse_assignment_expression, token->type == ',')
