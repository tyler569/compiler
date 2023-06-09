#pragma once
#ifndef COMPILER_IR_H
#define COMPILER_IR_H

#include <stdint.h>
#include <stddef.h>

#include "list.h"

enum ir_op : char {
    LABEL,  // name
    DATA,   // ?
    ADD,    // r1 <- r2 + r3
    SUB,    // r1 <- r2 - r3
    MUL,    // r1 <- r2 * r3
    DIV,    // r1 <- r2 / r3
    MOD,    // r1 <- r2 % r3
    AND,    // r1 <- r2 & r3
    OR,     // r1 <- r2 | r3
    XOR,    // r1 <- r2 ^ r3
    SHR,    // r1 <- r2 >> r3
    SHL,    // r1 <- r2 << r3
    NEG,    // r1 <- -r2
    NOT,    // r1 <- !r2
    INV,    // r1 <- ~r2
    MOVE,   // r1 <- r2
    IMM,    // r1 <- imm
    ST,     // [r1 + r3] <- r2
    LD,     // r1 <- [r2 + r3]
    ADDR,   // r1 <- addr r2
    CALL,   // name + args unknown
    RET,    // r1
    TEST,   // r1 <- r2 < r3
    JZ,     // pc <- ? if ?
    JMP,    // pc <- ?

    PHI,    // r0 <- phi [ r1, r2 ]
};

typedef list(struct ir_instr) ir_list_t;
typedef list(struct ir_reg *) reg_list_t;

struct ir_reg {
    struct scope *scope;
    struct function *function;
    short index;
};

struct ir_instr {
    const char *name;
    uint64_t immediate_i;
    double immediate_f;
    reg_list_t args;
    struct ir_reg *r[3];
    enum ir_op op;
};

struct function {
    short temporary_id;
    short cond_id;
    ir_list_t ir_list;
};

typedef struct ir_instr ir;
typedef struct ir_reg reg;
struct tu;

int emit(struct tu *tu);
void print_ir_instr(struct tu *tu, struct ir_instr *i);

#endif //COMPILER_IR_H
