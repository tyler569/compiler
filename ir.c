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

void print_token(struct tu *tu, struct token *token) {
    fprintf(stderr, "%.*s", token->len, &tu->source[token->index]);
}

void print_ir_reg_name(struct ir_reg *reg) {
    fprintf(stderr, "r%i", reg->index);
}

void print_ir_reg(struct tu *tu, struct ir_reg *reg) {
    if (reg->is_phi) {
        fprintf(stderr, "phi (");
        bool first = true;
        for_each (&reg->phi_list) {
            if (!first) fprintf(stderr, ", ");
            first = false;
            print_ir_reg_name(*it);
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
    print_ir_reg(tu, i->r[r]);
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

    case CALL:
        print_reg(tu, i, 0);
        fprintf(stderr, " := call ");
        print_reg(tu, i, 1);
        fprintf(stderr, ", ");
        bool first = true;
        for_each (&i->args) {
            if (!first) fprintf(stderr, ", ");
            print_ir_reg(tu, *it);
        }
        break;

    case PHI:
        fprintf(stderr, "PHI instruction shouldn't be a thing");
        break;

    default:
        fprintf(stderr, "no print for ir %i\n", i->op);

#undef CASE3
    }
}

void print_link(struct bb *from, struct bb *to) {
    if (from->name)
        fprintf(stderr, "    \"%s\" -> ", from->name);
    else
        fprintf(stderr, "    \"%p\" -> ", from);
    if (to->name)
        fprintf(stderr, "\"%s\";\n", to->name);
    else
        fprintf(stderr, "\"%p\";\n", to);
}

void print_control_flow(struct tu *tu, struct function *function) {
    fprintf(stderr, "digraph {\n");
    for_each (&function->bbs) {
        for_each_n (bb, &(*it)->inputs) {
            print_link(*bb, *it);
        }
        for_each_n (bb, &(*it)->outputs) {
            print_link(*it, *bb);
        }
    }
    fprintf(stderr, "}\n");
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

struct bb *active_bb(struct function *function) {
    return list_last(&function->bbs);
}

struct ir_reg *bb_emit_node(struct tu *tu, struct function *function, struct node *node, bool write);
struct function *new_function();

int emit(struct tu *tu) {
    struct function *function = new_function();

    bb_emit_node(tu, function, tu->nodes, false);

    for_each (&function->bbs) {
        fprintf(stderr, "\nnew bb:\n");

        for_each_n (instr, &(*it)->ir_list) {
            print_ir_instr(tu, instr);
        }
    }

    // fprintf(stderr, "\n");
    // print_control_flow(tu, function);

    return 0;
}

struct ir_instr ir_move(struct ir_reg *out, struct ir_reg *in) {
    ir i = {
        .op = MOVE,
        .r = { out, in },
    };
    return i;
}

struct ir_instr ir_binop(enum ir_op op, struct ir_reg *out, struct ir_reg *in1, struct ir_reg *in2) {
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

struct ir_instr ir_unop(enum ir_op op, struct ir_reg *out, struct ir_reg *in) {
    ir i = {
        .op = op,
        .r = { out, in },
    };
    return i;
}

struct ir_instr ir_ret(struct ir_reg *v) {
    ir i = {
        .op = RET,
        .r = { v },
    };
    return i;
}

struct ir_instr ir_jz(const char *label, struct ir_reg *cond) {
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

struct ir_instr ir_imm(uint64_t immediate, struct ir_reg *out) {
    ir i = {
        .op = IMM,
        .immediate_i = immediate,
        .r = { out },
    };
    return i;
}

struct ir_instr ir_call(struct ir_reg *out, struct ir_reg *func, reg_list_t args) {
    ir i = {
        .op = CALL,
        .r = { out, func },
        .args = args,
    };
    return i;
}

struct bb *new_bb(struct function *func, struct bb *prev) {
    struct bb *bb = calloc(1, sizeof(struct bb));
    bb->function = func;
    list_push(&func->bbs, bb);
    if (prev) list_push(&bb->inputs, prev);

    return bb;
}

struct function *new_function() {
    struct function *function = calloc(1, sizeof(struct function));
    new_bb(function, nullptr);
    return function;
}

struct ir_reg *new_reg(struct bb *bb) {
    reg *r = calloc(1, sizeof(struct ir_reg));
    r->bb = bb;
    return r;
}

struct ir_reg *new_phi(struct bb *bb, struct scope *scope) {
    reg *r = new_reg(bb);
    r->is_phi = true;
    r->scope = scope;
    r->index = 0;
    return r;
}

struct ir_reg *new_temporary(struct function *function) {
    reg *r = new_reg(active_bb(function));
    r->index = function->temporary_id++;
    return r;
}

static struct ir_reg *new_scope_reg(struct function *function, struct scope *scope, bool write) {
    if (write) scope->ir_index += 1;
    reg *r = new_reg(active_bb(function));
    r->scope = scope;
    r->index = (short)scope->ir_index;
    return r;
}

static struct ir_reg *new_scope_reg_bb(struct bb *bb, struct scope *scope, bool write) {
    if (write) scope->ir_index += 1;
    reg *r = new_reg(bb);
    r->scope = scope;
    r->index = (short)scope->ir_index;
    return r;
}

void bb_own(struct bb *bb, struct ir_reg *reg);

static struct ir_reg *lookup_ident_decl(struct tu *tu, struct function *function, struct scope *scope) {
    struct bb *bb = active_bb(function);
    reg *r = new_scope_reg(function, scope, true);
    bb_own(bb, r);
    return r;
}

struct ir_reg *bb_owns(struct bb *bb, struct scope *scope) {
    if (bb->owned_vars.len == 0) return nullptr;

    for_each (&bb->owned_vars) {
        if ((*it)->scope == scope) {
            return *it;
        }
    }
    return nullptr;
}

void bb_own(struct bb *bb, struct ir_reg *reg) {
    if (!reg->scope) return;

    for_each (&bb->owned_vars) {
        if ((*it)->scope == reg->scope) {
            *it = reg;
            return;
        }
    }

    list_push(&bb->owned_vars, reg);
}

struct ir_reg *try_remove_trivial_phi(struct ir_reg *phi) {
     if (phi->phi_list.len == 0) {
         reg *r = new_scope_reg_bb(phi->bb, phi->scope, false);
         bb_own(phi->bb, r);
         return r;
     }

     if (phi->phi_list.len == 1) {
         reg *r = phi->phi_list.data[0];
         return r;
     }

     for_each (&phi->phi_dependants) {
         try_remove_trivial_phi(*it);
     }

    // It's not trivial
    return phi;
}


struct ir_reg *lookup_ident_2_global(struct tu *tu, struct bb *bb, struct scope *scope);

struct ir_reg *lookup_ident_2_local(struct tu *tu, struct bb *bb, struct scope *scope, bool write) {
    if (write) {
        return new_scope_reg_bb(bb, scope, true);
    }

    reg *r;
    if ((r = bb_owns(bb, scope))) {
        return r;
    }

    return lookup_ident_2_global(tu, bb, scope);
}

void add_phi_operands(struct tu *tu, struct bb *bb, struct ir_reg *phi) {
    for_each (&bb->inputs) {
        reg *r = lookup_ident_2_local(tu, *it, phi->scope, false);

        list_push(&phi->phi_list, r);
    }
}

struct ir_reg *lookup_ident_2_global(struct tu *tu, struct bb *bb, struct scope *scope) {
    if (!bb->sealed) {
        reg *phi = new_phi(bb, scope);
        list_push(&bb->incomplete_phis, phi);
        return phi;
    }

    if (bb->inputs.len == 1) {
        return lookup_ident_2_local(tu, bb->inputs.data[0], scope, false);
    }

    reg *phi = new_phi(bb, scope);
    list_push(&phi->phi_list, phi);
    add_phi_operands(tu, bb, phi);

    reg *tmp = new_temporary(bb->function);
    tmp->scope = scope;
    bb_own(bb, tmp);

    ir i = ir_move(tmp, phi);
    list_push(&bb->ir_list, i);

    return tmp;
}

void bb_seal(struct tu *tu, struct bb *bb) {
    bb->sealed = true;

    if (bb->incomplete_phis.len == 0) return;

    for_each_n (phi, &bb->incomplete_phis) {
        for_each_n (ibb, &bb->inputs) {
            reg *input = lookup_ident_2_local(tu, *ibb, (*phi)->scope, false);
            if (input) list_push(&(*phi)->phi_list, input);
        }
    }

    list_clear(&bb->incomplete_phis);
}

void bb_fill(struct tu *tu, struct bb *bb) {
    bb->filled = true;
}

struct ir_reg *bb_emit_node(struct tu *tu, struct function *function, struct node *node, bool write) {
#define ACTIVE_BB list_last(&function->bbs)
#define EMIT(i) list_push(&ACTIVE_BB->ir_list, (i))

    switch (node->type) {
    case NODE_ROOT:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i += 1) {
            bb_emit_node(tu, function, node->root.children[i], false);
        }
        return nullptr;
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
            return nullptr;
        }
        if (op == MOVE) {
            reg *lhs = bb_emit_node(tu, function, node->binop.lhs, true);
            reg *rhs = bb_emit_node(tu, function, node->binop.rhs, false);
            EMIT(ir_move(lhs, rhs));
            return lhs;
        } else {
            reg *lhs = bb_emit_node(tu, function, node->binop.lhs, true);
            reg *rhs = bb_emit_node(tu, function, node->binop.rhs, false);
            reg *res = new_temporary(function);
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
        return lookup_ident_2_local(tu, ACTIVE_BB, scope, write);
    }
    case NODE_INT_LITERAL: {
        reg *res = new_temporary(function);
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
    case NODE_FUNCTION_CALL: {
        struct ir_instr ins = {};
        for (int i = 0; i < MAX_FUNCTION_ARGS && node->funcall.args[i]; i += 1) {
            list_push(&ins.args, bb_emit_node(tu, function, node->funcall.args[i], false));
        }
        reg *f = bb_emit_node(tu, function, node->funcall.inner, false);
        reg *out = new_temporary(function);
        EMIT(ir_call(out, f, ins.args));
        return out;
    }
    case NODE_DECLARATION:
        for (int i = 0; i < MAX_DECLARATORS && node->decl.declarators[i]; i += 1) {
            bb_emit_node(tu, function, node->decl.declarators[i], false);
        }
        return nullptr;
    case NODE_TYPE_SPECIFIER:
        break;
    case NODE_DECLARATOR:
        if (node->d.initializer) {
            struct scope *scope = TSCOPE(node->d.scope_id);
            reg *init = bb_emit_node(tu, function, node->d.initializer, false);
            reg *out = lookup_ident_decl(tu, function, scope);
            EMIT(ir_move(out, init));
        }
        return nullptr;
    case NODE_ARRAY_DECLARATOR:
        break;
    case NODE_FUNCTION_DECLARATOR:
        break;
    case NODE_FUNCTION_DEFINITION:
        bb_emit_node(tu, function, node->fun.decl, false);
        bb_emit_node(tu, function, node->fun.body, false);
        return nullptr;
    case NODE_STATIC_ASSERT:
        break;
    case NODE_BLOCK:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i += 1) {
            bb_emit_node(tu, function, node->block.children[i], false);
        }
        return nullptr;
    case NODE_LABEL:
        break;
    case NODE_RETURN: {
        reg *ret = bb_emit_node(tu, function, node->ret.expr, false);
        EMIT(ir_ret(ret));
        return nullptr;
    }
    case NODE_IF: {
        reg *test = bb_emit_node(tu, function, node->if_.cond, false);
        const char *label_false = tprintf(tu, "if%i.false", ++function->cond_id);
        const char *label_end = tprintf(tu, "if%i.end", function->cond_id);

        EMIT(ir_jz(label_false, test));

        struct bb *bb_before_if = ACTIVE_BB;
        bb_fill(tu, ACTIVE_BB);

        struct bb *bb_true = new_bb(function, bb_before_if);
        bb_seal(tu, bb_true);

        bb_true->name = "if.true";
        bb_emit_node(tu, function, node->if_.block_true, false);
        EMIT(ir_jmp(label_end));
        bb_fill(tu, ACTIVE_BB);

        struct bb *bb_false = new_bb(function, bb_before_if);
        bb_seal(tu, bb_false);

        bb_false->name = "if.false";
        EMIT(ir_label(label_false));
        bb_emit_node(tu, function, node->if_.block_false, false);
        bb_fill(tu, ACTIVE_BB);

        struct bb *bb_end = new_bb(function, bb_true);
        bb_end->name = "if.end";
        list_push(&bb_end->inputs, bb_false);
        bb_seal(tu, bb_end);

        EMIT(ir_label(label_end));

        return nullptr;
    }
    case NODE_WHILE: {
        const char *label_top = tprintf(tu, "while%i.top", ++function->cond_id);
        const char *label_end = tprintf(tu, "while%i.end", function->cond_id);

        struct bb *bb_before_while = ACTIVE_BB;
        bb_fill(tu, ACTIVE_BB);

        struct bb *bb_test = new_bb(function, bb_before_while);
        bb_test->name = "while.test";
        EMIT(ir_label(label_top));
        reg *test = bb_emit_node(tu, function, node->while_.cond, false);
        EMIT(ir_jz(label_end, test));
        bb_fill(tu, bb_test);

        struct bb *bb_body = new_bb(function, bb_test);
        bb_seal(tu, bb_body);
        bb_body->name = "while.body";
        bb_emit_node(tu, function, node->while_.block, false);
        EMIT(ir_jmp(label_top));
        bb_fill(tu, bb_body);

        struct bb *bb_body_end = ACTIVE_BB;

        struct bb *bb_end = new_bb(function, bb_test);
        bb_seal(tu, bb_end);
        bb_end->name = "while.end";
        EMIT(ir_label(label_end));

        list_push(&bb_test->inputs, bb_body_end);
        bb_seal(tu, bb_test);

        return nullptr;
    }
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

    return nullptr;

#undef ACTIVE_BB
}