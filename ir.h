#pragma once
#ifndef COMPILER_IR_H
#define COMPILER_IR_H

#include <stdint.h>

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
    CALL,   // name + args unknown
    RET,    // r1
    TEST,   // r1 <- r2 < r3
    JZ,     // pc <- ? if ?
    JMP,    // pc <- ?

    PHI,    // r0 <- phi [ r1, r2 ]
};

struct ir_reg {
    union {
        const char *iname;
        struct token *name;
    };
    short index;
};

struct ir_instr {
    union {
        struct token *name;
        uint64_t immediate_i;
        double immediate_f;
    };
    struct ir_reg r[3];
    enum ir_op op;
};

typedef struct ir_instr ir;
struct tu;

int emit(struct tu *tu);
void print_ir_instr(struct tu *tu, struct ir_instr *i);

#endif //COMPILER_IR_H
