#pragma once
#ifndef COMPILER_DIAG_H
#define COMPILER_DIAG_H

struct token;

void print_line(const char *source, int position, int line_number);
void print_and_highlight(const char *source, struct token *token);

#endif //COMPILER_DIAG_H
