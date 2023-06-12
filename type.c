#include "type.h"
#include "tu.h"
#include "token.h"
#include "parse.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SCOPE(n) (&context->sca.scopes[(n)])
#define NODE(n) (&context->root[(n)])
#define TYPE(n) (&context->tya.types[(n)])
#define TOKEN_STR(tok) (&context->tu->source[(tok)->index])

struct scope {
    struct token *token;
    struct node *decl;
    int c_type;
    int parent;
    int block_depth;
};

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
static int debug_create_type(struct context *context, int parent, enum base_type base, enum type_flags flags);
static void print_type(struct context *context, int type_id);

static struct type *new_type(struct context *context);
static struct scope *new_scope(struct context *context);

int type(struct tu *tu) {
    struct context *context = &(struct context) {
        .tu = tu,
        .root = tu->nodes,
    };

    // discard index 0 so it can be used for "none"
    (void) new_type(context);
    (void) new_scope(context);

    type_recur(context, context->root, 0, 0);

    for (int i = 0; i < context->tya.len; i += 1) {
        print_type(context, i);
        putc('\n', stderr);
    }

    return context->errors;
}

static void report_error(struct context *context, const char *message) {
    fprintf(stderr, "typer error: %s\n", message);
    context->errors += 1;
    exit(1);
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

    if (type->inner) {
        print_type(context, type->inner);
        fputc(' ', stderr);
    }

#define CASE(type_id, type_name) case (type_id): fputs((type_name), stderr); break
    switch (type->base) {
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
    CASE(TYPE_POINTER, "*");
    CASE(TYPE_ARRAY, "[]");
    CASE(TYPE_FUNCTION, "()");
    CASE(TYPE_ENUM, "(enum)");
    CASE(TYPE_STRUCT, "(struct)");
    CASE(TYPE_UNION, "(union)");
    }
#undef CASE

#define FLAG(flag_id, flag_name) if (type->flags & (flag_id)) do { fputc(' ', stderr); fputs((flag_name), stderr); } while (0)
    FLAG(TF_ATOMIC, "atomic");
    FLAG(TF_CONST, "const");
    FLAG(TF_VOLATILE, "volatile");
    FLAG(TF_RESTRICT, "restrict");
    FLAG(TF_INLINE, "inline");
    FLAG(TF_NORETURN, "noreturn");
#undef FLAG

    if (type->flags & (0xf << TF_ALIGNAS_BIT)) {
        fprintf(stderr, " alignas(%i)", 1 << ((type->flags >> TF_ALIGNAS_BIT) & 0xf));
    }
}

static int debug_create_type(struct context *context, int parent, enum base_type base, enum type_flags flags) {
    struct type *type = new_type(context);
    type->inner = parent;
    type->base = base;
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

int find_or_create(struct context *context, int inner, enum base_type base, enum type_flags flags) {
    for (int i = 0; i < context->tya.len; i += 1) {
        struct type *type = TYPE(i);
        if (type->inner == inner && type->base == base && type->flags == flags)
            return i;
    }

    struct type *ty = new_type(context);
    ty->base = base;
    ty->flags = flags;
    ty->inner = inner;

    return type_id(context, ty);
}

int find_or_create_type_inner(struct context *context, int typ, struct node *decl) {
    switch (decl->type) {
    case NODE_DECLARATOR: {
        if (decl->d.inner) {
            int inner = find_or_create_type_inner(context, typ, NODE(decl->d.inner));
            return find_or_create(context, inner, TYPE_POINTER, 0);
        } else {
            return typ;
        }
    }
    case NODE_FUNCTION_DECLARATOR: {
        int inner = find_or_create_type_inner(context, typ, NODE(decl->d.inner));
        return find_or_create(context, inner, TYPE_FUNCTION, 0);
    }
    case NODE_ARRAY_DECLARATOR: {
        int inner = find_or_create_type_inner(context, typ, NODE(decl->d.inner));
        return find_or_create(context, inner, TYPE_FUNCTION, 0);
    }
    default:
        report_error(context, "invalid declarator type");
        return 0;
    }
}

int find_or_create_type(struct context *context, struct node *type, struct node *decl) {
    enum base_type b_type = -1;
    for (int i = 0; i < ARRAY_LEN(base_type_ids); i += 1) {
        if (!base_type_ids[i]) continue;
        if (strncmp(base_type_ids[i], &context->tu->source[type->token->index], type->token->len) == 0) {
            b_type = i;
        }
    }
    if (b_type == -1) {
        report_error(context, "invalid type name");
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

    while (scope) {
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

    return scope_id(context, scope);
}

bool name_exists(struct context *context, struct token *token, int scope_id) {
    struct scope *scope = SCOPE(scope_id);
    int depth = scope->block_depth;

    while (scope->block_depth == depth) {
        if (token_cmp(context, scope->token, token) == 0)
            return true;

        scope = SCOPE(scope->parent);
        if (!scope) break;
    }

    return false;
}

int type_recur(struct context *context, struct node *node, int block_depth, int parent_scope) {
    int scope = parent_scope;

    switch (node->type) {
    case NODE_DECLARATION: {
        struct node *base_type = NODE(node->decl.type);
        for (int i = 0; i < MAX_DECLARATORS && node->decl.declarators[i]; i += 1) {
            struct node *decl = NODE(node->decl.declarators[i]);
            int type_id = find_or_create_type(context, base_type, decl);
            scope = create_scope(context, scope, type_id, block_depth, decl);

            if (decl->d.initializer)
                type_recur(context, NODE(node->d.initializer), block_depth, scope);
        }
        return scope;
    }
    case NODE_ROOT:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i++) {
            int s = type_recur(context, NODE(node->root.children[i]), block_depth, scope);
            if (s) scope = s;
        }
        break;
    case NODE_BLOCK:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i++) {
            int s = type_recur(context, NODE(node->block.children[i]), block_depth + 1, scope);
            if (s) scope = s;
        }
        break;
    case NODE_FUNCTION_DEFINITION:
        // TODO: open scope for parameters
        type_recur(context, NODE(node->fun.body), block_depth + 1, scope);
        break;
    case NODE_BINARY_OP:
        type_recur(context, NODE(node->binop.left), block_depth, scope);
        type_recur(context, NODE(node->binop.right), block_depth, scope);
        break;
    case NODE_UNARY_OP:
    case NODE_POSTFIX_OP:
        type_recur(context, NODE(node->unary_op.inner), block_depth, scope);
        break;
    case NODE_SUBSCRIPT:
        type_recur(context, NODE(node->subscript.inner), block_depth, scope);
        type_recur(context, NODE(node->subscript.subscript), block_depth, scope);
        break;
    case NODE_TERNARY:
        type_recur(context, NODE(node->ternary.condition), block_depth, scope);
        type_recur(context, NODE(node->ternary.branch_true), block_depth, scope);
        type_recur(context, NODE(node->ternary.branch_false), block_depth, scope);
        break;
    case NODE_RETURN:
        type_recur(context, NODE(node->ret.expr), block_depth, scope);
        break;
    case NODE_IDENT: {
        int scope_id = resolve_name(context, node->token, scope);
        if (!scope_id) {
            report_error(context, "undefined identifier");
        }
        fprintf(stderr, "resolving %.*s (line %i) to ", node->token->len, TOKEN_STR(node->token), node->token->line);
        print_type(context, SCOPE(scope_id)->c_type);
        fprintf(stderr, " declared on line %i\n", SCOPE(scope_id)->token->line);
        node->ident.scope_id = scope_id;
        break;
    }
    default:
    }

    return 0;
}