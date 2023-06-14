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
        for_each (&reg->phi_list) {
            print_ir_reg(tu, it);
            fprintf(stderr, ", ");
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
        fprintf(stderr, "label: %s:\n", i->r[0].label);
        break;

    case JMP:
        fprintf(stderr, "jmp %s\n", i->r[0].label);
        break;

    case JZ:
        fprintf(stderr, "jz %s, ", i->r[0].label);
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

// struct ir_reg emit_one_node(struct context *context, struct node *node, bool write);
struct ir_reg bb_emit_node(struct tu *tu, struct function *function, struct node *node, bool write);
struct function *new_function();

int emit(struct tu *tu) {
    struct function *function = new_function();

    bb_emit_node(tu, function, tu->nodes, false);

    for_each_v (&function->bbs) {
        fprintf(stderr, "new bb:\n");

        for_each_n (instr, &it->ir_list) {
            print_ir_instr(tu, instr);
        }
    }

    return 0;
}

// struct ir_reg emit_one_node(struct context *context, struct node *node, bool write) {
//     switch (node->type) {
//     case NODE_INT_LITERAL: {
//         ir *i = new(context);
//         i->r[0] = next(context);
//         i->op = IMM;
//         i->immediate_i = node->token->int_.value;
//         return i->r[0];
//     }
//     case NODE_FLOAT_LITERAL: {
//         ir *i = new(context);
//         i->r[0] = next(context);
//         i->op = IMM;
//         i->immediate_f = node->token->float_.value;
//         return i->r[0];
//     }
//     case NODE_IDENT: {
//         if (write) SCOPE(node->ident.scope_id)->ir_index += 1;
//         struct ir_reg reg = {
//             .name = node->token,
//             .index = SCOPE(node->ident.scope_id)->ir_index,
//         };
//         return reg;
//     }
//     case NODE_BINARY_OP: {
//         if (node->token->type == '=') {
//             struct ir_reg in = emit_one_node(context, node->binop.rhs, false);
//             struct ir_reg out = emit_one_node(context, node->binop.lhs, true);
//
//             ir *i = new(context);
//             i->op = MOVE;
//             i->r[0] = out;
//             i->r[1] = in;
//             return i->r[0];
//         }
//         struct ir_reg lhs = emit_one_node(context, node->binop.lhs, false);
//         struct ir_reg rhs = emit_one_node(context, node->binop.rhs, false);
//
//         ir *i = new(context);
//         i->r[0] = next(context);
//         switch (node->token->type) {
//         case '+': i->op = ADD; break;
//         case '-': i->op = SUB; break;
//         case '*': i->op = MUL; break;
//         case '/': i->op = DIV; break;
//         case '%': i->op = MOD; break;
//         case TOKEN_SHIFT_RIGHT: i->op = SHR; break;
//         case TOKEN_SHIFT_LEFT: i->op = SHL; break;
//         case '&': i->op = AND; break;
//         case '|': i->op = OR; break;
//         case '^': i->op = XOR; break;
//         case TOKEN_EQUAL_EQUAL: i->op = TEST; break;
//         default:
//             report_error(context, "unhandled binary operation");
//         }
//         i->r[1] = lhs;
//         i->r[2] = rhs;
//         return i->r[0];
//     }
//     case NODE_UNARY_OP: {
//         struct ir_reg inner = emit_one_node(context, node->unary_op.inner, false);
//         if (node->token->type == '+') return inner;
//
//         ir *i = new(context);
//         i->r[0] = next(context);
//         i->r[1] = inner;
//         switch (node->token->type) {
//         case '-': i->op = NEG; break;
//         case '~': i->op = INV; break;
//         case '!': i->op = NOT; break;
//         case '*':
//         case '&':
//             report_error(context, "pointers not yet supported");
//             return i->r[0];
//         default:
//             report_error(context, "unknown unary operator");
//             return i->r[0];
//         }
//         return i->r[0];
//     }
//     case NODE_TERNARY: {
//         struct ir_reg cond = emit_one_node(context, node->ternary.condition, false);
//         short cond_id = context->next_condition++;
//         struct ir_reg cnd_false = (struct ir_reg) { .label = tprintf(context, "cnd%i.false", cond_id), };
//         struct ir_reg cnd_end = (struct ir_reg) { .label = tprintf(context, "cnd%i.end", cond_id) };
//         {
//             ir *i = new(context);
//             i->op = JZ;
//             i->r[0] = cnd_false;
//             i->r[1] = cond;
//         }
//         struct ir_reg bt = emit_one_node(context, node->ternary.branch_true, false);
//         {
//             ir *i = new(context);
//             i->op = MOVE;
//             i->r[0] = next(context);
//             i->r[1] = bt;
//             bt = i->r[0];
//         }
//         {
//             ir *i = new(context);
//             i->op = JMP;
//             i->r[0] = cnd_end;
//         }
//         {
//             ir *i = new(context);
//             i->op = LABEL;
//             i->r[0] = cnd_false;
//         }
//         struct ir_reg bf = emit_one_node(context, node->ternary.branch_false, false);
//         {
//             ir *i = new(context);
//             i->op = MOVE;
//             i->r[0] = next(context);
//             i->r[1] = bf;
//             bf = i->r[0];
//         }
//         {
//             ir *i = new(context);
//             i->op = LABEL;
//             i->r[0] = cnd_end;
//         }
//         ir *i = new(context);
//         i->op = PHI;
//         i->r[0] = next(context);
//         i->r[1] = bt;
//         i->r[2] = bf;
//         return i->r[0];
//     }
//     case NODE_IF: {
//         struct ir_reg cond = emit_one_node(context, node->if_.cond, false);
//         short cond_id = context->next_condition++;
//         struct ir_reg cnd_false = (struct ir_reg) { .label = tprintf(context, "if%i.else", cond_id), };
//         struct ir_reg cnd_end = (struct ir_reg) { .label = tprintf(context, "if%i.end", cond_id) };
//         if (node->if_.block_false) {
//             ir *i = new(context);
//             i->op = JZ;
//             i->r[0] = cnd_false;
//             i->r[1] = cond;
//         } else {
//             ir *i = new(context);
//             i->op = JZ;
//             i->r[0] = cnd_end;
//             i->r[1] = cond;
//         }
//         emit_one_node(context, node->if_.block_true, false);
//         if (node->if_.block_false) {
//             {
//                 ir *i = new(context);
//                 i->op = JMP;
//                 i->r[0] = cnd_end;
//             }
//             {
//                 ir *i = new(context);
//                 i->op = LABEL;
//                 i->r[0] = cnd_false;
//             }
//             emit_one_node(context, node->if_.block_false, false);
//         }
//         {
//             ir *i = new(context);
//             i->op = LABEL;
//             i->r[0] = cnd_end;
//         }
//         return (struct ir_reg){};
//     }
//     case NODE_RETURN: {
//         struct ir_reg rv = emit_one_node(context, node->ret.expr, false);
//         ir *i = new(context);
//         i->op = RET;
//         i->r[0] = rv;
//         return i->r[0];
//     }
//     case NODE_ROOT:
//         for (int i = 0; i < MAX_BLOCK_MEMBERS && node->root.children[i]; i += 1) {
//             emit_one_node(context, node->root.children[i], false);
//         }
//         break;
//     case NODE_FUNCTION_DEFINITION:
//         emit_one_node(context, node->fun.body, false);
//         break;
//     case NODE_BLOCK:
//         for (int i = 0; i < MAX_BLOCK_MEMBERS && node->block.children[i]; i += 1) {
//             emit_one_node(context, node->block.children[i], false);
//         }
//         break;
//     default:
//         fprintf(stderr, "unknown node type in emit_one: %i\n", node->type);
//     }
//     return (struct ir_reg){};
// }



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

static struct ir_reg lookup_ident_bb(struct tu *tu, struct function *function, struct scope *scope, bool write, struct bb *bb) {
    for_each (&bb->owned_registers) {
        if (it->scope == scope) {
            return new_scope_reg(scope, write);
        }
    }

    if (bb->inputs.len == 1) {
        return lookup_ident_bb(tu, function, scope, write, bb->inputs.data[0]);
    }

    if (bb->inputs.len == 0) {
        reg r = new_scope_reg(scope, write);
        list_push(&bb->owned_registers, r);
        return r;
    }

    reg pre_phi = (reg){
        .is_phi = true,
        .scope = scope,
    };

    for_each_v (&bb->inputs) {
        for_each_n (r, &it->owned_registers) {
            if (r->scope == scope) {
                list_push(&pre_phi.phi_list, *r);
            }
        }
    }

    if (pre_phi.phi_list.len == 0) {
        reg r = new_scope_reg(scope, write);
        list_push(&bb->owned_registers, r);
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
        bool inner_write = false;
        if (node->token->type == '=') inner_write = true;
        reg lhs = bb_emit_node(tu, function, node->binop.lhs, inner_write);
        reg rhs = bb_emit_node(tu, function, node->binop.rhs, false);
        reg res = new_temporary(function);
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
            break;
        }
        if (op == MOVE) {
            EMIT(ir_move(lhs, rhs));
            return lhs;
        } else {
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