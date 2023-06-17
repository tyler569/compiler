#include "diag.h"
#include "token.h"
#include "tu.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *safe_find_right(const char *base, const char *ptr, char c) {
    for (; *ptr != c && ptr != base; ptr -= 1) {
    }
    return ptr;
}

void print_line(const char *source, int position, int line_number) {
    if (line_number == 0) return;

    const char *start = safe_find_right(source, source + position, '\n');
    if (*start == '\n') start += 1;
    const char *end = strchr(start + 1, '\n');

    int len = (int)(end - start);

    fprintf(stderr, "%3i| %.*s\n", line_number, len, start);
}

void print_and_highlight(const char *source, struct token *token) {
    if (token->line == 0) return;

    print_line(source, token->index, token->line);
    for (int i = 0; i < token->column + 4; i += 1) fputc(' ', stderr);
    fputc('^', stderr);
    for (int i = 1; i < token->len; i += 1) fputc('~', stderr);
    fputc('\n', stderr);
}

void print_error(struct tu *tu, struct node *node, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    print_and_highlight(tu->source, node->token);

    va_end(args);
}
