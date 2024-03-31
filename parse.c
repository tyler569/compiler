#include "parse.h"
#include "token.h"
#include "util.h"
#include "diag.h"
#include "type.h"
#include "tu.h"

#include <stdarg.h>
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
};


// static functions
static bool is_typename(struct context *, struct token *first, size_t count);
static bool more_data(struct context *);
static struct node *new(struct context *, enum node_type);
static void report_error(struct context *, const char *message, ...);
static struct node *report_error_node(struct context *, const char *message, ...);
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

    while (more_data(context) && context->errors == 0) {
        list_push(&root->root.children, parse_external_definition(context));
    }

    tu->ast_root = root;

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

static void report_error(struct context *context, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_error_token(context->tu, TOKEN(context), message, args);

    context->errors += 1;

    va_end(args);
}

static struct node *report_error_node(struct context *context, const char *message, ...) {
    va_list args;
    va_start(args, message);

    struct node *node = new(context, NODE_ERROR);

    vprint_error_node(context->tu, node, message, args);

    context->errors += 1;
    pass(context);

    va_end(args);

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
        print_error_token(context->tu, TOKEN(context),
                          "expected '%s', found '%s' in %s",
                          token_type_string(token_type),
                          token_type_string(TOKEN(context)->type),
                          function_name);
    }
    pass(context);
}

static void fast_forward(struct context *context, int token_type) {
}

#define eat(ctx, typ) eat(ctx, typ, __func__)

static void print_space(int level) {
    for (int i = 0; i < level; i++) fputs("  ", stderr);
}

const char *node_type_strings[NODE_TYPE_COUNT] = {
    [NODE_NULL] = "NODE_NULL",
    [NODE_ROOT] = "NODE_ROOT",
    [NODE_BINARY_OP] = "NODE_BINARY_OP",
    [NODE_UNARY_OP] = "NODE_UNARY_OP",
    [NODE_POSTFIX_OP] = "NODE_POSTFIX_OP",
    [NODE_IDENT] = "NODE_IDENT",
    [NODE_INT_LITERAL] = "NODE_INT_LITERAL",
    [NODE_FLOAT_LITERAL] = "NODE_FLOAT_LITERAL",
    [NODE_STRING_LITERAL] = "NODE_STRING_LITERAL",
    [NODE_ERROR] = "NODE_ERROR",
    [NODE_MEMBER] = "NODE_MEMBER",
    [NODE_SUBSCRIPT] = "NODE_SUBSCRIPT",
    [NODE_TERNARY] = "NODE_TERNARY",
    [NODE_FUNCTION_CALL] = "NODE_FUNCTION_CALL",
    [NODE_DECLARATION] = "NODE_DECLARATION",
    [NODE_TYPE_SPECIFIER] = "NODE_TYPE_SPECIFIER",
    [NODE_DECLARATOR] = "NODE_DECLARATOR",
    [NODE_ARRAY_DECLARATOR] = "NODE_ARRAY_DECLARATOR",
    [NODE_FUNCTION_DECLARATOR] = "NODE_FUNCTION_DECLARATOR",
    [NODE_FUNCTION_DEFINITION] = "NODE_FUNCTION_DEFINITION",
    [NODE_STATIC_ASSERT] = "NODE_STATIC_ASSERT",
    [NODE_BLOCK] = "NODE_BLOCK",
    [NODE_LABEL] = "NODE_LABEL",
    [NODE_RETURN] = "NODE_RETURN",
    [NODE_IF] = "NODE_IF",
    [NODE_WHILE] = "NODE_WHILE",
    [NODE_DO] = "NODE_DO",
    [NODE_FOR] = "NODE_FOR",
    [NODE_GOTO] = "NODE_GOTO",
    [NODE_SWITCH] = "NODE_SWITCH",
    [NODE_CASE] = "NODE_CASE",
    [NODE_BREAK] = "NODE_BREAK",
    [NODE_CONTINUE] = "NODE_CONTINUE",
    [NODE_DEFAULT] = "NODE_DEFAULT",
    [NODE_STRUCT] = "NODE_STRUCT",
    [NODE_ENUM] = "NODE_ENUM",
    [NODE_UNION] = "NODE_UNION",
};

#define TOKEN_STR(token) (&(tu)->source[(token)->index])
#define PRINT_TOKEN(token) fprintf(stderr, "%.*s", (token)->len, TOKEN_STR(token))

static void print_dcl_flat(struct tu *tu, struct node *node) {
    if (node->type != NODE_DECLARATOR) {
        print_internal_error(tu, "attempting to print non-d with print_dcl_flat");
        return;
    }

    if (node->d.inner) {
        print_dcl_flat(tu, node->d.inner);
        fprintf(stderr, " -> ");
    }

    if (node->d.name) {
        PRINT_TOKEN(node->d.name);
    } else if (node->d.nameless) {
        fprintf(stderr, "(nameless)");
    }
}

static void print_dcl_list(struct tu *tu, node_list_t *nodes) {
    fprintf(stderr, "print_dcl_list: ");
    for_each (nodes) {
        struct node *d = list_first(&(*it)->decl.declarators);
        if (d) {
            print_dcl_flat(tu, d);
        }
        fprintf(stderr, " ");
        print_type(tu, (*it)->decl.decl_spec_c_type);

        if (!list_islast(nodes, *it)) {
            fprintf(stderr, ", ");
        }
    }
}

#define RECUR(node) print_ast_recursive(nullptr, tu, (node), level + 1)
#define RECUR_INFO(info, node) print_ast_recursive((info), tu, (node), level + 1)
static void print_ast_recursive(const char *info, struct tu *tu, struct node *node, int level) {
    print_space(level);
    if (info) fprintf(stderr, "%s ", info);
    if (!node) {
        print_internal_error(tu, "ast node is nullptr");
        return;
    }
    if (level > 50) {
        print_internal_error(tu, "ast more than 50 levels deep, loop?");
        exit(1);
    }
    if (node == tu->ast_root && level > 0) {
        print_internal_error(tu, "found root node in non-root position");
        exit(1);
    }

    struct token *token = node->token;
    const char *source = tu->source;

    switch (node->type) {
    case NODE_ROOT: {
        fprintf(stderr, "root:\n");
        for_each (&node->root.children) {
            RECUR(*it);
        }
        break;
    }
    case NODE_BLOCK: {
        fprintf(stderr, "block:\n");
        for_each (&node->block.children) {
            RECUR(*it);
        }
        break;
    }
    case NODE_INT_LITERAL: {
        fprintf(stderr, "int: %.*s (%llu)\n", token->len, &source[token->index], token->int_.value);
        break;
    }
    case NODE_FLOAT_LITERAL: {
        fprintf(stderr, "float: %.*s (%f)\n", token->len, &source[token->index], token->float_.value);
        break;
    }
    case NODE_STRING_LITERAL:
        fprintf(stderr, "string: %.*s\n", token->len, &source[token->index]);
        break;
    case NODE_IDENT: {
        fprintf(stderr, "ident: %.*s\n", token->len, &source[token->index]);
        break;
    }
    case NODE_BINARY_OP: {
        fprintf(stderr, "binop: %.*s\n", token->len, &source[token->index]);
        RECUR(node->binop.lhs);
        RECUR(node->binop.rhs);
        break;
    }
    case NODE_UNARY_OP: {
        fprintf(stderr, "unop: %.*s\n", token->len, &source[token->index]);
        RECUR(node->unary_op.inner);
        break;
    }
    case NODE_POSTFIX_OP: {
        fprintf(stderr, "postfix: %.*s\n", token->len, &source[token->index]);
        RECUR(node->unary_op.inner);
        break;
    }
    case NODE_SUBSCRIPT: {
        fprintf(stderr, "subscript:\n");
        RECUR_INFO("arr:", node->subscript.inner);
        RECUR_INFO("sub:", node->subscript.subscript);
        break;
    }
    case NODE_TERNARY: {
        fprintf(stderr, "ternary:\n");
        RECUR_INFO("cnd:", node->ternary.condition);
        RECUR_INFO("tru:", node->ternary.branch_true);
        RECUR_INFO("fls:", node->ternary.branch_false);
        break;
    }
    case NODE_FUNCTION_CALL: {
        fprintf(stderr, "funcall:\n");
        RECUR_INFO("fun:", node->funcall.inner);
        for_each (&node->funcall.args) {
            RECUR_INFO("arg", *it);
        }
        break;
    }
    case NODE_DECLARATION: {
        fprintf(stderr, "decl:\n");
        print_space(level + 1);
        fprintf(stderr, "typ: ");
        print_type(tu, node->decl.decl_spec_c_type);
        fprintf(stderr, "\n");
        for_each (&node->decl.declarators) {
            RECUR_INFO("dcl:", *it);
        }
        break;
    }
    case NODE_TYPE_SPECIFIER: {
        fprintf(stderr, "decl_spec: %.*s\n", token->len, &source[token->index]);
        break;
    }
    case NODE_DECLARATOR:
    case NODE_FUNCTION_DECLARATOR:
    case NODE_ARRAY_DECLARATOR: {
        fprintf(stderr, "d: ");
        struct node *n = node;
        while (true) {
            token = n->token;
            if (n->type == NODE_DECLARATOR) {
                fprintf(stderr, "%.*s", token->len, &source[token->index]);
            } else if (n->type == NODE_FUNCTION_DECLARATOR) {
                fprintf(stderr, "()");
            } else if (n->type == NODE_ARRAY_DECLARATOR) {
                fprintf(stderr, "[]");
            }

            if (!n->d.inner) {
                fprintf(stderr, "\n");
                break;
            }
            fprintf(stderr, " -> ");
            n = n->d.inner;
        }
        if (node->d.initializer)
            RECUR_INFO("ini:", node->d.initializer);
        break;
    }
    case NODE_STATIC_ASSERT: {
        fprintf(stderr, "static assert:\n");
        RECUR_INFO("tst:", node->st_assert.expr);
        if (node->st_assert.message)
            RECUR_INFO("msg:", node->st_assert.message);
        break;
    }
    case NODE_FUNCTION_DEFINITION: {
        fprintf(stderr, "function:\n");
        RECUR_INFO("typ:", node->fun.decl);
        RECUR_INFO("bdy:", node->fun.body);
        break;
    }
    case NODE_RETURN:
        fprintf(stderr, "return:\n");
        RECUR(node->ret.expr);
        break;
    case NODE_IF:
        fprintf(stderr, "if:\n");
        RECUR_INFO("cnd:", node->if_.cond);
        RECUR_INFO("yes:", node->if_.block_true);
        if (node->if_.block_false)
            RECUR_INFO("no: ", node->if_.block_false);
        break;
    case NODE_WHILE:
        fprintf(stderr, "while:\n");
        RECUR_INFO("cnd:", node->while_.cond);
        RECUR_INFO("blk:", node->while_.block);
        break;
    case NODE_NULL:
        fprintf(stderr, "null:\n");
        break;
    case NODE_ERROR:
        fprintf(stderr, "error: %.*s\n", token->len, &source[token->index]);
        break;
    case NODE_MEMBER:
        fprintf(stderr, "member:\n");
        RECUR_INFO("val:", node->member.inner);
        RECUR_INFO("nam:", node->member.ident);
        break;
    case NODE_LABEL:
        fprintf(stderr, "label:\n");
        RECUR(node->label.name);
        break;
    case NODE_DO:
        fprintf(stderr, "do:\n");
        RECUR_INFO("blk:", node->do_.block);
        RECUR_INFO("cnd:", node->do_.cond);
        break;
    case NODE_FOR:
        fprintf(stderr, "for:\n");
        RECUR_INFO("ini:", node->for_.init);
        RECUR_INFO("cnd:", node->for_.cond);
        RECUR_INFO("nxt:", node->for_.next);
        RECUR_INFO("blk:", node->for_.block);
        break;
    case NODE_GOTO:
        fprintf(stderr, "goto:\n");
        RECUR(node->goto_.label);
        break;
    case NODE_SWITCH:
        fprintf(stderr, "switch:\n");
        RECUR_INFO("exp:", node->switch_.expr);
        RECUR_INFO("blk:", node->switch_.block);
        break;
    case NODE_CASE:
        fprintf(stderr, "case:\n");
        RECUR(node->case_.value);
        break;
    case NODE_CONTINUE:
        fprintf(stderr, "continue:\n");
        break;
    case NODE_BREAK:
        fprintf(stderr, "break:\n");
        break;
    case NODE_DEFAULT:
        fprintf(stderr, "default:\n");
        break;
    case NODE_STRUCT:
        fprintf(stderr, "struct:\n");
        for_each (&node->struct_.decls) {
            RECUR(*it);
        }
        break;
    case NODE_UNION:
        fprintf(stderr, "union:\n");
        for_each (&node->struct_.decls) {
            RECUR(*it);
        }
        break;
    default:
        fprintf(stderr, "UNKNOWN:\n");
        break;
    }
}
#undef RECUR
#undef RECUR_INFO

void print_ast(struct tu *tu) {
    print_ast_recursive(nullptr, tu, tu->ast_root, 0);
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
        if (node->d.inner)
            return node_end(node->d.inner);
        else
            return node->token;
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
    eat(context, TOKEN_IDENT);
    return node;
}

static struct node *parse_primary_expression(struct context *context) {
    switch (TOKEN(context)->type) {
    case TOKEN_INT_LITERAL: {
        struct node *node = new(context, NODE_INT_LITERAL);
        pass(context);
        return node;
    }
    case TOKEN_FLOAT_LITERAL: {
        struct node *node = new(context, NODE_FLOAT_LITERAL);
        pass(context);
        return node;
    }
    case TOKEN_IDENT: {
        struct node *node = new(context, NODE_IDENT);
        pass(context);
        return node;
    }
    case TOKEN_STRING_LITERAL: {
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
            node->member.ident = parse_ident(context);
            node->token_end = TOKEN(context);
            pass(context);
            inner = node;
            break;
        }
        case '(': {
            struct node *node = new(context, NODE_FUNCTION_CALL);
            pass(context);
            node->funcall.inner = inner;
            while (TOKEN(context)->type != ')') {
                list_push(&node->funcall.args, parse_assignment_expression(context));
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
PARSE_BINOP(parse_and, parse_bitor, token->type == TOKEN_AND_AND)
PARSE_BINOP(parse_or, parse_and, token->type == TOKEN_OR_OR)

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
           token->type == TOKEN_INT ||
        token->type == TOKEN_LONG ||
           token->type == TOKEN_FLOAT_LITERAL ||
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

static bool is_typedef(struct context *context, struct token *token) {
    return false;
}

static bool is_declaration_specifier(struct context *context, struct token *token) {
     return is_type_qualifier(token) ||
         is_type_qualifier(token) ||
         is_bare_type_specifier(token) ||
         is_storage_class(token) ||
         is_function_specifier(token); // || is_typedef(context, token);
}

static bool begins_type_name(struct context *context, struct token *token) {
    // return is_type_qualifier(token) ||
    //     is_type_qualifier(token) ||
    //     is_bare_type_specifier(token) ||
    //     is_function_specifier(token) ||
    //     is_typedef(context, token);
    return is_bare_type_specifier(token) || token->type == TOKEN_STRUCT || token->type == TOKEN_UNION ||
        is_declaration_specifier(context, token);
}

static struct node *parse_struct(struct context *context) {
    struct node *node;
    if (TOKEN(context)->type == TOKEN_STRUCT)
        node = new(context, NODE_STRUCT);
    else if (TOKEN(context)->type == TOKEN_UNION)
        node = new(context, NODE_UNION);
    else return report_error_node(context, "not a struct");
    pass(context);

    if (TOKEN(context)->type == TOKEN_IDENT) {
        struct node *name = parse_ident(context);
        node->struct_.name = name;
    }
    if (TOKEN(context)->type == ';') {
        eat(context, ';');
        return node;
    }
    eat(context, '{');
    while (TOKEN(context)->type != '}') {
        struct node *n = parse_declaration(context);
        list_push(&node->struct_.decls, n);
    }
    node->token_end = TOKEN(context);
    eat(context, '}');
    return node;
}

static struct node *parse_type_specifier(struct context *context) {
    struct token *token = TOKEN(context);

    if (is_bare_type_specifier(token)) {
        struct node *node = new(context, NODE_TYPE_SPECIFIER);
        pass(context);
        return node;
    } else if (TOKEN(context)->type == TOKEN_STRUCT || TOKEN(context)->type == TOKEN_UNION) {
        return parse_struct(context);
    } else {
        return report_error_node(context, "non-basic decl_spec specifiers are not yet supported");
    }
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
    case ',':
    case ';':
    case ')': {
        print_info_token(context->tu, TOKEN(context), "interpreting this as a nameless declarator");
        struct node *inner = new(context, NODE_DECLARATOR);
        inner->d.name = nullptr;
        inner->d.nameless = true;
        node = inner;
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
            while (TOKEN(context)->type != ')') {
                list_push(&inner->d.fun.args, parse_single_declaration(context));
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
        if (TOKEN(context)->type == TOKEN_STRING_LITERAL) {
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

enum parse_state {
    SEEN_TOKEN_CHAR = (1 << 0),
    SEEN_TOKEN_SHORT = (1 << 1),
    SEEN_TOKEN_LONG = (1 << 2),
    SEEN_TOKEN_LONG_TWICE = (1 << 3),
    SEEN_TOKEN_INT = (1 << 4),
    SEEN_TOKEN_SIGNED = (1 << 5),
    SEEN_TOKEN_UNSIGNED = (6 << 6),
    SEEN_TOKEN_FLOAT = (1 << 7),
    SEEN_TOKEN_DOUBLE = (1 << 8),
    SEEN_TOKEN_COMPLEX = (1 << 9),

    SEEN_FLOAT = SEEN_TOKEN_FLOAT | SEEN_TOKEN_DOUBLE | SEEN_TOKEN_COMPLEX,
};

static bool incompatible_type_token(enum parse_state state, struct token *token) {
    switch (token->type) {
        case TOKEN_CHAR:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_SHORT | SEEN_TOKEN_LONG | SEEN_FLOAT)) != 0;
        case TOKEN_SHORT:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_LONG | SEEN_FLOAT)) != 0;
        case TOKEN_LONG:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_LONG_TWICE | SEEN_TOKEN_FLOAT)) != 0;
        case TOKEN_INT:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_INT | SEEN_FLOAT)) != 0;
        case TOKEN_SIGNED:
            return (state & (SEEN_TOKEN_UNSIGNED | SEEN_FLOAT)) != 0;
        case TOKEN_UNSIGNED:
            return (state & (SEEN_TOKEN_SIGNED | SEEN_FLOAT)) != 0;
        case TOKEN_FLOAT:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_SHORT | SEEN_TOKEN_LONG | SEEN_TOKEN_INT |
                SEEN_TOKEN_SIGNED | SEEN_TOKEN_UNSIGNED)) != 0;
        case TOKEN_DOUBLE:
            return (state & (SEEN_TOKEN_CHAR | SEEN_TOKEN_SHORT | SEEN_TOKEN_INT | SEEN_TOKEN_SIGNED |
            SEEN_TOKEN_UNSIGNED)) != 0;
        default:
            return false;
    }
}

static struct node *parse_declaration_specifier_list(struct context *context, struct node *node) {
    enum layer_type base_type = 0;
    enum type_flags type_flags = 0;
    enum storage_class sc = 0;

    enum parse_state state = 0;

    while (is_declaration_specifier(context, TOKEN(context))) {
        if (incompatible_type_token(state, TOKEN(context))) {
            return report_error_node(context, "invalid combination of declaration specifiers");
        }

        switch (TOKEN(context)->type) {
        case TOKEN_STRUCT:
        case TOKEN_UNION: {
            base_type = TYPE_STRUCT;
            struct node *struct_ = parse_struct(context);
        case TOKEN_ENUM:
            return report_error_node(context, "constructing struct and enum types is not yet supported");
        }

        case TOKEN_CONST:
            type_flags |= TF_CONST;
            break;
        case TOKEN_VOLATILE:
            type_flags |= TF_VOLATILE;
            break;
        case TOKEN__ATOMIC:
            type_flags |= TF_ATOMIC;
            break;
        case TOKEN_RESTRICT:
            type_flags |= TF_RESTRICT;
            break;
        case TOKEN_INLINE:
            type_flags |= TF_INLINE;
            break;
        case TOKEN__NORETURN:
            type_flags |= TF_NORETURN;
            break;

        case TOKEN_AUTO:

        case TOKEN_CONSTEXPR:
            if (sc != 0) goto error;
            sc = ST_CONSTEXPR;
            break;
        case TOKEN_EXTERN:
            if (sc != 0) goto error;
            sc = ST_EXTERNAL;
            break;
        case TOKEN_REGISTER:
            if (sc != 0) goto error;
            sc = ST_REGISTER;
            break;
        case TOKEN_STATIC:
            if (sc != 0) goto error;
            sc = ST_STATIC;
            break;
        case TOKEN_THREAD_LOCAL:
            if (sc != 0) goto error;
            sc = ST_THREAD_LOCAL;
            break;
        case TOKEN_TYPEDEF:
            if (sc != 0) goto error;
            sc = ST_TYPEDEF;
            break;

        case TOKEN_CHAR:
            state |= SEEN_TOKEN_CHAR;
            if (base_type == TYPE_UNSIGNED_INT) base_type = TYPE_UNSIGNED_CHAR;
            else if (base_type == TYPE_SIGNED_INT) base_type = TYPE_SIGNED_CHAR;
            else if (base_type == 0) base_type = TYPE_SIGNED_CHAR;
            else goto error;
            break;
        case TOKEN_SHORT:
            state |= SEEN_TOKEN_SHORT;
            if (base_type == TYPE_SIGNED_INT) base_type = TYPE_SIGNED_SHORT;
            else if (base_type == TYPE_UNSIGNED_INT) base_type = TYPE_UNSIGNED_SHORT;
            else if (base_type == 0) base_type = TYPE_SIGNED_SHORT;
            else goto error;
            break;
        case TOKEN_LONG:
            state |= SEEN_TOKEN_LONG;
            if (base_type == TYPE_SIGNED_INT) base_type = TYPE_SIGNED_LONG;
            else if (base_type == TYPE_UNSIGNED_INT) base_type = TYPE_UNSIGNED_LONG;
            else if (base_type == TYPE_SIGNED_LONG) base_type = TYPE_SIGNED_LONG_LONG;
            else if (base_type == TYPE_UNSIGNED_LONG) base_type = TYPE_UNSIGNED_LONG_LONG;
            else if (base_type == 0) base_type = TYPE_SIGNED_LONG;
            else goto error;
            break;
        case TOKEN_INT:
            state |= SEEN_TOKEN_INT;
            if (base_type == TYPE_SIGNED_SHORT) {}
            else if (base_type == TYPE_SIGNED_INT) {}
            else if (base_type == TYPE_SIGNED_LONG) {}
            else if (base_type == TYPE_SIGNED_LONG_LONG) {}
            else if (base_type == TYPE_UNSIGNED_SHORT) {}
            else if (base_type == TYPE_UNSIGNED_INT) {}
            else if (base_type == TYPE_UNSIGNED_LONG) {}
            else if (base_type == TYPE_UNSIGNED_LONG_LONG) {}
            else if (base_type == 0) base_type = TYPE_SIGNED_INT;
            else goto error;
            break;
        case TOKEN_SIGNED:
            state |= SEEN_TOKEN_SIGNED;
            if (base_type == TYPE_SIGNED_SHORT) {}
            else if (base_type == TYPE_SIGNED_INT) {}
            else if (base_type == TYPE_SIGNED_LONG) {}
            else if (base_type == TYPE_SIGNED_LONG_LONG) {}
            else if (base_type == 0) base_type = TYPE_SIGNED_INT;
            else goto error;
            break;
        case TOKEN_UNSIGNED:
            state |= SEEN_TOKEN_UNSIGNED;
            if (base_type == TYPE_UNSIGNED_SHORT) {}
            else if (base_type == TYPE_UNSIGNED_INT) {}
            else if (base_type == TYPE_UNSIGNED_LONG) {}
            else if (base_type == TYPE_UNSIGNED_LONG_LONG) {}
            else if (base_type == 0) base_type = TYPE_UNSIGNED_INT;
            else goto error;
            break;
        case TOKEN_FLOAT:
            state |= SEEN_TOKEN_FLOAT;
            if (base_type == 0) base_type = TYPE_FLOAT;
            else goto error;
            break;
        case TOKEN_DOUBLE:
            state |= SEEN_TOKEN_DOUBLE;
            if (base_type == TYPE_SIGNED_LONG) base_type = TYPE_LONG_DOUBLE;
            else if (base_type == 0) base_type = TYPE_DOUBLE;
            else goto error;
            break;
        default:
            return report_error_node(context, "invalid type parse state");
        }
        pass(context);
    }

    int decl_spec_c_type = find_or_create_type(context->tu, 0, base_type, type_flags);
    if (!decl_spec_c_type) goto error;
    node->decl.decl_spec_c_type = decl_spec_c_type;
    node->decl.sc = sc;
    return nullptr;

error:
    return report_error_node(context, "invalid type parse state");
}

static struct node *parse_declaration(struct context *context) {
    if (TOKEN(context)->type == TOKEN_STATIC_ASSERT) {
        return parse_static_assert_declaration(context);
    }

    struct node *node = new(context, NODE_DECLARATION);

    struct node *err = parse_declaration_specifier_list(context, node);
    if (err) return err;

    while (TOKEN(context)->type != ';') {
        list_push(&node->decl.declarators, parse_full_declarator(context));
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

    struct node *err = parse_declaration_specifier_list(context, node);
    if (err) return err;

    list_init_one(&node->decl.declarators);

    if (TOKEN(context)->type == '*' || TOKEN(context)->type == '(' || TOKEN(context)->type == TOKEN_IDENT)
        node->decl.declarators.data[0] = parse_declarator(context);

    return node;
}

// parse_other_statements

static struct node *parse_expression_statement(struct context *context) {
    struct node *expr = parse_expression(context);
    eat(context, ';');
    if (TOKEN(context)->type == TOKEN_COMMENT) {
        expr->attached_comment = TOKEN(context);
        pass(context);
    }
    return expr;
}

static struct node *parse_compound_statement(struct context *context) {
    struct node *node = new(context, NODE_BLOCK);
    eat(context, '{');
    while (TOKEN(context)->type != '}') {
        list_push(&node->block.children, parse_statement(context));
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

static struct node *parse_do_statement(struct context *context) {
    struct node *node = new(context, NODE_DO);
    eat(context, TOKEN_DO);
    struct node *block = parse_statement(context);
    eat(context, TOKEN_WHILE);
    eat(context, '(');
    struct node *cond = parse_expression(context);
    eat(context, ')');
    node->token_end = TOKEN(context);
    eat(context, ';');
    node->do_.block = block;
    node->do_.cond = cond;
    return node;
}

static struct node *parse_for_statement(struct context *context) {
    struct node *node = new(context, NODE_FOR);
    eat(context, TOKEN_FOR);
    eat(context, '(');
    struct node *init = nullptr, *cond = nullptr, *next = nullptr;
    if (TOKEN(context)->type != ';') {
        if (begins_type_name(context, TOKEN(context))) {
            init = parse_declaration(context);
        } else {
            init = parse_expression(context);
            eat(context, ';');
        }
    } else {
        eat(context, ';');
    }
    if (TOKEN(context)->type != ';') {
        cond = parse_expression(context);
    }
    eat(context, ';');
    if (TOKEN(context)->type != ')') {
        next = parse_expression(context);
    }
    eat(context, ')');
    struct node *block = parse_statement(context);
    node->for_.init = init;
    node->for_.cond = cond;
    node->for_.next = next;
    node->for_.block = block;
    return node;
}

static struct node *parse_switch_statement(struct context *context) {
    struct node *node = new(context, NODE_SWITCH);
    eat(context, TOKEN_SWITCH);
    eat(context, '(');
    struct node *expr = parse_expression(context);
    eat(context, ')');
    struct node *block = parse_statement(context);
    node->switch_.expr = expr;
    node->switch_.block = block;
    return node;
}

static struct node *parse_case_statement(struct context *context) {
    struct node *node = new(context, NODE_CASE);
    eat(context, TOKEN_CASE);
    struct node *value = parse_expression(context);
    eat(context, ':');
    node->case_.value = value;

    return node;
}

static struct node *parse_goto_statement(struct context *context) {
    struct node *node = new(context, NODE_GOTO);
    eat(context, TOKEN_GOTO);
    struct node *ident = parse_ident(context);
    eat(context, ';');
    node->goto_.label = ident;
    return node;
}

static struct node *parse_break_statement(struct context *context) {
    struct node *node = new(context, NODE_BREAK);
    eat(context, TOKEN_BREAK);
    eat(context, ';');
    return node;
}

static struct node *parse_continue_statement(struct context *context) {
    struct node *node = new(context, NODE_CONTINUE);
    eat(context, TOKEN_CONTINUE);
    eat(context, ';');
    return node;
}

static struct node *parse_default_statement(struct context *context) {
    struct node *node = new(context, NODE_DEFAULT);
    eat(context, TOKEN_DEFAULT);
    eat(context, ':');
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
        else if (begins_type_name(context, TOKEN(context)))
            return parse_declaration(context);
        else
            return parse_expression_statement(context);
    case TOKEN_RETURN:
        return parse_return_statement(context);
    case TOKEN_IF:
        return parse_if_statement(context);
    case TOKEN_WHILE:
        return parse_while_statement(context);
    case TOKEN_DO:
        return parse_do_statement(context);
    case TOKEN_FOR:
        return parse_for_statement(context);
    case TOKEN_SWITCH:
        return parse_switch_statement(context);
    case TOKEN_CASE:
        return parse_case_statement(context);
    case TOKEN_GOTO:
        return parse_goto_statement(context);
    case TOKEN_BREAK:
        return parse_break_statement(context);
    case TOKEN_CONTINUE:
        return parse_continue_statement(context);
    case TOKEN_DEFAULT:
        return parse_default_statement(context);
    }

    if (begins_type_name(context, TOKEN(context)))
        return parse_declaration(context);

    return parse_expression_statement(context);

    // return report_error_node(context, "unknown statement, probably TODO");
}

static struct node *parse_function_definition(struct context *context) {
    struct node *node = new(context, NODE_FUNCTION_DEFINITION);
    node->fun.decl = parse_single_declaration(context);
    node->fun.body = parse_compound_statement(context);
    node->fun.d = node->fun.decl->decl.declarators.data[0];
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