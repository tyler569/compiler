#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "diag.h"
#include "token.h"
#include "parse.h"
#include "tu.h"
#include "type.h"
#include "ir.h"

int main() {
    // const char *source = "int y = 0, *x = &y;\n"
    //                      "int foo(int bar, int *baz) {\n"
    //                      "    int x = 10;\n"
    //                      "    return y << 3;\n"
    //                      "}\n";
    // const char *source = "a, b + 2 & c++, condition ? true : false";
    // const char *source = "a = b, c |= d, 1 ? 2 : 3, -10, *a = b++, *a++, (*a)++, (2+2) * 12";
    // const char *source = "'\\\\', '\\'', '\\n', '\\t', -1, 2.2";
    // const char *source = "foo(a, b, c = 1)() && d[*a++]++";
    // const char *source = "sizeof(10), sizeof 10";
    // const char *source = "int a, *b, c[], d(), *e(), (*f)(), g[100];\n"
    //                      "static_assert(x == 10, \"message\");\n"
    //                      "a += *b;";
    // const char *source = "int foo(int, char, signed);";
    // const char *source = "int (*foo)(); int (*bar())();";
    // const char *source = "int main(int a, int b) {\na = b; int c = a; int d = c; return 0; }\nint a = main;";
    // const char *source = "int main() {\n"
    //                      "    int a, b;\n"
    //                      "    a = 1;\n"
    //                      "    b = 2;\n"
    //                      "    if (a == b) {\n"
    //                      "        a = 10;\n"
    //                      "    }\n"
    //                      // "    a = b ? a : b;\n"
    //                      "    return a;\n"
    //                      "}\n";
    const char *source = "int foo(bool b) {"
                         "    int a = 1;"
                         "    if (b) {"
                         "        a = 2;"
                         "    } else {"
                         "        a = 3;"
                         "    }"
                         "    return a;"
                         "}";

    struct tu *tu = &(struct tu){
        .source = source,
        .filename = "",
        .source_len = strlen(source),
        .abort = false // true,
    };

    tokenize(tu);
    print_tokens(tu);

    parse(tu);
    print_ast(tu);

    type(tu);

    fprintf(stderr, "\n");

    emit(tu);

    for (int i = 0; i < tu->ir_len; i += 1) {
        print_ir_instr(tu, &tu->ir[i]);
    }
}
