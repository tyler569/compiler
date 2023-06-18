#include "type.h"
#include "tu.h"
#include "token.h"
#include "parse.h"
#include "util.h"
#include "diag.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SCOPE(n) (&context->sca.scopes[(n)])
#define TYPE(n) (&context->tya.types[(n)])
#define TOKEN_STR(tok) (&context->tu->source[(tok)->index])

struct context {
    struct tu *tu;
    struct node *root;

    struct {
        size_t len;
        size_t capacity;
        struct type *types;
    } tya;

    struct {
        size_t len;
        size_t capacity;
        struct scope *scopes;
    } sca;
    int errors;
};

int type_recur(struct context *context, struct node *node, int block_depth, int parent_scope);
static int debug_create_type(struct context *context, int parent, enum layer_type base, enum type_flags flags);
static void print_type(struct context *context, int type_id);

static struct type *new_type(struct context *context);
static struct scope *new_scope(struct context *context);

int type(struct tu *tu) {
    struct context *context = &(struct context) {
        .tu = tu,
        .root = tu->nodes,
    };

    // discard index 0, so it can be used for "none"
    (void) new_type(context);
    (void) new_scope(context);

    type_recur(context, context->root, 0, 0);
    tu->scopes = context->sca.scopes;
    tu->scopes_len = context->sca.len;

    return context->errors;
}

static void report_error(struct context *context, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_error(context->tu, message, args);

    va_end(args);
}

static void report_error_node(struct context *context, struct node *node, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_error_node(context->tu, node, message, args);

    va_end(args);
}

static struct scope *new_scope(struct context *context) {
    if (context->sca.capacity <= context->sca.len) {
        size_t new_capacity = context->sca.capacity ? context->sca.capacity * 2 : 512;
        struct scope *new_ta = realloc(context->sca.scopes, new_capacity * sizeof(struct scope));
        if (!new_ta) {
            report_error(context, "memory allocation failed");
            return nullptr;
        }
        context->sca.scopes = new_ta;
        context->sca.capacity = new_capacity;
    }

    struct scope *scope = &context->sca.scopes[context->sca.len++];
    return scope;
}

static struct type *new_type(struct context *context) {
    if (context->tya.capacity <= context->tya.len) {
        size_t new_capacity = context->tya.capacity ? context->tya.capacity * 2 : 512;
        struct type *new_ta = realloc(context->tya.types, new_capacity * sizeof(struct type));
        if (!new_ta) {
            report_error(context, "memory allocation failed");
            return nullptr;
        }
        context->tya.types = new_ta;
        context->tya.capacity = new_capacity;
    }

    struct type *type = &context->tya.types[context->tya.len++];
    return type;
}

int scope_id(struct context *context, struct scope *scope) {
    return (int)(scope - context->sca.scopes);
}

int type_id(struct context *context, struct type *type) {
    return (int)(type - context->tya.types);
}

// This doesn't currently print in C syntax, but it's good enough to know
// what's going on.
static void print_type(struct context *context, int type_id) {
    struct type *type = TYPE(type_id);

#define FLAG(flag_id, flag_name) if (type->flags & (flag_id)) do { fputc(' ', stderr); fputs((flag_name), stderr); } while (0)
    FLAG(TF_ATOMIC, "atomic");
    FLAG(TF_CONST, "const");
    FLAG(TF_VOLATILE, "volatile");
    FLAG(TF_RESTRICT, "restrict");
    FLAG(TF_INLINE, "inline");
    FLAG(TF_NORETURN, "noreturn");
#undef FLAG

#define CASE(type_id, type_name) case (type_id): fputs((type_name), stderr); break
    switch (type->layer) {
    CASE(TYPE_VOID, "void");
    CASE(TYPE_SIGNED_CHAR, "char");
    CASE(TYPE_SIGNED_SHORT, "short");
    CASE(TYPE_SIGNED_INT, "int");
    CASE(TYPE_SIGNED_LONG, "long");
    CASE(TYPE_SIGNED_LONG_LONG, "long long");
    CASE(TYPE_UNSIGNED_CHAR, "unsigned char");
    CASE(TYPE_UNSIGNED_SHORT, "unsigned short");
    CASE(TYPE_UNSIGNED_INT, "unsigned int");
    CASE(TYPE_UNSIGNED_LONG, "unsigned long");
    CASE(TYPE_UNSIGNED_LONG_LONG, "unsigned long long");
    CASE(TYPE_BOOL, "bool");
    CASE(TYPE_FLOAT, "float");
    CASE(TYPE_DOUBLE, "double");
    CASE(TYPE_LONG_DOUBLE, "long double");
    CASE(TYPE_COMPLEX_FLOAT, "complex float");
    CASE(TYPE_COMPLEX_DOUBLE, "complex double");
    CASE(TYPE_COMPLEX_LONG_DOUBLE, "complex long double");
    CASE(TYPE_POINTER, "pointer to");
    CASE(TYPE_ARRAY, "array [] of");
    CASE(TYPE_FUNCTION, "function () returning");
    CASE(TYPE_ENUM, "(enum)");
    CASE(TYPE_STRUCT, "(struct)");
    CASE(TYPE_UNION, "(union)");
    default:
    }
#undef CASE

    if (type->flags & (0xf << TF_ALIGNAS_BIT)) {
        fprintf(stderr, " alignas(%i)", 1 << ((type->flags >> TF_ALIGNAS_BIT) & 0xf));
    }

    if (type->inner) {
        fputc(' ', stderr);
        print_type(context, type->inner);
    }
}

static int debug_create_type(struct context *context, int parent, enum layer_type base, enum type_flags flags) {
    struct type *type = new_type(context);
    type->inner = parent;
    type->layer = base;
    type->flags = flags;
    return type_id(context, type);
}

static const char *base_type_ids[] = {
    [TYPE_VOID] = "void",
    [TYPE_SIGNED_CHAR] = "char",
    [TYPE_SIGNED_SHORT] = "short",
    [TYPE_SIGNED_INT] = "int",
    [TYPE_SIGNED_LONG] = "long",
    [TYPE_FLOAT] = "float",
    [TYPE_DOUBLE] = "double",
    [TYPE_BOOL] = "bool",
};

int find_or_create(struct context *context, int inner, enum layer_type base, enum type_flags flags) {
    for (int i = 0; i < context->tya.len; i += 1) {
        struct type *type = TYPE(i);
        if (type->inner == inner && type->layer == base && type->flags == flags)
            return i;
    }

    struct type *ty = new_type(context);
    ty->layer = base;
    ty->flags = flags;
    ty->inner = inner;

    return type_id(context, ty);
}

int find_or_create_type_inner(struct context *context, int typ, struct node *decl) {
    switch (decl->type) {
    case NODE_DECLARATOR:
        if (!decl->d.inner) {
            return typ;
        } else {
            int layer = find_or_create(context, typ, TYPE_POINTER, 0);
            return find_or_create_type_inner(context, layer, decl->d.inner);
        }
    case NODE_FUNCTION_DECLARATOR: {
        int layer = find_or_create(context, typ, TYPE_FUNCTION, 0);
        return find_or_create_type_inner(context, layer, decl->d.inner);
    }
    case NODE_ARRAY_DECLARATOR: {
        int layer = find_or_create(context, typ, TYPE_ARRAY, 0);
        return find_or_create_type_inner(context, layer, decl->d.inner);
    }
    default:
        report_error_node(context, decl, "invalid declarator type");
        return 0;
    }
}

int find_or_create_type(struct context *context, struct node *type, struct node *decl) {
    enum layer_type b_type = -1;
    for (int i = 0; i < ARRAY_LEN(base_type_ids); i += 1) {
        if (!base_type_ids[i]) continue;
        if (strncmp(base_type_ids[i], &context->tu->source[type->token->index], type->token->len) == 0) {
            b_type = i;
        }
    }
    if (b_type == -1) {
        report_error_node(context, type, "invalid type name");
    }

    int typ = find_or_create(context, 0, b_type, 0);

    int decltype = find_or_create_type_inner(context, typ, decl);
    return decltype;
}

int token_cmp(struct context *context, struct token *a, struct token *b) {
    int len = a->len > b->len ? a->len : b->len;
    return strncmp(&context->tu->source[a->index], &context->tu->source[b->index], len);
}

int resolve_name(struct context *context, struct token *token, int sc) {
    struct scope *scope = SCOPE(sc);

    while (scope && scope->token) {
        if (token_cmp(context, token, scope->token) == 0) {
            return scope_id(context, scope);
        }
        scope = SCOPE(scope->parent);
    }

    return 0;
}

int create_scope(struct context *context, int parent, int c_type, int depth, struct node *decl) {
    struct scope *scope = new_scope(context);

    assert(decl->d.name);

    scope->token = decl->d.name;
    scope->decl = decl;
    scope->parent = parent;
    scope->c_type = c_type;
    scope->block_depth = depth;

    fprintf(stderr, "%.*s has type ", scope->token->len, TOKEN_STR(scope->token));
    print_type(context, c_type);
    fprintf(stderr, "\n");

    return scope_id(context, scope);
}

struct scope *name_exists(struct context *context, struct token *token, int scope_id, int depth) {
    struct scope *scope = SCOPE(scope_id);

    while (scope->block_depth == depth && scope->token) {
        if (token_cmp(context, scope->token, token) == 0)
            return scope;

        scope = SCOPE(scope->parent);
        if (!scope) break;
    }

    return nullptr;
}

// Recursively resolve types and names on the AST.
// If the node creates a new visible name (like a function definition or declaration), return
// a scope ID containing that name, otherwise return 0.
// You can create sub-scopes internally without returning them, for example BLOCK creates
// a scope with a higher block depth and uses it for child recursions, but does not return
// anything because BLOCKs don't create new names visible after them in their peer scope.
int type_recur(struct context *context, struct node *node, int block_depth, int parent_scope) {
    int scope = parent_scope;

    switch (node->type) {
    case NODE_DECLARATION: {
        struct node *base_type = node->decl.type;
        for_each (&node->decl.declarators) {
            struct node *decl = *it;
            struct scope *before;
            if ((before = name_exists(context, decl->d.name, scope, block_depth))) {
                report_error_node(context, decl, "redefinition of name");
                print_info_node(context->tu, before->decl, "previous definition is here");
            }
            int type_id = find_or_create_type(context, base_type, decl);
            scope = create_scope(context, scope, type_id, block_depth, decl);

            decl->d.scope_id = scope;

            if (decl->d.initializer) {
                type_recur(context, decl->d.initializer, block_depth, scope);
            }
        }
        return scope;
    }
    case NODE_ROOT:
        for_each (&node->root.children) {
            int s = type_recur(context, *it, block_depth, scope);
            if (s) scope = s;
        }
        return 0;
    case NODE_BLOCK:
        for_each (&node->block.children) {
            int s = type_recur(context, *it, block_depth + 1, scope);
            if (s) scope = s;
        }
        return 0;
    case NODE_FUNCTION_DEFINITION: {
        int new_outer = type_recur(context, node->fun.decl, block_depth, scope);
        struct node *n = node->fun.decl;
        struct node *d = n->decl.declarators.data[0];
        for_each (&d->d.fun.args) {
            int s = type_recur(context, *it, block_depth + 1, scope);
            if (s) scope = s;
        }
        // function body is a compound statement - that increments block_depth on its own, so
        // this drops back to outer scope to avoid the body of the function being deeper than
        // arguments.
        type_recur(context, node->fun.body, block_depth, scope);

        return new_outer;
    }
    case NODE_BINARY_OP:
        type_recur(context, node->binop.lhs, block_depth, scope);
        type_recur(context, node->binop.rhs, block_depth, scope);
        return 0;
    case NODE_UNARY_OP:
    case NODE_POSTFIX_OP:
        type_recur(context, node->unary_op.inner, block_depth, scope);
        return 0;
    case NODE_SUBSCRIPT:
        type_recur(context, node->subscript.inner, block_depth, scope);
        type_recur(context, node->subscript.subscript, block_depth, scope);
        return 0;
    case NODE_TERNARY:
        type_recur(context, node->ternary.condition, block_depth, scope);
        type_recur(context, node->ternary.branch_true, block_depth, scope);
        type_recur(context, node->ternary.branch_false, block_depth, scope);
        return 0;
    case NODE_RETURN:
        type_recur(context, node->ret.expr, block_depth, scope);
        return 0;
    case NODE_IDENT: {
        int scope_id = resolve_name(context, node->token, scope);
        if (!scope_id) {
            report_error_node(context, node, "undeclared identifier");
            exit(1);
        }
        fprintf(stderr, "resolving %.*s (line %i) to ", node->token->len, TOKEN_STR(node->token), node->token->line);
        print_type(context, SCOPE(scope_id)->c_type);
        fprintf(stderr, " declared on line %i\n", SCOPE(scope_id)->token->line);
        fprintf(stderr, "block depth is %i\n", block_depth);
        node->ident.scope_id = scope_id;
        return 0;
    }
    case NODE_IF: {
        type_recur(context, node->if_.cond, block_depth, scope);
        type_recur(context, node->if_.block_true, block_depth + 1, scope);
        if (node->if_.block_false)
            type_recur(context, node->if_.block_false, block_depth + 1, scope);
        return 0;
    }
    case NODE_WHILE: {
        type_recur(context, node->while_.cond, block_depth, scope);
        type_recur(context, node->while_.block, block_depth + 1, scope);
        return 0;
    }
    default:
        return 0;
    }
}