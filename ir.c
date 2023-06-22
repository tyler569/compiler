#include "ir.h"
#include "token.h"
#include "parse.h"
#include "type.h"
#include "tu.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define SCOPE(n) list_ptr(&context->tu->scopes, n)
#define TSCOPE(n) list_ptr(&tu->scopes, n)

void print_ir_reg(struct tu *tu, struct ir_reg *reg) {
    fputc('r', stderr);
    if (reg->scope) {
        print_token(tu, reg->scope->token);
        // fputc('.', stderr);
    } else {
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
            first = false;
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

struct ir_reg *emit_node_recur(struct tu *tu, struct function *function, struct node *node, bool write);
struct function *new_function();

int emit(struct tu *tu) {
    struct function *function = new_function();

    emit_node_recur(tu, function, tu->ast_root, false);

    for_each_n (instr, &function->ir_list) {
        print_ir_instr(tu, instr);
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

struct function *new_function() {
    struct function *function = calloc(1, sizeof(struct function));
    return function;
}

struct ir_reg *new_reg(struct function *function) {
    reg *r = calloc(1, sizeof(struct ir_reg));
    r->function = function;
    return r;
}

struct ir_reg *new_temporary(struct function *function) {
    reg *r = new_reg(function);
    r->index = function->temporary_id++;
    return r;
}

static struct ir_reg *new_scope_reg(struct function *function, struct scope *scope, bool write) {
    if (write) scope->ir_index += 1;
    reg *r = new_reg(function);
    r->scope = scope;
    r->index = (short)scope->ir_index;
    return r;
}

struct ir_reg *emit_node_recur(struct tu *tu, struct function *function, struct node *node, bool write) {
#define EMIT(i) list_push(&function->ir_list, (i))

    switch (node->type) {
    case NODE_ROOT:
        for_each (&node->root.children) {
            emit_node_recur(tu, function, *it, false);
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
            reg *lhs = emit_node_recur(tu, function, node->binop.lhs, true);
            reg *rhs = emit_node_recur(tu, function, node->binop.rhs, false);
            EMIT(ir_move(lhs, rhs));
            return lhs;
        } else {
            reg *lhs = emit_node_recur(tu, function, node->binop.lhs, true);
            reg *rhs = emit_node_recur(tu, function, node->binop.rhs, false);
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
        return new_scope_reg(function, scope, false);
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
        for_each (&node->funcall.args) {
            list_push(&ins.args, emit_node_recur(tu, function, *it, false));
        }
        reg *f = emit_node_recur(tu, function, node->funcall.inner, false);
        reg *out = new_temporary(function);
        EMIT(ir_call(out, f, ins.args));
        return out;
    }
    case NODE_DECLARATION:
        for_each (&node->decl.declarators) {
            emit_node_recur(tu, function, *it, false);
        }
        return nullptr;
    case NODE_TYPE_SPECIFIER:
        break;
    case NODE_DECLARATOR:
        if (node->d.initializer) {
            struct scope *scope = TSCOPE(node->d.scope_id);
            reg *init = emit_node_recur(tu, function, node->d.initializer, false);
            reg *out = new_scope_reg(function, scope, true);
            EMIT(ir_move(out, init));
        }
        return nullptr;
    case NODE_ARRAY_DECLARATOR:
        break;
    case NODE_FUNCTION_DECLARATOR:
        break;
    case NODE_FUNCTION_DEFINITION:emit_node_recur(tu, function, node->fun.decl, false);
        emit_node_recur(tu, function, node->fun.body, false);
        return nullptr;
    case NODE_STATIC_ASSERT:
        break;
    case NODE_BLOCK:
        for_each (&node->block.children) {
            emit_node_recur(tu, function, *it, false);
        }
        return nullptr;
    case NODE_LABEL:
        break;
    case NODE_RETURN: {
        reg *ret = emit_node_recur(tu, function, node->ret.expr, false);
        EMIT(ir_ret(ret));
        return nullptr;
    }
    case NODE_IF: {
        reg *test = emit_node_recur(tu, function, node->if_.cond, false);
        const char *label_false = tprintf(tu, "if%i.false", ++function->cond_id);
        const char *label_end = tprintf(tu, "if%i.end", function->cond_id);

        EMIT(ir_jz(label_false, test));

        emit_node_recur(tu, function, node->if_.block_true, false);
        EMIT(ir_jmp(label_end));

        EMIT(ir_label(label_false));
        emit_node_recur(tu, function, node->if_.block_false, false);
        EMIT(ir_label(label_end));

        return nullptr;
    }
    case NODE_WHILE: {
        const char *label_top = tprintf(tu, "while%i.top", ++function->cond_id);
        const char *label_end = tprintf(tu, "while%i.end", function->cond_id);

        EMIT(ir_label(label_top));
        reg *test = emit_node_recur(tu, function, node->while_.cond, false);
        EMIT(ir_jz(label_end, test));

        emit_node_recur(tu, function, node->while_.block, false);
        EMIT(ir_jmp(label_top));
        EMIT(ir_label(label_end));

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
    case NODE_BREAK:
        break;
    case NODE_CONTINUE:
        break;
    case NODE_DEFAULT:
        break;
    case NODE_STRUCT:
        break;
    case NODE_ENUM:
        break;
    case NODE_UNION:
        break;
    }

    fprintf(stderr, "unhandled node: %i\n", node->type);

    return nullptr;

#undef EMIT
}