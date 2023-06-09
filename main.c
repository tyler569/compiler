#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "token.h"
#include "parse.h"

// const char *source = "int x = 10;\nint foo() { return 10 << 3; }";
const char *source = "a, b + 2 & c++, condition ? true : false";

void print_line(const char *source, int position, int line);
void print_and_highlight(const char *source, struct token *token);

int main() {
    struct token *tokens = tokenize(strlen(source), source, "");

    for (struct token *t = tokens; t->type != TOKEN_EOF; t += 1) {
        fputs("token", stdout);
        print_token_type(t);
        printf("@(%i:%i) '%.*s'\n", t->line, t->column, t->len, &source[t->index]);

        print_and_highlight(source, t);
    }

    struct node *root = parse(tokens, source);
    print_ast(root, source);
}

const char *safe_find_right(const char *base, const char *ptr, char c) {
    for (; *ptr != c && ptr != base; ptr -= 1) {
    }
    return ptr;
}

void print_line(const char *source, int position, int line_number) {
    const char *start = safe_find_right(source, source + position, '\n');
    if (*start == '\n') start += 1;
    const char *end = strchr(start + 1, '\n');

    int len = (int)(end - start);

    printf("%3i| %.*s\n", line_number, len, start);
}

void print_and_highlight(const char *source, struct token *token) {
    print_line(source, token->index, token->line);
    for (int i = 0; i < token->column + 4; i += 1) putchar(' ');
    putchar('^');
    for (int i = 1; i < token->len; i += 1) putchar('~');
    putchar('\n');
}