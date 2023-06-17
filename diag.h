#pragma once
#ifndef COMPILER_DIAG_H
#define COMPILER_DIAG_H

struct token;
struct tu;
struct node;

void print_line(const char *source, int position, int line_number);
void print_and_highlight(const char *source, struct token *token);
void print_error(struct tu *tu, struct node *node, const char *format, ...);

#endif //COMPILER_DIAG_H
