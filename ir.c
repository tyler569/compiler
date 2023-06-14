#include "ir.h"
#include "token.h"
#include "parse.h"
#include "type.h"
#include "tu.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define NODE(n) (n)
#define SCOPE(n) (&context->tu->scopes[(n)])

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

void print_reg(struct tu *tu, struct ir_instr *i, int r) {
    fputc('r', stderr);
    if (i->r[r].name) {
        print_token(tu, i->r[r].name);
        fputc('.', stderr);
    }
    fprintf(stderr, "%i", i->r[r].index);
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
        fprintf(stderr, "label: %s:\n", i->r[0].iname);
        break;

    case JMP:
        fprintf(stderr, "jmp %s\n", i->r[0].iname);
        break;

    case JZ:
        fprintf(stderr, "jz %s, ", i->r[0].iname);
        print_reg(tu, i, 1);
        fprintf(stderr, "\n");
        break;

    case PHI:
        print_reg(tu, i, 0);
        fprintf(stderr, " := phi(");
        print_reg(tu, i, 1);
        fprintf(stderr, ", ");
        print_reg(tu, i, 2);
        fprintf(stderr, ")\n");
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

const char *tprintf(struct context *context, const char *format, ...) {
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

struct ir_reg emit_one_node(struct context *context, struct node *node, bool write);

int emit(struct tu *tu) {
    struct context *context = &(struct context){
        .tu = tu,
    };

    emit_one_node(context, tu->nodes, false);

    tu->ir = context->ia.instrs;
    tu->ir_len = context->ia.len;

    return context->errors;
}

struct ir_reg emit_one_node(struct context *context, struct node *node, bool write) {
    switch (node->type) {
    case NODE_INT_LITERAL: {
        ir *i = new(context);
        i->r[0] = next(context);
        i->op = IMM;
        i->immediate_i = node->token->int_.value;
        return i->r[0];
    }
    case NODE_FLOAT_LITERAL: {
        ir *i = new(context);
        i->r[0] = next(context);
        i->op = IMM;
        i->immediate_f = node->token->float_.value;
        return i->r[0];
    }
    case NODE_IDENT: {
        if (write) SCOPE(node->ident.scope_id)->ir_index += 1;
        struct ir_reg reg = {
            .name = node->token,
            .index = SCOPE(node->ident.scope_id)->ir_index,
        };
        return reg;
    }
    case NODE_BINARY_OP: {
        if (node->token->type == '=') {
            struct ir_reg in = emit_one_node(context, NODE(node->binop.rhs), false);
            struct ir_reg out = emit_one_node(context, NODE(node->binop.lhs), true);

            ir *i = new(context);
            i->op = MOVE;
            i->r[0] = out;
            i->r[1] = in;
            return i->r[0];
        }
        struct ir_reg lhs = emit_one_node(context, NODE(node->binop.lhs), false);
        struct ir_reg rhs = emit_one_node(context, NODE(node->binop.rhs), false);

        ir *i = new(context);
        i->r[0] = next(context);
        switch (node->token->type) {
        case '+': i->op = ADD; break;
        case '-': i->op = SUB; break;
        case '*': i->op = MUL; break;
        case '/': i->op = DIV; break;
        case '%': i->op = MOD; break;
        case TOKEN_SHIFT_RIGHT: i->op = SHR; break;
        case TOKEN_SHIFT_LEFT: i->op = SHL; break;
        case '&': i->op = AND; break;
        case '|': i->op = OR; break;
        case '^': i->op = XOR; break;
        case TOKEN_EQUAL_EQUAL: i->op = TEST; break;
        default:
            report_error(context, "unhandled binary operation");
        }
        i->r[1] = lhs;
        i->r[2] = rhs;
        return i->r[0];
    }
    case NODE_UNARY_OP: {
        struct ir_reg inner = emit_one_node(context, NODE(node->unary_op.inner), false);
        if (node->token->type == '+') return inner;

        ir *i = new(context);
        i->r[0] = next(context);
        i->r[1] = inner;
        switch (node->token->type) {
        case '-': i->op = NEG; break;
        case '~': i->op = INV; break;
        case '!': i->op = NOT; break;
        case '*':
        case '&':
            report_error(context, "pointers not yet supported");
            return i->r[0];
        default:
            report_error(context, "unknown unary operator");
            return i->r[0];
        }
        return i->r[0];
    }
    case NODE_TERNARY: {
        struct ir_reg cond = emit_one_node(context, NODE(node->ternary.condition), false);
        short cond_id = context->next_condition++;
        struct ir_reg cnd_false = (struct ir_reg) { .iname = tprintf(context, "cnd%i.false", cond_id), };
        struct ir_reg cnd_end = (struct ir_reg) { .iname = tprintf(context, "cnd%i.end", cond_id) };
        {
            ir *i = new(context);
            i->op = JZ;
            i->r[0] = cnd_false;
            i->r[1] = cond;
        }
        struct ir_reg bt = emit_one_node(context, NODE(node->ternary.branch_true), false);
        {
            ir *i = new(context);
            i->op = MOVE;
            i->r[0] = next(context);
            i->r[1] = bt;
            bt = i->r[0];
        }
        {
            ir *i = new(context);
            i->op = JMP;
            i->r[0] = cnd_end;
        }
        {
            ir *i = new(context);
            i->op = LABEL;
            i->r[0] = cnd_false;
        }
        struct ir_reg bf = emit_one_node(context, NODE(node->ternary.branch_false), false);
        {
            ir *i = new(context);
            i->op = MOVE;
            i->r[0] = next(context);
            i->r[1] = bf;
            bf = i->r[0];
        }
        {
            ir *i = new(context);
            i->op = LABEL;
            i->r[0] = cnd_end;
        }
        ir *i = new(context);
        i->op = PHI;
        i->r[0] = next(context);
        i->r[1] = bt;
        i->r[2] = bf;
        return i->r[0];
    }
    case NODE_IF: {
        struct ir_reg cond = emit_one_node(context, NODE(node->if_.cond), false);
        short cond_id = context->next_condition++;
        struct ir_reg cnd_false = (struct ir_reg) { .iname = tprintf(context, "if%i.else", cond_id), };
        struct ir_reg cnd_end = (struct ir_reg) { .iname = tprintf(context, "if%i.end", cond_id) };
        if (node->if_.block_false) {
            ir *i = new(context);
            i->op = JZ;
            i->r[0] = cnd_false;
            i->r[1] = cond;
        } else {
            ir *i = new(context);
            i->op = JZ;
            i->r[0] = cnd_end;
            i->r[1] = cond;
        }
        emit_one_node(context, NODE(node->if_.block_true), false);
        if (node->if_.block_false) {
            {
                ir *i = new(context);
                i->op = JMP;
                i->r[0] = cnd_end;
            }
            {
                ir *i = new(context);
                i->op = LABEL;
                i->r[0] = cnd_false;
            }
            emit_one_node(context, NODE(node->if_.block_false), false);
        }
        {
            ir *i = new(context);
            i->op = LABEL;
            i->r[0] = cnd_end;
        }
        return (struct ir_reg){};
    }
    case NODE_RETURN: {
        struct ir_reg rv = emit_one_node(context, NODE(node->ret.expr), false);
        ir *i = new(context);
        i->op = RET;
        i->r[0] = rv;
        return i->r[0];
    }
    case NODE_ROOT:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i += 1) {
            emit_one_node(context, NODE(node->root.children[i]), false);
        }
        break;
    case NODE_FUNCTION_DEFINITION:
        emit_one_node(context, NODE(node->fun.body), false);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i += 1) {
            emit_one_node(context, NODE(node->block.children[i]), false);
        }
        break;
    default:
        fprintf(stderr, "unknown node type in emit_one: %i\n", node->type);
    }
}
