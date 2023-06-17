#include "parse.h"
#include "token.h"
#include "util.h"
#include "diag.h"
#include "tu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef int node_pos;

#define TOKEN(context) (&context->tokens[context->position])
#define PEEK(context) (&context->tokens[context->position + 1])
#define PEEKN(context, n) (&context->tokens[context->position + (n)])

struct context {
    struct tu *tu;
    struct token *tokens;
    int position;
    const char *source;
    int errors;

    struct node *root;
};

// static functions
static bool is_typename(struct context *, struct token *first, size_t count);
static bool more_data(struct context *);
static struct node *new(struct context *, enum node_type);
static void report_error(struct context *, const char *message);
static struct node *report_error_node(struct context *, const char *message);
static void pass(struct context *);
static void eat(struct context *, int token_type, const char *function_name);

static struct node *parse_assignment_expression(struct context *);
static struct node *parse_expression(struct context *);
static struct node *parse_declaration(struct context *);
static struct node *parse_statement(struct context *);
static struct node *parse_external_definition(struct context *);

int parse(struct tu *tu) {
    struct context *context = &(struct context){
        .tu = tu,
        .tokens = tu->tokens,
        .source = tu->source,
    };

    struct node *root = new(context, NODE_ROOT);

    root->token = tu->tokens;
    root->type = NODE_ROOT;
    int n = 0;
    context->root = root;

    while (more_data(context) && context->errors == 0 && n < MAX_BLOCK_MEMBERS) {
        root->root.children[n++] = parse_external_definition(context);
    }

    tu->nodes = root;

    return context->errors;
}

static bool more_data(struct context *context) {
    return TOKEN(context)->type != TOKEN_EOF;
}

// TODO: make this a pool or bump allocator of some kind so I can free these without
// recursing through the entire tree.
static struct node *new(struct context *context, enum node_type type) {
    struct node *node = calloc(1, sizeof(struct node));
    node->type = type;
    node->token = TOKEN(context);

    return node;
}

static void report_error(struct context *context, const char *message) {
    fprintf(stderr, "ast error: %s\n", message);
    print_and_highlight(context->source, TOKEN(context));
    context->errors += 1;

    if (context->tu->abort) exit(1);
}

static struct node *report_error_node(struct context *context, const char *message) {
    struct node *node = new(context, NODE_ERROR);
    fprintf(stderr, "new error: %s\n", message);
    print_and_highlight(context->source, TOKEN(context));

    if (context->tu->abort) exit(1);

    pass(context);
    return node;
}

static void pass(struct context *context) {
    context->position += 1;
}

// This isn't very useful right now since it just advances no matter what.
// It should probably add an error node or something, but I'm not sure that
// can be genericized.
static void eat(struct context *context, int token_type, const char *function_name) {
    if (TOKEN(context)->type != token_type) {
        char buffer[128];
        snprintf(buffer, 128, "eat: expected '%s', found '%s' in %s",
                 token_type_string(token_type),
                 token_type_string(TOKEN(context)->type),
                 function_name);
        report_error(context, buffer);
    }
    pass(context);
}

#define eat(ctx, typ) eat(ctx, typ, __func__)

static void print_space(int level) {
    for (int i = 0; i < level; i++) fputs("  ", stdout);
}

#define RECUR(node) print_ast_recursive(nullptr, tu, (node), level + 1)
#define RECUR_INFO(info, node) print_ast_recursive((info), tu, (node), level + 1)
static void print_ast_recursive(const char *info, struct tu *tu, struct node *node, int level) {
    print_error(tu, node, "testing");

    if (level > 20) exit(1);
    print_space(level);
    if (info) printf("%s ", info);

    struct token *token = node->token;
    const char *source = tu->source;

    switch (node->type) {
    case NODE_ROOT: {
        printf("root:\n");
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i++) {
            RECUR(node->root.children[i]);
        }
        break;
    }
    case NODE_BLOCK: {
        printf("block:\n");
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i++) {
            RECUR(node->block.children[i]);
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
        RECUR(node->binop.lhs);
        RECUR(node->binop.rhs);
        break;
    }
    case NODE_UNARY_OP: {
        printf("unop: %.*s\n", token->len, &source[token->index]);
        RECUR(node->unary_op.inner);
        break;
    }
    case NODE_POSTFIX_OP: {
        printf("postfix: %.*s\n", token->len, &source[token->index]);
        RECUR(node->unary_op.inner);
        break;
    }
    case NODE_SUBSCRIPT: {
        printf("subscript:\n");
        RECUR_INFO("arr:", node->subscript.inner);
        RECUR_INFO("sub:", node->subscript.subscript);
        break;
    }
    case NODE_TERNARY: {
        printf("ternary:\n");
        RECUR_INFO("cnd:", node->ternary.condition);
        RECUR_INFO("tru:", node->ternary.branch_true);
        RECUR_INFO("fls:", node->ternary.branch_false);
        break;
    }
    case NODE_FUNCTION_CALL: {
        printf("funcall:\n");
        RECUR_INFO("fun:", node->funcall.inner);
        for (int i = 0; i < MAX_FUNCTION_ARGS && node->funcall.args[i] != 0; i += 1) {
            RECUR_INFO("arg", node->funcall.args[i]);
        }
        break;
    }
    case NODE_DECLARATION: {
        printf("decl:\n");
        RECUR_INFO("typ:", node->decl.type);
        for (int i = 0; i < MAX_DECLARATORS && node->decl.declarators[i] != 0; i += 1) {
            RECUR_INFO("dcl:", node->decl.declarators[i]);
        }
        break;
    }
    case NODE_TYPE_SPECIFIER: {
        printf("type: %.*s\n", token->len, &source[token->index]);
        break;
    }
    case NODE_DECLARATOR:
    case NODE_FUNCTION_DECLARATOR:
    case NODE_ARRAY_DECLARATOR: {
        printf("d: ");
        struct node *n = node;
        while (true) {
            token = n->token;
            if (n->type == NODE_DECLARATOR) {
                printf("%.*s", token->len, &source[token->index]);
            } else if (n->type == NODE_FUNCTION_DECLARATOR) {
                printf("()");
            } else if (n->type == NODE_ARRAY_DECLARATOR) {
                printf("[]");
            }

            if (!n->d.inner) {
                printf("\n");
                break;
            }
            printf(" -> ");
            n = n->d.inner;
        }
        if (node->d.initializer)
            RECUR_INFO("ini:", node->d.initializer);
        break;
    }
    case NODE_STATIC_ASSERT: {
        printf("static assert:\n");
        RECUR_INFO("tst:", node->st_assert.expr);
        if (node->st_assert.message)
            RECUR_INFO("msg:", node->st_assert.message);
        break;
    }
    case NODE_FUNCTION_DEFINITION: {
        printf("function:\n");
        RECUR_INFO("typ:", node->fun.decl);
        RECUR_INFO("bdy:", node->fun.body);
        break;
    }
    case NODE_RETURN:
        printf("return:\n");
        RECUR(node->ret.expr);
        break;
    case NODE_IF:
        printf("if:\n");
        RECUR_INFO("cnd:", node->if_.cond);
        RECUR_INFO("yes:", node->if_.block_true);
        if (node->if_.block_false)
            RECUR_INFO(" no:", node->if_.block_false);
        break;
    case NODE_WHILE:
        printf("while:\n");
        RECUR_INFO("cnd:", node->while_.cond);
        RECUR_INFO("blk:", node->while_.block);
        break;
    case NODE_NULL:
        printf("null:\n");
        break;
    case NODE_ERROR:
        printf("error: %.*s\n", token->len, &source[token->index]);
        break;
    default:
        printf("unknown\n");
    }
}
#undef RECUR
#undef RECUR_INFO

void print_ast(struct tu *tu) {
    print_ast_recursive(nullptr, tu, tu->nodes, 0);
}

struct token *node_begin(struct node *node) {
    switch (node->type) {
    case NODE_BINARY_OP:
        return node_begin(node->binop.lhs);
    case NODE_POSTFIX_OP:
        return node_begin(node->unary_op.inner);
    case NODE_FUNCTION_DECLARATOR:
    case NODE_ARRAY_DECLARATOR:
        return node_begin(node->d.inner);
    case NODE_TERNARY:
        return node_begin(node->ternary.condition);
    default:
        return node->token;
    }
}

struct token *node_end(struct node *node) {
    if (node->token_end) return node->token_end;

    switch (node->type) {
    case NODE_FUNCTION_DEFINITION:
        return node_end(node->fun.body);
    case NODE_IF:
        if (node->if_.block_false)
            return node_end(node->if_.block_false);
        else
            return node_end(node->if_.block_true);
    case NODE_WHILE:
        return node_end(node->while_.block);
    case NODE_UNARY_OP:
        return node_end(node->unary_op.inner);
    case NODE_BINARY_OP:
        return node_end(node->binop.rhs);
    case NODE_DECLARATOR:
        return node_end(node->d.inner);
    case NODE_TERNARY:
        return node_end(node->ternary.branch_false);
    default:
        return node->token;
    }
}

static struct node *parse_ident(struct context *context) {
    if (TOKEN(context)->type != TOKEN_IDENT)
        return report_error_node(context, "expected an ident, but didn't find it");
    struct node *node = new(context, NODE_IDENT);
    pass(context);
    return node;
}

static struct node *parse_primary_expression(struct context *context) {
    switch (TOKEN(context)->type) {
    case TOKEN_INT: {
        struct node *node = new(context, NODE_INT_LITERAL);
        pass(context);
        return node;
    }
    case TOKEN_FLOAT: {
        struct node *node = new(context, NODE_FLOAT_LITERAL);
        pass(context);
        return node;
    }
    case TOKEN_IDENT: {
        struct node *node = new(context, NODE_IDENT);
        pass(context);
        return node;
    }
    case TOKEN_STRING: {
        struct node *node = new(context, NODE_STRING_LITERAL);
        pass(context);
        return node;
    }
    case '(': {
        pass(context);
        struct node *expr = parse_expression(context);
        expr->token_end = TOKEN(context);
        eat(context, ')');
        return expr;
    }
    default:
        return report_error_node(context, "expected primary expression");
    }
}

static struct node *parse_postfix_expression(struct context *context) {
    struct node *inner = parse_primary_expression(context);
    bool cont = true;
    while (cont) {
        switch (TOKEN(context)->type) {
        case TOKEN_PLUS_PLUS:
        case TOKEN_MINUS_MINUS: {
            struct node *node = new(context, NODE_POSTFIX_OP);
            pass(context);
            node->unary_op.inner = inner;
            inner = node;
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
            node->token_end = TOKEN(context);
            pass(context);
            inner = node;
            break;
        }
        case '(': {
            struct node *node = new(context, NODE_FUNCTION_CALL);
            pass(context);
            node->funcall.inner = inner;
            int i = 0;
            while (TOKEN(context)->type != ')' && i < MAX_FUNCTION_ARGS) {
                node->funcall.args[i++] = parse_assignment_expression(context);
                if (TOKEN(context)->type != ')') eat(context, ',');
            }
            eat(context, ')');
            inner = node;
            break;
        }
        case '[': {
            struct node *node = new(context, NODE_SUBSCRIPT);
            pass(context);
            node->subscript.inner = inner;
            node->subscript.subscript = parse_expression(context);
            eat(context, ']');
            inner = node;
            break;
        }
        default:
            cont = false;
        }
    }
    return inner;
}

static struct node *parse_prefix_expression(struct context *context) {
    struct token *token = TOKEN(context);
    if (token->type == TOKEN_PLUS_PLUS || token->type == TOKEN_MINUS_EQUAL ||
        token->type == '+' || token->type == '-' || token->type == '*' ||
        token->type == '&' || token->type == '~' || token->type == '!' ||
        token->type == TOKEN_SIZEOF || token->type == TOKEN_ALIGNOF) {

        struct node *node = new(context, NODE_UNARY_OP);
        pass(context);
        node->unary_op.inner = parse_prefix_expression(context);
        return node;
    } else {
        return parse_postfix_expression(context);
    }
}

static struct node *parse_cast_expression(struct context *context) {
    return parse_prefix_expression(context);
}

#define PARSE_BINOP(name, upstream, tt_expr) \
static struct node *name(struct context *context) { \
    struct node *result = upstream(context); \
    struct token *token = TOKEN(context); \
    while (tt_expr) { \
        struct node *node = new(context, NODE_BINARY_OP); \
        pass(context); \
        node->binop.lhs = result; \
        node->binop.rhs = upstream(context); \
        result = node; \
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

static struct node *parse_ternary_expression(struct context *context) {
    struct node *condition = parse_or(context);
    if (TOKEN(context)->type != '?') {
        return condition;
    }
    struct node *node = new(context, NODE_TERNARY);
    pass(context);

    struct node *branch_true = parse_expression(context);
    eat(context, ':');
    struct node *branch_false = parse_ternary_expression(context);

    node->ternary.condition = condition;
    node->ternary.branch_true = branch_true;
    node->ternary.branch_false = branch_false;
    return node;
}

static struct node *parse_assignment_expression(struct context *context) {
    struct context saved = *context;

    struct node *expr = parse_prefix_expression(context);
    struct token *token = TOKEN(context);

    if (token->type == '=' || token->type == TOKEN_STAR_EQUAL || token->type == TOKEN_DIVIDE_EQUAL ||
        token->type == TOKEN_MOD_EQUAL || token->type == TOKEN_PLUS_EQUAL || token->type == TOKEN_MINUS_EQUAL ||
        token->type == TOKEN_SHIFT_LEFT_EQUAL || token->type == TOKEN_SHIFT_RIGHT_EQUAL ||
        token->type == TOKEN_BITAND_EQUAL || token->type == TOKEN_BITXOR_EQUAL || token->type == TOKEN_BITOR_EQUAL) {

        struct node *node = new(context, NODE_BINARY_OP);
        pass(context);

        node->binop.lhs = expr;
        node->binop.rhs = parse_assignment_expression(context);
        return node;
    } else {
        *context = saved;
        return parse_ternary_expression(context);
    }
}

PARSE_BINOP(parse_expression, parse_assignment_expression, token->type == ',')

// end expressions

static bool is_type_qualifier(struct token *token) {
    // C23(N3096) 6.7.3.1
    return token->type == TOKEN_CONST ||
        token->type == TOKEN_RESTRICT ||
        token->type == TOKEN_VOLATILE ||
        token->type == TOKEN__ATOMIC;
}

static bool is_storage_class(struct token *token) {
    // C23(N3096) 6.7.1.1
    return token->type == TOKEN_AUTO ||
        token->type == TOKEN_CONSTEXPR ||
        token->type == TOKEN_EXTERN ||
        token->type == TOKEN_REGISTER ||
        token->type == TOKEN_STATIC ||
        token->type == TOKEN_THREAD_LOCAL ||
        token->type == TOKEN_TYPEDEF;
}

static bool is_bare_type_specifier(struct token *token) {
    // C23(N3096) 6.7.2.1
    return token->type == TOKEN_VOID ||
        token->type == TOKEN_CHAR ||
        token->type == TOKEN_SHORT ||
        token->type == TOKEN_INT_ ||
        token->type == TOKEN_LONG ||
        token->type == TOKEN_FLOAT ||
        token->type == TOKEN_DOUBLE ||
        token->type == TOKEN_SIGNED ||
        token->type == TOKEN_BOOL ||
        token->type == TOKEN__DECIMAL32 ||
        token->type == TOKEN__DECIMAL64 ||
        token->type == TOKEN__DECIMAL128;
}

static bool is_function_specifier(struct token *token) {
    // C23(N3096) 6.7.4.1
    return token->type == TOKEN_INLINE ||
        token->type == TOKEN__NORETURN;
}

static struct node *parse_type_specifier(struct context *context) {
    struct token *token = TOKEN(context);

    if (is_bare_type_specifier(token)) {
        struct node *node = new(context, NODE_TYPE_SPECIFIER);
        pass(context);
        return node;
    } else {
        return report_error_node(context, "non-basic type specifiers are not yet supported");
    }
}

static struct node *parse_type_specifier_qualifier_list(struct context *context) {
    struct token *token = TOKEN(context);

    while (is_bare_type_specifier(token) || is_type_qualifier(token)) {
    }

    return report_error_node(context, "todo 123123212412");
}

static struct node *parse_direct_declarator(struct context *);
static struct node *parse_single_declaration(struct context *);

static struct node *parse_declarator(struct context *context) {
    struct token *token = TOKEN(context);

    if (token->type == '*') {
        struct node *node = new(context, NODE_DECLARATOR);
        pass(context);
        node->d.inner = parse_declarator(context);
        node->d.name = node->d.inner->d.name;
        return node;
    } else {
        return parse_direct_declarator(context);
    }
}

static struct node *parse_direct_declarator(struct context *context) {
    struct node *node;

    switch (TOKEN(context)->type) {
    case TOKEN_IDENT: {
        struct node *inner = new(context, NODE_DECLARATOR);
        inner->d.name = TOKEN(context);
        pass(context);
        node = inner;
        break;
    }
    case '(': {
        pass(context);
        node = parse_declarator(context);
        node->token_end = TOKEN(context);
        eat(context, ')');
        break;
    }
    default:
        return report_error_node(context, "unable to parse d");
    }

    struct node *inner = nullptr;

    bool cont = true;
    while (cont) {
        switch (TOKEN(context)->type) {
        case '[': {
            inner = new(context, NODE_ARRAY_DECLARATOR);
            pass(context);
            inner->d.inner = node;
            inner->d.name = inner->d.inner->d.name;
            if (TOKEN(context)->type != ']')
                inner->d.arr.subscript = parse_assignment_expression(context);
            node->token_end = TOKEN(context);
            eat(context, ']');
            node = inner;
            break;
        }
        case '(': {
            inner = new(context, NODE_FUNCTION_DECLARATOR);
            eat(context, '(');
            inner->d.inner = node;
            inner->d.name = inner->d.inner->d.name;
            // TODO: args
            int i = 0;
            while (TOKEN(context)->type != ')' && i < MAX_FUNCTION_ARGS) {
                inner->d.fun.args[i++] = parse_single_declaration(context);
                if (TOKEN(context)->type != ')') eat(context, ',');
            }
            node->token_end = TOKEN(context);
            eat(context, ')');
            node = inner;
            break;
        }
        default:
            cont = false;
        }
    }

    return node;
}

static struct node *parse_full_declarator(struct context *context) {
    struct node *inner = parse_declarator(context);
    struct node *expr = nullptr;
    if (TOKEN(context)->type == '=') {
        pass(context);
        expr = parse_assignment_expression(context);
    }

    struct node *node = inner;
    node->d.initializer = expr;
    node->d.full = true;

    return inner;
}

static struct node *parse_static_assert_declaration(struct context *context) {
    struct node* node = new(context, NODE_STATIC_ASSERT);
    pass(context);
    eat(context, '(');
    node->st_assert.expr = parse_assignment_expression(context);
    if (TOKEN(context)->type == ',') {
        eat(context, ',');
        if (TOKEN(context)->type == TOKEN_STRING) {
            node->st_assert.message = report_error_node(context, "static assert string literals not supported");
        } else {
            node->st_assert.message = report_error_node(context, "static assert message must be string literal");
        }
    }
    eat(context, ')');
    node->token_end = TOKEN(context);
    eat(context, ';');
    return node;
}

static struct node *parse_declaration(struct context *context) {
    if (TOKEN(context)->type == TOKEN_STATIC_ASSERT) {
        return parse_static_assert_declaration(context);
    }

    struct node *node = new(context, NODE_DECLARATION);

    node->decl.type = parse_type_specifier(context);
    int i = 0;
    while (TOKEN(context)->type != ';' && i < MAX_DECLARATORS) {
        node->decl.declarators[i++] = parse_full_declarator(context);
        if (TOKEN(context)->type != ';')
            eat(context, ',');
    }
    node->token_end = TOKEN(context);
    eat(context, ';');

    return node;
}

// a "single declaration" contains 0 or 1 declarators and doesn't necessarily end with a ;
// this is for function definitions and parameters.
static struct node *parse_single_declaration(struct context *context) {
    struct node *node = new(context, NODE_DECLARATION);
    node->decl.type = parse_type_specifier(context);

    if (TOKEN(context)->type == '*' || TOKEN(context)->type == '(' || TOKEN(context)->type == TOKEN_IDENT)
        node->decl.declarators[0] = parse_full_declarator(context);

    return node;
}

// parse_other_statements

static struct node *parse_expression_statement(struct context *context) {
    struct node *expr = parse_expression(context);
    eat(context, ';');
    return expr;
}

static struct node *parse_compound_statement(struct context *context) {
    struct node *node = new(context, NODE_BLOCK);
    eat(context, '{');
    int i = 0;
    while (TOKEN(context)->type != '}') {
        node->block.children[i++] = parse_statement(context);
    }
    node->token_end = TOKEN(context);
    eat(context, '}');
    return node;
}

static struct node *parse_label(struct context *context) {
    struct node *node = new(context, NODE_LABEL);
    node->label.name = parse_ident(context);
    node->token_end = TOKEN(context);
    eat(context, ':');
    return node;
}

static struct node *parse_return_statement(struct context *context) {
    struct node *node = new(context, NODE_RETURN);
    pass(context);
    if (TOKEN(context)->type != ';') {
        node->ret.expr = parse_expression(context);
    }
    node->token_end = TOKEN(context);
    eat(context, ';');
    return node;
}

static struct node *parse_null_statement(struct context *context) {
    struct node *node = new(context, NODE_NULL);
    eat(context, ';');
    return node;
}

static struct node *parse_if_statement(struct context *context) {
    struct node *node = new(context, NODE_IF);
    eat(context, TOKEN_IF);
    eat(context, '(');
    struct node *cond = parse_expression(context);
    eat(context, ')');
    struct node *block_true = parse_statement(context);
    struct node *block_false = 0;
    if (TOKEN(context)->type == TOKEN_ELSE) {
        eat(context, TOKEN_ELSE);
        block_false = parse_statement(context);
    }
    node->if_.cond = cond;
    node->if_.block_true = block_true;
    node->if_.block_false = block_false;
    return node;
}

static struct node *parse_while_statement(struct context *context) {
    struct node *node = new(context, NODE_WHILE);
    eat(context, TOKEN_WHILE);
    eat(context, '(');
    struct node *cond = parse_expression(context);
    eat(context, ')');
    struct node *block = parse_statement(context);
    node->while_.cond = cond;
    node->while_.block = block;
    return node;
}

static struct node *parse_statement(struct context *context) {
    switch (TOKEN(context)->type) {
    case '{':
        return parse_compound_statement(context);
    case ';':
        return parse_null_statement(context);
    case TOKEN_STATIC_ASSERT:
        return parse_declaration(context);
    case TOKEN_IDENT:
        if (PEEK(context)->type == ':')
            return parse_label(context);
        else if (is_bare_type_specifier(TOKEN(context)))
            return parse_declaration(context);
        else
            return parse_expression_statement(context);
    case TOKEN_RETURN:
        return parse_return_statement(context);
    case TOKEN_IF:
        return parse_if_statement(context);
    case TOKEN_WHILE:
        return parse_while_statement(context);
    }

    if (is_bare_type_specifier(TOKEN(context)))
        return parse_declaration(context);

    return report_error_node(context, "unknown statement, probably TODO");
}

static struct node *parse_function_definition(struct context *context) {
    struct node *node = new(context, NODE_FUNCTION_DEFINITION);
    node->fun.decl = parse_single_declaration(context);
    node->fun.body = parse_compound_statement(context);
    return node;
}

static struct node *parse_external_definition(struct context *context) {
    enum fun_dec {
        UNKNOWN,
        FUNCTION,
        DECLARATION,
    };
    enum fun_dec this = UNKNOWN;

    for (int i = 0; PEEKN(context, i)->type != TOKEN_EOF; i += 1) {
        if (PEEKN(context, i)->type == '{') {
            this = FUNCTION;
            break;
        }
        if (PEEKN(context, i)->type == '=' ||
                PEEKN(context, i)->type == ';' ||
                PEEKN(context, i)->type == TOKEN_STATIC_ASSERT) {
            this = DECLARATION;
            break;
        }
    }

    switch (this) {
    case UNKNOWN:
        return report_error_node(context, "unknown external definition");
    case DECLARATION:
        return parse_declaration(context);
    case FUNCTION:
        return parse_function_definition(context);
    }
}