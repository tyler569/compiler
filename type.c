#include "type.h"
#include "tu.h"
#include "token.h"
#include "parse.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define SCOPE(n) &(context->sca.scopes[(n)])
#define NODE(n) &(context->root[(n)])
#define TYPE(n) &(context->tya.types[(n)])

struct scope {
    struct token *token;
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
};

static int debug_create_type(struct context *context, int parent, enum base_type base, enum type_flags flags);
static void print_type(struct context *context, int type_id);

int type(struct tu *tu) {
    struct context *context = &(struct context) {
        .tu = tu,
        .root = tu->nodes,
    };

    int t_null = debug_create_type(context, 0, TYPE_VOID, 0);
    int t_void = debug_create_type(context, 0, TYPE_VOID, 0);
    int t_const_void = debug_create_type(context, 0, TYPE_VOID, TF_CONST);
    int t_ptr_const_void = debug_create_type(context, t_const_void, TYPE_POINTER, 0);
    int t_fun_void = debug_create_type(context, t_void, TYPE_FUNCTION, 0);

    int t_int = debug_create_type(context, 0, TYPE_SIGNED_INT, 0);

    int t_alignas_int = debug_create_type(context, 0, TYPE_SIGNED_INT, 5 << TF_ALIGNAS_BIT);
    int t_const_ptr_alignas_int = debug_create_type(context, t_alignas_int, TYPE_POINTER, TF_CONST);

    int t_fun_int = debug_create_type(context, t_int, TYPE_FUNCTION, 0);
    int t_ptr_fun_int = debug_create_type(context, t_fun_int, TYPE_POINTER, 0);

    int t_ptr_int = debug_create_type(context, t_int, TYPE_POINTER, 0);
    int t_fun_ptr_int = debug_create_type(context, t_ptr_int, TYPE_FUNCTION, 0);

#define PRINT(t) do { print_type(context, (t)); fputc('\n', stderr); } while (0)
    PRINT(t_void);
    PRINT(t_const_void);
    PRINT(t_ptr_const_void);
    PRINT(t_fun_void);

    PRINT(t_alignas_int);
    PRINT(t_const_ptr_alignas_int);

    PRINT(t_int);
    PRINT(t_fun_int);
    PRINT(t_ptr_fun_int);
    PRINT(t_ptr_int);
    PRINT(t_fun_ptr_int);
#undef PRINT

    return -1;
}

static void report_error(struct context *context, const char *message) {
    fprintf(stderr, "typer error: %s\n", message);
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

int find_or_create_type(struct context *context, struct node *type, struct node *decl) {
    return 0;
}

void type_recur(struct context *context, struct node *node, int block_depth, int scope) {
    switch (node->type) {
    case NODE_DECLARATION: {
        struct node *base_type = NODE(node->decl.type);
        for (int i = 0; i < MAX_DECLARATORS && node->decl.declarators[i]; i += 1) {
            struct node *decl = NODE(node->decl.declarators[i]);
            find_or_create_type(context, base_type, decl);
        }
    }
    default:
        return;
    }
}