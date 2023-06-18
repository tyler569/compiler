#pragma once
#ifndef COMPILER_DIAG_H
#define COMPILER_DIAG_H

#include <stdarg.h>

struct token;
struct tu;
struct node;

void print_and_highlight(const char *source, struct token *token);
void print_error(struct tu *tu, const char *format, ...);
void print_error_node(struct tu *tu, struct node *node, const char *format, ...);
void print_info_node(struct tu *tu, struct node *node, const char *format, ...);
void print_error_token(struct tu *tu, struct token *token, const char *format, ...);
void vprint_error(struct tu *tu, const char *format, va_list args);
void vprint_error_node(struct tu *tu, struct node *node, const char *format, va_list args);
void vprint_error_token(struct tu *tu, struct token *token, const char *format, va_list);

void error_abort(struct tu *tu, const char *format, ...);

#endif //COMPILER_DIAG_H
