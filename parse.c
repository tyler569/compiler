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

    struct {
        struct node *nodes;
        size_t len;
        size_t capacity;
    } ta;
};

// static functions
static bool is_typename(struct context *, struct token *first, size_t count);
static bool more_data(struct context *);
static struct node *new(struct context *, enum node_type);
static int id(struct context *, struct node *);
static void report_error(struct context *, const char *message);
static int report_error_node(struct context *, const char *message);
static void pass(struct context *);
static void eat(struct context *, int token_type, const char *function_name);

static int parse_assignment_expression(struct context *);
static int parse_expression(struct context *);
static int parse_declaration(struct context *);
static int parse_statement(struct context *);
static int parse_external_definition(struct context *);

int parse(struct tu *tu) {
    struct context *context = &(struct context){
        .tu = tu,
        .tokens = tu->tokens,
        .source = tu->source,
    };

    struct node *root = new(context, NODE_ROOT);
    memset(root, 0, sizeof(*root));

    root->token = &(struct token){};
    root->type = NODE_ROOT;
    int n = 0;

    while (more_data(context) && context->errors == 0 && n < MAX_BLOCK_MEMBERS) {
        root->root.children[n++] = parse_external_definition(context);
    }

    tu->nodes = context->ta.nodes;
    tu->nodes_len = context->ta.len;

    return context->errors;
}

static bool more_data(struct context *context) {
    return TOKEN(context)->type != TOKEN_EOF;
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
    if (node == nullptr) {
        exit(1);
    }
    return (int)(node - context->ta.nodes);
}

static void report_error(struct context *context, const char *message) {
    fprintf(stderr, "ast error: %s\n", message);
    print_and_highlight(context->source, TOKEN(context));
    context->errors += 1;

    if (context->tu->abort) exit(1);
}

static int report_error_node(struct context *context, const char *message) {
    struct node *node = new(context, NODE_ERROR);
    fprintf(stderr, "new error: %s\n", message);
    print_and_highlight(context->source, TOKEN(context));

    if (context->tu->abort) exit(1);

    pass(context);
    return id(context, node);
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
static void print_ast_recursive(const char *info, struct tu *tu, int node_id, int level) {
    if (level > 10) exit(1);
    print_space(level);
    if (info) printf("%s ", info);

    struct node *node = tu_node(tu, node_id);
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
        RECUR(node->binop.left);
        RECUR(node->binop.right);
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
    case NODE_DECLARATOR: {
        printf("d: %.*s\n", token->len, &source[token->index]);
        if (node->d.inner) {
            RECUR(node->d.inner);
        }
        break;
    }
    case NODE_FUNCTION_DECLARATOR: {
        printf("d.func:\n");
        RECUR(node->d.inner);
        break;
    }
    case NODE_ARRAY_DECLARATOR: {
        printf("d.array:\n");
        RECUR_INFO("arr:", node->d.inner);
        if (node->d.arr.subscript)
            RECUR_INFO("sub:", node->d.arr.subscript);
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
        RECUR_INFO("ret:", node->fun.ret_type);
        RECUR_INFO("nam:", node->fun.name);
        RECUR_INFO("bdy:", node->fun.body);
        break;
    }
    case NODE_RETURN: {
        printf("return:\n");
        RECUR(node->ret.expr);
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
#undef RECUR
#undef RECUR_INFO

void print_ast(struct tu *tu) {
    print_ast_recursive(nullptr, tu, 0, 0);
}

static int parse_ident(struct context *context) {
    if (TOKEN(context)->type != TOKEN_IDENT)
        return report_error_node(context, "expected an ident, but didn't find it");
    struct node *node = new(context, NODE_IDENT);
    pass(context);
    return id(context, node);
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
            node->funcall.inner = inner;
            int i = 0;
            while (TOKEN(context)->type != ')' && i < MAX_FUNCTION_ARGS) {
                node->funcall.args[i++] = parse_assignment_expression(context);
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
        token->type == TOKEN_SIZEOF || token->type == TOKEN_ALIGNOF) {

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

static int parse_type_specifier(struct context *context) {
    struct token *token = TOKEN(context);

    if (is_bare_type_specifier(token)) {
        struct node *node = new(context, NODE_TYPE_SPECIFIER);
        pass(context);
        return id(context, node);
    } else {
        return report_error_node(context, "non-basic type specifiers are not yet supported");
    }
}

static int parse_type_specifier_qualifier_list(struct context *context) {
    struct token *token = TOKEN(context);

    while (is_bare_type_specifier(token) || is_type_qualifier(token)) {
    }

    return report_error_node(context, "todo 123123212412");
}

static int parse_direct_declarator(struct context *);

static int parse_declarator(struct context *context) {
    struct token *token = TOKEN(context);

    if (token->type == '*') {
        struct node *node = new(context, NODE_DECLARATOR);
        pass(context);
        node->unary_op.inner = parse_declarator(context);
        return id(context, node);
    } else {
        return parse_direct_declarator(context);
    }
}

static int parse_direct_declarator(struct context *context) {
    int node_id = -1;

    switch (TOKEN(context)->type) {
    case TOKEN_IDENT: {
        struct node *node = new(context, NODE_DECLARATOR);
        pass(context);
        node_id = id(context, node);
        break;
    }
    case '(': {
        pass(context);
        node_id = parse_declarator(context);
        eat(context, ')');
        break;
    }
    }

    if (node_id == -1) {
        return report_error_node(context, "unable to parse d");
    }

    struct node *node = nullptr;

    bool cont = true;
    while (cont) {
        switch (TOKEN(context)->type) {
        case '[': {
            node = new(context, NODE_ARRAY_DECLARATOR);
            pass(context);
            node->d.inner = node_id;
            if (TOKEN(context)->type != ']')
                node->d.arr.subscript = parse_assignment_expression(context);
            eat(context, ']');
            node_id = id(context, node);
            break;
        }
        case '(': {
            node = new(context, NODE_FUNCTION_DECLARATOR);
            pass(context);
            node->d.inner = node_id;
            // TODO: args
            eat(context, ')');
            node_id = id(context, node);
            break;
        }
        default:
            cont = false;
        }
    }

    return node_id;
}

static int parse_full_declarator(struct context *context) {
    int node_id = parse_declarator(context);
    int expr = 0;
    if (TOKEN(context)->type == '=') {
        pass(context);
        expr = parse_assignment_expression(context);
    }

    struct node *node = &context->ta.nodes[node_id];
    switch (node->type) {
    case NODE_DECLARATOR:
    case NODE_ARRAY_DECLARATOR:
    case NODE_FUNCTION_DECLARATOR:
        node->d.initializer = expr;
        node->d.full = true;
        break;
    default:
    }

    return node_id;
}

static int parse_static_assert_declaration(struct context *context) {
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
    eat(context, ';');
    return id(context, node);
}

static int parse_declaration(struct context *context) {
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
    eat(context, ';');

    return id(context, node);
}

// parse_other_statements

static int parse_expression_statement(struct context *context) {
    int expr = parse_expression(context);
    eat(context, ';');
    return expr;
}

static int parse_compound_statement(struct context *context) {
    struct node *node = new(context, NODE_BLOCK);
    eat(context, '{');
    int i = 0;
    while (TOKEN(context)->type != '}') {
        node->block.children[i++] = parse_statement(context);
    }
    eat(context, '}');
    return id(context, node);
}

static int parse_label(struct context *context) {
    struct node *node = new(context, NODE_LABEL);
    node->label.name = parse_ident(context);
    eat(context, ':');
    return id(context, node);
}

static int parse_return_statement(struct context *context) {
    struct node *node = new(context, NODE_RETURN);
    pass(context);
    if (TOKEN(context)->type != ';') {
        node->ret.expr = parse_expression(context);
    }
    eat(context, ';');
    return id(context, node);
}

static int parse_statement(struct context *context) {
    switch (TOKEN(context)->type) {
    case '{':
        return parse_compound_statement(context);
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
    // case TOKEN_BREAK:
    //     return parse_break_statement(context);
    // case TOKEN_IF:
    //     return parse_if_statement(context);
    }

    if (is_bare_type_specifier(TOKEN(context)))
        return parse_declaration(context);

    return report_error_node(context, "unknown statement, probably TODO");
}

static int parse_function_definition(struct context *context) {
    struct node *node = new(context, NODE_FUNCTION_DEFINITION);
    node->fun.ret_type = parse_type_specifier(context);
    node->fun.name = parse_ident(context);
    eat(context, '(');
    eat(context, ')');
    node->fun.body = parse_compound_statement(context);
    return id(context, node);
}

static int parse_external_definition(struct context *context) {
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
        if (PEEKN(context, i)->type == '=' || PEEKN(context, i)->type == ';') {
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