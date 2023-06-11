#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "diag.h"
#include "token.h"
#include "parse.h"
#include "tu.h"

int main() {
    // const char *source = "int x = 10;\nint foo() { return 10 << 3; }";
    // const char *source = "a, b + 2 & c++, condition ? true : false";
    // const char *source = "a = b, c |= d, 1 ? 2 : 3, -10, *a = b++, *a++, (*a)++, (2+2) * 12";
    // const char *source = "'\\\\', '\\'', '\\n', '\\t', -1, 2.2";
    // const char *source = "foo(a, b, c = 1)() && d[*a++]++";
    // const char *source = "sizeof(10), sizeof 10";
    const char *source = "int a, *b, c[], d(), *e(), (*f)(), g[100];\nstatic_assert(x == 10, \"message\");";

    struct tu *tu = &(struct tu){
        .source = source,
        .filename = "",
        .source_len = strlen(source),
        .abort = false // true,
    };

    tokenize(tu);

    // for (struct token *t = tokens; t->type != TOKEN_EOF; t += 1) {
    //     fputs("token", stdout);
    //     print_token_type(t);
    //     printf("@(%i:%i) '%.*s'\n", t->line, t->column, t->len, &source[t->index]);
    //     print_and_highlight(source, t);
    // }

    parse(tu);
    print_ast(tu);
}
