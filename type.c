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

#define SCOPE(n) list_ptr(&tu->scopes, n)
#define TYPE(n) list_ptr(&tu->types, n)
#define TOKEN_STR(tok) (&tu->source[(tok)->index])

int type_recur(struct tu *tu, struct node *node, int block_depth, int parent_scope);
static int debug_create_type(struct tu *tu, int parent, enum layer_type base, enum type_flags flags);

static struct type *new_type(struct tu *tu);
static struct scope *new_scope(struct tu *tu);

int type(struct tu *tu) {
    // discard index 0, so it can be used for "none"
    // moved to main for now since this has to happen during parse() now
    // (void) new_type(tu);
    // (void) new_scope(tu);

    type_recur(tu, tu->ast_root, 0, 0);

    return 0;
}

static void report_error(struct tu *tu, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_error(tu, message, args);

    va_end(args);
}

static void report_error_node(struct tu *tu, struct node *node, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vprint_error_node(tu, node, message, args);

    va_end(args);
}

static struct scope *new_scope(struct tu *tu) {
    list_push(&tu->scopes, (struct scope){});
    return &list_last(&tu->scopes);
}

static struct type *new_type(struct tu *tu) {
    list_push(&tu->types, (struct type){});
    return &list_last(&tu->types);
}

int scope_id(struct tu *tu, struct scope *scope) {
    return (int)(list_indexof(&tu->scopes, scope));
}

int type_id(struct tu *tu, struct type *type) {
    return (int)(list_indexof(&tu->types, type));
}

// This doesn't currently print in C syntax, but it's good enough to know
// what's going on.
void print_type(struct tu *tu, int type_id) {
    struct type *type = TYPE(type_id);

#define FLAG(flag_id, flag_name) if (type->flags & (flag_id)) do { fputs((flag_name), stderr); } while (0)
    FLAG(TF_ATOMIC, "atomic ");
    FLAG(TF_CONST, "const ");
    FLAG(TF_VOLATILE, "volatile ");
    FLAG(TF_RESTRICT, "restrict ");
    FLAG(TF_INLINE, "inline ");
    FLAG(TF_NORETURN, "noreturn ");
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
        print_type(tu, type->inner);
    }
}

void print_storage_class(enum storage_class class) {
    switch (class) {
#define CASE(sc, name) case sc: fprintf(stderr, "%s ", name); break
    CASE(ST_TYPEDEF, "typedef");
    CASE(ST_THREAD_LOCAL, "thead_local");
    CASE(ST_AUTOMATIC, "auto");
    CASE(ST_CONSTEXPR, "constexpr");
    CASE(ST_EXTERNAL, "extern");
    CASE(ST_REGISTER, "register");
    CASE(ST_STATIC, "static");
#undef CASE
    }
}

static int debug_create_type(struct tu *tu, int parent, enum layer_type base, enum type_flags flags) {
    struct type *type = new_type(tu);
    type->inner = parent;
    type->layer = base;
    type->flags = flags;
    return type_id(tu, type);
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

int find_or_create_type(struct tu *tu, int inner, enum layer_type base, enum type_flags flags) {
    for_each (&tu->types) {
        if (it->inner == inner && it->layer == base && it->flags == flags)
            return list_indexof(&tu->types, it);
    }

    struct type *ty = new_type(tu);
    ty->layer = base;
    ty->flags = flags;
    ty->inner = inner;

    return type_id(tu, ty);
}

int find_or_create_type_inner(struct tu *tu, int typ, struct node *decl) {
    switch (decl->type) {
    case NODE_DECLARATOR:
        if (!decl->d.inner) {
            return typ;
        } else {
            int layer = find_or_create_type(tu, typ, TYPE_POINTER, 0);
            return find_or_create_type_inner(tu, layer, decl->d.inner);
        }
    case NODE_FUNCTION_DECLARATOR: {
        int layer = find_or_create_type(tu, typ, TYPE_FUNCTION, 0);
        return find_or_create_type_inner(tu, layer, decl->d.inner);
    }
    case NODE_ARRAY_DECLARATOR: {
        int layer = find_or_create_type(tu, typ, TYPE_ARRAY, 0);
        return find_or_create_type_inner(tu, layer, decl->d.inner);
    }
    default:
        report_error_node(tu, decl, "invalid declarator decl_spec");
        return 0;
    }
}

int find_or_create_decl_type(struct tu *tu, struct node *decl, struct node *d) {
    assert(decl->decl.decl_spec_c_type);

    int decltype = find_or_create_type_inner(tu, decl->decl.decl_spec_c_type, d);
    return decltype;
}

size_t type_size(struct tu *tu, int type_id) {
    struct type *type = TYPE(type_id);

    switch (type->layer) {
    case TYPE_COMPLEX_DOUBLE:
    case TYPE_COMPLEX_LONG_DOUBLE:
        return 16;
    case TYPE_POINTER:
    case TYPE_SIGNED_LONG:
    case TYPE_UNSIGNED_LONG:
    case TYPE_SIGNED_LONG_LONG:
    case TYPE_UNSIGNED_LONG_LONG:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_COMPLEX_FLOAT:
        return 8;
    case TYPE_SIGNED_INT:
    case TYPE_UNSIGNED_INT:
    case TYPE_FLOAT:
        return 4;
    case TYPE_SIGNED_SHORT:
    case TYPE_UNSIGNED_SHORT:
        return 2;
    case TYPE_SIGNED_CHAR:
    case TYPE_UNSIGNED_CHAR:
    case TYPE_BOOL:
        return 1;
    case TYPE_ENUM:
        return type_size(tu, type->inner);
    case TYPE_VOID:
        return 0;
    case TYPE_STRUCT:
    case TYPE_UNION:
        report_error(tu, "struct and union type sizes are not implemented");
        return 0;
    case TYPE_ARRAY:
        report_error(tu, "array type sizes are not implemented");
        return 0;
    case TYPE_FUNCTION:
        report_error(tu, "function types do not have a size");
        return 0;
    case TYPE_AUTO:
        report_error(tu, "invalid! auto must be resolved before this point");
        return 0;
    }
}

size_t type_align(struct tu *tu, int type_id) {
    struct type *type = TYPE(type_id);

    switch (type->layer) {
    case TYPE_COMPLEX_DOUBLE:
    case TYPE_COMPLEX_LONG_DOUBLE:
    case TYPE_POINTER:
    case TYPE_SIGNED_LONG:
    case TYPE_UNSIGNED_LONG:
    case TYPE_SIGNED_LONG_LONG:
    case TYPE_UNSIGNED_LONG_LONG:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_COMPLEX_FLOAT:
        return 8;
    case TYPE_SIGNED_INT:
    case TYPE_UNSIGNED_INT:
    case TYPE_FLOAT:
        return 4;
    case TYPE_SIGNED_SHORT:
    case TYPE_UNSIGNED_SHORT:
        return 2;
    case TYPE_SIGNED_CHAR:
    case TYPE_UNSIGNED_CHAR:
    case TYPE_BOOL:
        return 1;
    case TYPE_ENUM:
        return type_align(tu, type->inner);
    case TYPE_VOID:
        return 0;
    case TYPE_STRUCT:
    case TYPE_UNION:
        report_error(tu, "struct and union type sizes are not implemented");
        return 0;
    case TYPE_ARRAY:
        return type_align(tu, type->inner);
        return 0;
    case TYPE_FUNCTION:
        report_error(tu, "function types do not have an alignment");
        return 0;
    case TYPE_AUTO:
        report_error(tu, "invalid! auto must be resolved before this point");
        return 0;
    }
}

int token_cmp(struct tu *tu, struct token *a, struct token *b) {
    int len = a->len > b->len ? a->len : b->len;
    return strncmp(&tu->source[a->index], &tu->source[b->index], len);
}

int resolve_name(struct tu *tu, struct token *token, int sc) {
    struct scope *scope = SCOPE(sc);

    while (scope && scope->token) {
        if (token_cmp(tu, token, scope->token) == 0) {
            return scope_id(tu, scope);
        }
        scope = SCOPE(scope->parent);
    }

    return 0;
}

int create_scope(struct tu *tu, int parent, int c_type, int depth, enum storage_class sc, struct node *d, struct node *function) {
    struct scope *scope = new_scope(tu);
    struct scope *parent_scope = SCOPE(parent);

    assert(d->d.name);

    scope->token = d->d.name;
    scope->decl = d;
    scope->parent = parent;
    scope->c_type = c_type;
    scope->block_depth = depth;
    scope->sc = sc;

    fprintf(stderr, "%.*s has type ", scope->token->len, TOKEN_STR(scope->token));
    print_storage_class(sc);
    print_type(tu, c_type);
    fprintf(stderr, "\n");

    return scope_id(tu, scope);
}

struct scope *name_exists(struct tu *tu, struct token *token, int scope_id, int depth) {
    struct scope *scope = SCOPE(scope_id);

    while (scope->block_depth == depth && scope->token) {
        if (token_cmp(tu, scope->token, token) == 0)
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
int type_recur(struct tu *tu, struct node *node, int block_depth, int parent_scope) {
    int scope = parent_scope;

    switch (node->type) {
    case NODE_DECLARATION: {
        // struct node *base_type = node->decl.decl_spec;
        for_each (&node->decl.declarators) {
            struct node *d = *it;
            struct scope *before;
            if ((before = name_exists(tu, d->d.name, scope, block_depth))) {
                report_error_node(tu, d, "redefinition of name");
                print_info_node(tu, before->decl, "previous definition is here");
            }
            int type_id = find_or_create_decl_type(tu, node, d);
            scope = create_scope(tu, scope, type_id, block_depth, node->decl.sc, d, nullptr);

            d->d.scope_id = scope;

            if (d->d.initializer) {
                type_recur(tu, d->d.initializer, block_depth, scope);
            }
        }
        return scope;
    }
    case NODE_ROOT:
        for_each (&node->root.children) {
            int s = type_recur(tu, *it, block_depth, scope);
            if (s) scope = s;
        }
        return 0;
    case NODE_BLOCK:
        for_each (&node->block.children) {
            int s = type_recur(tu, *it, block_depth + 1, scope);
            if (s) scope = s;
        }
        return 0;
    case NODE_FUNCTION_DEFINITION: {
        int new_outer = type_recur(tu, node->fun.decl, block_depth, scope);
        struct node *n = node->fun.decl;
        struct node *d = n->decl.declarators.data[0];
        for_each (&d->d.fun.args) {
            int s = type_recur(tu, *it, block_depth + 1, scope);
            if (s) scope = s;
        }
        // function body is a compound statement - that increments block_depth on its own, so
        // this drops back to outer scope to avoid the body of the function being deeper than
        // arguments.
        type_recur(tu, node->fun.body, block_depth, scope);

        return new_outer;
    }
    case NODE_BINARY_OP:
        type_recur(tu, node->binop.lhs, block_depth, scope);
        type_recur(tu, node->binop.rhs, block_depth, scope);
        return 0;
    case NODE_UNARY_OP:
    case NODE_POSTFIX_OP:
        type_recur(tu, node->unary_op.inner, block_depth, scope);
        return 0;
    case NODE_SUBSCRIPT:
        type_recur(tu, node->subscript.inner, block_depth, scope);
        type_recur(tu, node->subscript.subscript, block_depth, scope);
        return 0;
    case NODE_TERNARY:
        type_recur(tu, node->ternary.condition, block_depth, scope);
        type_recur(tu, node->ternary.branch_true, block_depth, scope);
        type_recur(tu, node->ternary.branch_false, block_depth, scope);
        return 0;
    case NODE_RETURN:
        type_recur(tu, node->ret.expr, block_depth, scope);
        return 0;
    case NODE_IDENT: {
        int scope_id = resolve_name(tu, node->token, scope);
        if (!scope_id) {
            report_error_node(tu, node, "undeclared identifier");
            exit(1);
        }
        fprintf(stderr, "resolving %.*s (line %i) to ", node->token->len, TOKEN_STR(node->token), node->token->line);
        print_type(tu, SCOPE(scope_id)->c_type);
        fprintf(stderr, " declared on line %i\n", SCOPE(scope_id)->token->line);
        fprintf(stderr, "block depth is %i\n", block_depth);
        node->ident.scope_id = scope_id;
        return 0;
    }
    case NODE_IF: {
        type_recur(tu, node->if_.cond, block_depth, scope);
        type_recur(tu, node->if_.block_true, block_depth + 1, scope);
        if (node->if_.block_false)
            type_recur(tu, node->if_.block_false, block_depth + 1, scope);
        return 0;
    }
    case NODE_WHILE: {
        type_recur(tu, node->while_.cond, block_depth, scope);
        type_recur(tu, node->while_.block, block_depth + 1, scope);
        return 0;
    }
    default:
        return 0;
    }
}