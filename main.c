#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>

#include "diag.h"
#include "token.h"
#include "parse.h"
#include "tu.h"
#include "type.h"
#include "ir.h"

int main(int argc, char **argv) {
    struct tu *tu = &(struct tu){
        .abort = false // true,
    };

    list_push(&tu->types, (struct type){});
    list_push(&tu->scopes, (struct scope){});

    if (argc < 2) {
        tu->source = "int main() { const int x = 10; register short int y = 11; x + y; }";
        tu->source_len = strlen(tu->source);
    } else {
        int file = open(argv[1], O_RDONLY);
        if (file < 0) {
            print_error(tu, "unable to open file %s (%s)", argv[1], strerror(errno));
            return 1;
        }
        struct stat stat;
        int err = fstat(file, &stat);
        if (err < 0) {
            error_abort(tu, "unable to stat file %s (%s)", argv[1], strerror(errno));
        }
        // TODO: mmap the file?
        char *s = malloc(stat.st_size + 1);
        if (!s) {
            error_abort(tu, "unable to allocate memory %s (%s)", argv[1], strerror(errno));
        }
        s[stat.st_size] = 0;
        err = read(file, s, stat.st_size);
        if (err < 0) {
            error_abort(tu, "unable to read file %s (%s)", argv[1], strerror(errno));
        }
        tu->source = s;
        tu->source_len = stat.st_size;
    }

    tokenize(tu);
    // print_tokens(tu);

    parse(tu);
    print_ast(tu);

    type(tu);

    // fprintf(stderr, "\n");

    // emit(tu);

    // for (int i = 0; i < tu->ir_len; i += 1) {
    //     print_ir_instr(tu, &tu->ir[i]);
    // }
}
