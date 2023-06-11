#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "diag.h"
#include "token.h"
#include "parse.h"
#include "tu.h"
#include "type.h"

int main() {
    const char *source = "int y = 0, *x = &y;\nint foo() { int x = 10; return y << 3; }";
    // const char *source = "a, b + 2 & c++, condition ? true : false";
    // const char *source = "a = b, c |= d, 1 ? 2 : 3, -10, *a = b++, *a++, (*a)++, (2+2) * 12";
    // const char *source = "'\\\\', '\\'', '\\n', '\\t', -1, 2.2";
    // const char *source = "foo(a, b, c = 1)() && d[*a++]++";
    // const char *source = "sizeof(10), sizeof 10";
    // const char *source = "int a, *b, c[], d(), *e(), (*f)(), g[100];\n"
    //                      "static_assert(x == 10, \"message\");\n"
    //                      "a += *b;";

    struct tu *tu = &(struct tu){
        .source = source,
        .filename = "",
        .source_len = strlen(source),
        .abort = false // true,
    };

    tokenize(tu);
    parse(tu);
    print_ast(tu);

    type(tu);
}
