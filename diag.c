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

static void print_highlight(int begin, int len) {
    for (int i = 0; i < begin; i += 1) fputc(' ', stderr);
    fputc('^', stderr);
    for (int i = 1; i < len; i += 1) fputc('~', stderr);
    fputc('\n', stderr);
}

static int line_len(const char *line_ptr) {
    int len;
    for (len = 0; line_ptr[len] && line_ptr[len] != '\n'; len += 1) {
        if (line_ptr[len] == '\t') {
            // do something special
        }
    }
    return len;
}

void print_and_highlight(const char *source, struct token *token) {
    if (token->line == 0) return;

    print_line(source, token->index, token->line);
    print_highlight(token->column + 4, token->len);
}

void print_and_highlight_extent(struct tu *tu, struct token *begin, struct token *end) {
    if (begin == end) {
        return print_and_highlight(tu->source, begin);
    }

    print_line(tu->source, begin->index, begin->line);

    if (begin->line != end->line) {
        int len = line_len(tu->source + begin->index);
        print_highlight(begin->column + 4, len);
    } else {
        print_highlight(begin->column + 4, end->column + end->len - begin->column);
    }
}

void print_error(struct tu *tu, struct node *node, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    print_and_highlight_extent(tu, node_begin(node), node_end(node));

    va_end(args);
}
