#include "ir.h"
#include "token.h"
#include "parse.h"
#include "type.h"
#include "tu.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define SCOPE(n) (&context->tu->scopes[(n)])
#define TSCOPE(n) (&tu->scopes[(n)])

struct context {
    struct tu *tu;
    int blocks[MAX_BLOCK_MEMBERS];
    short next_reg;
    short next_condition;
    int errors;

    struct {
        size_t len;
        size_t capacity;
        struct ir_instr *instrs;
    } ia;
};

void print_token(struct tu *tu, struct token *token) {
    fprintf(stderr, "%.*s", token->len, &tu->source[token->index]);
}

void print_ir_reg(struct tu *tu, struct ir_reg *reg) {
    if (reg->is_phi) {
        fprintf(stderr, "phi (");
        bool first = true;
        for_each (&reg->phi_list) {
            if (!first) fprintf(stderr, ", ");
            first = false;
            print_ir_reg(tu, it);
        }
        fprintf(stderr, ")");
    } else {
        fputc('r', stderr);
        if (reg->scope) {
            print_token(tu, reg->scope->token);
            fputc('.', stderr);
        }
        fprintf(stderr, "%i", reg->index);
    }
}

void print_reg(struct tu *tu, struct ir_instr *i, int r) {
    print_ir_reg(tu, &i->r[r]);
}

void print_ir_instr(struct tu *tu, struct ir_instr *i) {
    switch (i->op) {
#define CASE3(instr, name) case (instr): \
    print_reg(tu, i, 0); \
    fprintf(stderr, " := " name " "); \
    print_reg(tu, i, 1); \
    fputs(", ", stderr); \
    print_reg(tu, i, 2); \
    fputc('\n', stderr); \
    break

#define CASE2(instr, name) case (instr): \
    print_reg(tu, i, 0); \
    fprintf(stderr, " := " name " "); \
    print_reg(tu, i, 1); \
    fputc('\n', stderr); \
    break

    CASE3(ADD, "add");
    CASE3(SUB, "sub");
    CASE3(MUL, "mul");
    CASE3(DIV, "div");
    CASE3(MOD, "mod");
    CASE3(AND, "and");
    CASE3(OR, "or");
    CASE3(XOR, "xor");
    CASE3(SHR, "shr");
    CASE3(SHL, "shl");
    CASE3(TEST, "test");

    CASE2(NEG, "neg");
    CASE2(INV, "inv");
    CASE2(NOT, "not");

    case MOVE:
        print_reg(tu, i, 0);
        fprintf(stderr, " := ");
        print_reg(tu, i, 1);
        fputc('\n', stderr);
        break;

    case IMM:
        print_reg(tu, i, 0);
        fprintf(stderr, " := %llu\n", i->immediate_i);
        break;

    case RET:
        fprintf(stderr, "ret ");
        print_reg(tu, i, 0);
        fputc('\n', stderr);
        break;

    case LABEL:
        fprintf(stderr, "label: %s:\n", i->name);
        break;

    case JMP:
        fprintf(stderr, "jmp %s\n", i->name);
        break;

    case JZ:
        fprintf(stderr, "jz %s, ", i->name);
        print_reg(tu, i, 0);
        fprintf(stderr, "\n");
        break;

    case PHI:
        fprintf(stderr, "PHI instruction shouldn't be a thing");
        break;

    default:
        fprintf(stderr, "no print for ir %i\n", i->op);

#undef CASE3
    }
}

void report_error(struct context *context, const char *message) {
    fprintf(stderr, "ir error: %s\n", message);
    context->errors += 1;
    exit(1);
}

// TODO: this should use the tu argument to print to tu->strtab instead of allocating
// for itself.
const char *tprintf(struct tu *tu, const char *format, ...) {
    char *out;
    va_list args;
    va_start(args, format);
    vasprintf(&out, format, args);
    va_end(args);
    return out;
}

struct ir_instr *new(struct context *context) {
    if (context->ia.capacity <= context->ia.len) {
        size_t new_capacity = context->ia.capacity ? context->ia.capacity * 2 : 512;
        struct ir_instr *new_ia = realloc(context->ia.instrs, new_capacity * sizeof(struct ir_instr));
        if (!new_ia) {
            report_error(context, "memory allocation failed");
            return nullptr;
        }
        context->ia.instrs = new_ia;
        context->ia.capacity = new_capacity;
    }

    struct ir_instr *ir_instr = &context->ia.instrs[context->ia.len++];
    return ir_instr;
}

struct ir_reg next(struct context *context) {
    return (struct ir_reg){.index = context->next_reg++};
}

struct ir_reg bb_emit_node(struct tu *tu, struct function *function, struct node *node, bool write);
struct function *new_function();

int emit(struct tu *tu) {
    struct function *function = new_function();

    bb_emit_node(tu, function, tu->nodes, false);

    for_each_v (&function->bbs) {
        fprintf(stderr, "\nnew bb:\n");

        for_each_n (instr, &it->ir_list) {
            print_ir_instr(tu, instr);
        }
    }

    return 0;
}

struct ir_instr ir_move(struct ir_reg out, struct ir_reg in) {
    ir i = {
        .op = MOVE,
        .r = { out, in },
    };
    return i;
}

struct ir_instr ir_binop(enum ir_op op, struct ir_reg out, struct ir_reg in1, struct ir_reg in2) {
    ir i = {
        .op = op,
        .r = { out, in1, in2 },
    };
    return i;
}

struct ir_instr ir_label(const char *name) {
    ir i = {
        .op = LABEL,
        .name = name,
    };
    return i;
}

struct ir_instr ir_unop(enum ir_op op, struct ir_reg out, struct ir_reg in) {
    ir i = {
        .op = op,
        .r = { out, in },
    };
    return i;
}

struct ir_instr ir_ret(struct ir_reg v) {
    ir i = {
        .op = RET,
        .r = { v },
    };
    return i;
}

struct ir_instr ir_jz(const char *label, struct ir_reg cond) {
    ir i = {
        .op = JZ,
        .name = label,
        .r = { cond },
    };
    return i;
}

struct ir_instr ir_jmp(const char *label) {
    ir i = {
        .op = JMP,
        .name = label,
    };
    return i;
}

struct ir_instr ir_imm(uint64_t immediate, struct ir_reg out) {
    ir i = {
        .op = IMM,
        .immediate_i = immediate,
        .r = { out },
    };
    return i;
}

struct bb *new_bb(struct function *func, struct bb *prev) {
    struct bb *bb = calloc(1, sizeof(struct bb));
    list_push(&func->bbs, bb);
    if (prev) list_push(&bb->inputs, prev);

    return bb;
}

struct function *new_function() {
    struct function *function = calloc(1, sizeof(struct function));
    new_bb(function, nullptr);
    return function;
}

struct ir_reg new_temporary(struct function *function) {
    return (reg){ .index = function->temporary_id++ };
}

static struct ir_reg new_scope_reg(struct scope *scope, bool write) {
    if (write) scope->ir_index += 1;

    return (reg){ .scope = scope, .index = (short)scope->ir_index };
}

static struct ir_reg lookup_ident_bb(struct tu *tu, struct function *function, struct scope *scope, bool write, struct bb *bb);

static struct ir_reg lookup_ident(struct tu *tu, struct function *function, struct scope *scope, bool write) {
    struct bb *active_bb = list_last(&function->bbs);
    return lookup_ident_bb(tu, function, scope, write, active_bb);
}

static struct ir_reg lookup_ident_decl(struct tu *tu, struct function *function, struct scope *scope) {
    struct bb *bb = list_last(&function->bbs);
    reg r = new_scope_reg(scope, true);
    list_push(&bb->owned_registers, r);
    return r;
}

struct ir_reg *bb_owns(struct bb *bb, struct scope *scope) {
    for_each (&bb->owned_registers) {
        if (it->scope == scope) {
            return it;
        }
    }
    return nullptr;
}

void bb_own(struct bb *bb, struct ir_reg reg) {
    list_push(&bb->owned_registers, reg);
}

void lookup_ident_pre(struct tu *tu, struct function *function, struct bb *bb, struct scope *scope, struct ir_reg *pre_phi);

static struct ir_reg lookup_ident_bb(struct tu *tu, struct function *function, struct scope *scope, bool write, struct bb *bb) {
    if (bb_owns(bb, scope))
        return new_scope_reg(scope, write);

    if (bb->inputs.len == 1) {
        reg r = lookup_ident_bb(tu, function, scope, write, bb->inputs.data[0]);
        if (write) bb_own(bb, r);
        return r;
    }

    if (bb->inputs.len == 0 && !bb->filled) {
        reg r = new_scope_reg(scope, write);
        bb_own(bb, r);
        return r;
    }

    reg pre_phi = (reg){
        .is_phi = true,
        .scope = scope,
    };

    bb_own(bb, pre_phi);

    for_each_v (&bb->inputs) {
        lookup_ident_pre(tu, function, it, scope, &pre_phi);
    }

    if (pre_phi.phi_list.len == 0) {
        reg r = new_scope_reg(scope, write);
        bb_own(bb, r);
        return r;
    }

    if (pre_phi.phi_list.len == 1) {
        reg r = pre_phi.phi_list.data[0];
        return r;
    }

    reg r = new_temporary(function);
    ir i = {
        .op = MOVE,
        .r = { r, pre_phi },
    };

    list_push(&bb->ir_list, i);
    return r;
}

void lookup_ident_pre(struct tu *tu, struct function *function, struct bb *bb, struct scope *scope, struct ir_reg *pre_phi) {
    struct ir_reg *p_reg;
    if ((p_reg = bb_owns(bb, scope))) {
        list_push(&pre_phi->phi_list, *p_reg);
        return;
    }

    for_each (&bb->inputs) {
        lookup_ident_pre(tu, function, bb, scope, pre_phi);
    }
}

struct ir_reg bb_emit_node(struct tu *tu, struct function *function, struct node *node, bool write) {
#define ACTIVE_BB list_last(&function->bbs)
#define EMIT(i) list_push(&ACTIVE_BB->ir_list, (i))

    switch (node->type) {
    case NODE_ROOT:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i += 1) {
            bb_emit_node(tu, function, node->root.children[i], false);
        }
        return (reg){};
    case NODE_BINARY_OP: {
        enum ir_op op;
        switch (node->token->type) {
#define CASE(token, p) case (token): op = (p); break
        CASE('+', ADD);
        CASE('-', SUB);
        CASE('*', MUL);
        CASE('/', DIV);
        CASE('&', AND);
        CASE('|', OR);
        CASE('^', XOR);
        CASE(TOKEN_EQUAL_EQUAL, TEST);
        CASE(TOKEN_SHIFT_RIGHT, SHR);
        CASE(TOKEN_SHIFT_LEFT, SHL);
        CASE('=', MOVE);
#undef CASE
        default:
            fprintf(stderr, "unhandled binary operation: %i\n", node->token->type);
            return (reg){};
        }
        if (op == MOVE) {
            reg lhs = bb_emit_node(tu, function, node->binop.lhs, true);
            reg rhs = bb_emit_node(tu, function, node->binop.rhs, false);
            EMIT(ir_move(lhs, rhs));
            return lhs;
        } else {
            reg lhs = bb_emit_node(tu, function, node->binop.lhs, true);
            reg rhs = bb_emit_node(tu, function, node->binop.rhs, false);
            reg res = new_temporary(function);
            EMIT(ir_binop(op, res, lhs, rhs));
            return res;
        }
    }
    case NODE_UNARY_OP:
        break;
    case NODE_POSTFIX_OP:
        break;
    case NODE_IDENT: {
        struct scope *scope = TSCOPE(node->ident.scope_id);
        return lookup_ident(tu, function, scope, write);
    }
    case NODE_INT_LITERAL: {
        reg res = new_temporary(function);
        EMIT(ir_imm(node->token->int_.value, res));
        return res;
    }
    case NODE_FLOAT_LITERAL:
        break;
    case NODE_STRING_LITERAL:
        break;
    case NODE_ERROR:
        break;
    case NODE_MEMBER:
        break;
    case NODE_SUBSCRIPT:
        break;
    case NODE_TERNARY:
        break;
    case NODE_FUNCTION_CALL:
        break;
    case NODE_DECLARATION:
        for (int i = 0; i < MAX_DECLARATORS && node->decl.declarators[i]; i += 1) {
            bb_emit_node(tu, function, node->decl.declarators[i], false);
        }
        return (reg){};
    case NODE_TYPE_SPECIFIER:
        break;
    case NODE_DECLARATOR:
        if (node->d.initializer) {
            struct scope *scope = TSCOPE(node->d.scope_id);
            reg init = bb_emit_node(tu, function, node->d.initializer, false);
            reg out = lookup_ident_decl(tu, function, scope);
            EMIT(ir_move(out, init));
        }
        return (reg){};
    case NODE_ARRAY_DECLARATOR:
        break;
    case NODE_FUNCTION_DECLARATOR:
        break;
    case NODE_FUNCTION_DEFINITION:
        bb_emit_node(tu, function, node->fun.decl, false);
        bb_emit_node(tu, function, node->fun.body, false);
        return (reg){};
    case NODE_STATIC_ASSERT:
        break;
    case NODE_BLOCK:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i += 1) {
            bb_emit_node(tu, function, node->block.children[i], false);
        }
        return (reg){};
    case NODE_LABEL:
        break;
    case NODE_RETURN: {
        reg ret = bb_emit_node(tu, function, node->ret.expr, false);
        EMIT(ir_ret(ret));
        return (reg){};
    }
    case NODE_IF: {
        reg test = bb_emit_node(tu, function, node->if_.cond, false);
        const char *label_false = tprintf(tu, "if%i.false", ++function->cond_id);
        const char *label_end = tprintf(tu, "if%i.end", function->cond_id);

        EMIT(ir_jz(label_false, test));

        struct bb *bb_this = ACTIVE_BB;

        struct bb *bb_true = new_bb(function, bb_this);
        bb_emit_node(tu, function, node->if_.block_true, false);
        EMIT(ir_jmp(label_end));

        struct bb *bb_false = new_bb(function, bb_this);
        EMIT(ir_label(label_false));
        bb_emit_node(tu, function, node->if_.block_false, false);

        struct bb *bb_end = new_bb(function, bb_true);
        list_push(&bb_end->inputs, bb_false);

        EMIT(ir_label(label_end));

        return (reg){};
    }
    case NODE_WHILE:
        break;
    case NODE_DO:
        break;
    case NODE_FOR:
        break;
    case NODE_GOTO:
        break;
    case NODE_SWITCH:
        break;
    case NODE_CASE:
        break;
    case NODE_NULL:
        break;
    }

    fprintf(stderr, "unhandled node: %i\n", node->type);

    return (reg){};

#undef ACTIVE_BB
}