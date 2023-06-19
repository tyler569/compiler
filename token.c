#include "token.h"
#include "util.h"
#include "tu.h"
#include "diag.h"

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// This affects the column offsets in tokens, when the compiler finds a tab it
// adds extra columns to account for the fact that the byte offset and visual offset
// are different. This should be made configurable with a command line flag in the future.
#define SPACES_PER_TAB 8

struct state {
    int position;
    int line;
    int line_start;
    int extra_columns;
    size_t len;
    const char *source;
    const char *filename;
    struct {
        struct token *tokens;
        size_t len;
        size_t capacity;
    } ta;
    int errors;
};

#define CHAR(state) (state->source[state->position])
#define PEEK(state) (state->source[state->position + 1])

static void skip_whitespace(struct state *);
static void new_line(struct state *);
static bool more_data(struct state *);
static int column(struct state *);
static struct token *new(struct state *, int);
static void end(struct state *, struct token *);
static void report_error(struct state *, const char *message);
static void eat(struct state *, char c);
static void pass(struct state *);
static bool pull(struct state *, char c);

static void read_ident(struct state *);
static void read_number(struct state *);
static void read_string(struct state *);
static void read_char(struct state *);
static void read_symbol(struct state *);

int tokenize(struct tu *tu) {
    struct state *state = &(struct state){
        .len = tu->source_len,
        .source = tu->source,
        .filename = tu->filename,
    };

    while (more_data(state)) {
        skip_whitespace(state);
        if (!more_data(state)) break;

        char c = CHAR(state);
        if (isalpha(c)) {
            read_ident(state);
        } else if (isnumber(c)) {
            read_number(state);
        } else if (c == '"') {
            read_string(state);
        } else if (c == '\'') {
            read_char(state);
        } else {
            read_symbol(state);
        }
    }

    struct token *token = new(state, TOKEN_EOF);
    end(state, token);

    tu->tokens = state->ta.tokens;
    tu->tokens_len = state->ta.len;

    return state->errors;
}

// Eat the character 'c' from the tokenization state. Create an error if this is not the correct character.
static void eat(struct state *state, char c) {
    if (CHAR(state) != c) {
        report_error(state, "ate wrong character");
    }
    state->position += 1;
}

// Ignore the current character in state.
static void pass(struct state *state) {
    state->position += 1;
}

// Try to eat the character 'c' from the tokenization state. Return true if it can, false if it can't.
static bool pull(struct state *state, char c) {
    if (CHAR(state) == c) {
        eat(state, c);
        return true;
    }
    return false;
}

static void skip_whitespace(struct state *state) {
    char c = CHAR(state);
    while (isspace(c)) {
        pass(state);
        if (c == '\n') {
            new_line(state);
        }
        if (c == '\t') {
            state->extra_columns += SPACES_PER_TAB - 1;
            state->extra_columns &= ~(1 - SPACES_PER_TAB);
        }
        c = CHAR(state);
    }
}

static void new_line(struct state *state) {
    state->line += 1;
    state->line_start = state->position;
    state->extra_columns = 0;
}

static bool more_data(struct state *state) {
    return state->position < state->len;
}

static int column(struct state *state) {
    return state->position - state->line_start + state->extra_columns;
}

static struct token *new(struct state *state, int token_type) {
    if (state->ta.capacity <= state->ta.len) {
        size_t new_capacity = state->ta.capacity ? state->ta.capacity * 2 : 512;
        struct token *new_ta = realloc(state->ta.tokens, new_capacity * sizeof(struct token));
        if (!new_ta) {
            report_error(state, "memory allocation failed");
            return nullptr;
        }
        state->ta.tokens = new_ta;
        state->ta.capacity = new_capacity;
    }

    struct token *token = &state->ta.tokens[state->ta.len++];
    token->type = token_type;
    token->index = state->position;
    token->line = state->line + 1;
    token->column = column(state) + 1;
    return token;
}

static void end(struct state *state, struct token *token) {
    token->len = state->position - token->index;
}

static void report_error(struct state *state, const char *message) {
    state->errors += 1;
    fprintf(stderr, "Error (%s:%i:%i) %s\n",
            state->filename,
            state->line,
            column(state),
            message);
}

static bool is_ident(char c) {
    return isalnum(c) || c == '_';
}

static const char *keywords[] = {
    "alignas",
    "alignof",
    "auto",
    "bool",
    "break",
    "case",
    "char",
    "const",
    "constexpr",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "false",
    "float",
    "for",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "nullptr",
    "register",
    "restrict",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "struct",
    "switch",
    "thread_local",
    "true",
    "typedef",
    "typeof",
    "typeof_unqual",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
    "_Atomic",
    "_BitInt",
    "_Complex",
    "_Decimal128",
    "_Decimal32",
    "_Decimal64",
    "_Generic",
    "_Imaginary",
    "_Noreturn",
};

static_assert(ARRAY_LEN(keywords) == TOKEN_LAST_KEYWORD - TOKEN_FIRST_KEYWORD);

// could populate the lengths here on startup if we need to
size_t keyword_lens[ARRAY_LEN(keywords)];

static void read_ident(struct state *state) {
    struct token *token = new(state, TOKEN_IDENT);

    const char *first = &CHAR(state);
    while (is_ident(CHAR(state))) {
        pass(state);
    }
    const char *last = &CHAR(state);

    // TODO: This is probably really slow
    for (int i = 0; i < ARRAY_LEN(keywords); i += 1) {
        size_t keyword_len = strlen(keywords[i]);
        size_t token_len = last - first;
        size_t len = keyword_len > token_len ? keyword_len : token_len;
        if (strncmp(keywords[i], first, len) == 0) {
            token->type = TOKEN_ALIGNAS + i;
        }
    }

    end(state, token);
}

static bool is_digit(char c) {
    return isdigit(c) || c == '\'';
}

static bool is_xdigit(char c) {
    return isxdigit(c) || c == '\'';
}

static void read_number(struct state *state) {
    struct token *token = new(state, TOKEN_INT);

    const char *str = &CHAR(state);
    char *after = nullptr;

    // TODO: digit separators. I'll need to make a custom strtoull / strtod

    errno = 0;
    token->int_.value = strtoull(str, &after, 0);

    if (*after == '.' || *after == 'e' || *after == 'p') {
        token->type = TOKEN_FLOAT;
        token->float_.value = strtod(str, &after);
    }

    if (errno == ERANGE) {
        errno = 0;
        report_error(state, "number literal out of range");
    }

    state->position += (int)(after - str);

    end(state, token);
}

static void read_string(struct state *state) {
    struct token *token = new(state, TOKEN_STRING);

    eat(state, '"');
    // TODO: escaping "s
    while (CHAR(state) != '"') {
        pass(state);
    }
    eat(state, '"');

    end(state, token);
}

static void read_char(struct state *state) {
    struct token *token = new(state, TOKEN_INT);
    uint64_t value = 0;
#define VALUE_PUSH(v) (value <<= 8, value |= (v))
#define ESCAPE_CASE(l, e) case (l): VALUE_PUSH((e)); break

    eat(state, '\'');
    bool in_escape = false;
    bool cont = true;

    while (cont) {
        if (in_escape) {
            switch (CHAR(state)) {
            ESCAPE_CASE('\\', '\\');
            ESCAPE_CASE('\'', '\'');
            ESCAPE_CASE('?', '\?');
            ESCAPE_CASE('"', '"');
            ESCAPE_CASE('a', '\a');
            ESCAPE_CASE('b', '\b');
            ESCAPE_CASE('f', '\f');
            ESCAPE_CASE('n', '\n');
            ESCAPE_CASE('r', '\r');
            ESCAPE_CASE('t', '\t');
            ESCAPE_CASE('v', '\v');
            case 'x':
                report_error(state, "hex escape codes not yet implemented");
                break;
            default:
                if (CHAR(state) >= '0' && CHAR(state) < '8')
                    report_error(state, "octal escape codes not yet implemeted");
                else
                    report_error(state, "unknown escape code");
            }
            in_escape = false;
        } else {
            switch (CHAR(state)) {
            case '\\':
                in_escape = true;
                break;
            case '\'':
                cont = false;
                break;
            default:
                VALUE_PUSH(CHAR(state)); break;
            }
        }
        pass(state);
    }

    token->int_.value = value;

    // eat(state, '\'');
    end(state, token);
#undef VALUE_PUSH
#undef ESCAPE_CASE
}

static void read_symbol(struct state *state) {
    struct token *token = new(state, TOKEN_NULL);

    char c = CHAR(state);
    pass(state);
    switch (c) {
    case '!':
        if (pull(state, '=')) token->type = TOKEN_NOT_EQUAL;
        break;
    case '+':
        if (pull(state, '=')) token->type = TOKEN_PLUS_EQUAL;
        else if (pull(state, '+')) token->type = TOKEN_PLUS_PLUS;
        break;
    case '-':
        if (pull(state, '=')) token->type = TOKEN_MINUS_EQUAL;
        else if (pull(state, '-')) token->type = TOKEN_MINUS_MINUS;
        else if (pull(state, '>')) token->type = TOKEN_ARROW;
        break;
    case '*':
        if (pull(state, '=')) token->type = TOKEN_STAR_EQUAL;
        break;
    case '/':
        if (pull(state, '=')) token->type = TOKEN_DIVIDE_EQUAL;
        break;
    case '%':
        if (pull(state, '=')) token->type = TOKEN_MOD_EQUAL;
        break;
    case '^':
        if (pull(state, '=')) token->type = TOKEN_BITXOR_EQUAL;
        break;
    case '=':
        if (pull(state, '=')) token->type = TOKEN_EQUAL_EQUAL;
        break;
    case ':':
        if (pull(state, ':')) token->type = TOKEN_COLON_COLON;
        break;
    case '>':
        if (pull(state, '>')) {
            if (pull(state, '=')) token->type = TOKEN_SHIFT_RIGHT_EQUAL;
            else token->type = TOKEN_SHIFT_RIGHT;
        } else if (pull(state, '=')) token->type = TOKEN_GREATER_EQUAL;
        break;
    case '<':
        if (pull(state, '<')) {
            if (pull(state, '=')) token->type = TOKEN_SHIFT_LEFT_EQUAL;
            else token->type = TOKEN_SHIFT_LEFT;
        } else if (pull(state, '=')) token->type = TOKEN_LESS_EQUAL;
        break;
    case '|':
        if (pull(state, '|')) {
            if (pull(state, '=')) token->type = TOKEN_OR_EQUAL;
            else token->type = TOKEN_OR;
        } else if (pull(state, '=')) token->type = TOKEN_BITOR_EQUAL;
        break;
    case '&':
        if (pull(state, '&')) {
            if (pull(state, '=')) token->type = TOKEN_AND_EQUAL;
            else token->type = TOKEN_AND;
        } else if (pull(state, '=')) token->type = TOKEN_BITAND_EQUAL;
        break;
    case '.':
        if (PEEK(state) == '.' && state->source[state->position + 2] == '.') {
            pass(state);
            pass(state);
            token->type = TOKEN_ELLIPSES;
            break;
        }
    default:
    }

    if (token->type == TOKEN_NULL) {
        token->type = (int)c;
    }

    end(state, token);
}

void print_token_type(struct token *token) {
    putchar('(');
    if (token->type != 0 && token->type < 128) {
        putchar('\'');
        putchar(token->type);
        putchar('\'');
    } else if (token->type >= TOKEN_FIRST_KEYWORD) {
        fputs(keywords[token->type - TOKEN_FIRST_KEYWORD], stdout);
    } else switch (token->type) {
#define CASE(tt, str) case (tt): fputs((str), stdout); break;
    CASE(TOKEN_NULL, "null")
    CASE(TOKEN_IDENT, "ident")
    CASE(TOKEN_INT, "int")
    CASE(TOKEN_FLOAT, "float")
    CASE(TOKEN_STRING, "string")
    CASE(TOKEN_EOF, "eof")
    CASE(TOKEN_ARROW, "->")
    CASE(TOKEN_EQUAL_EQUAL, "==")
    CASE(TOKEN_NOT_EQUAL, "!=")
    CASE(TOKEN_GREATER_EQUAL, ">=")
    CASE(TOKEN_LESS_EQUAL, "<=")
    CASE(TOKEN_PLUS_EQUAL, "+=")
    CASE(TOKEN_MINUS_EQUAL, "-=")
    CASE(TOKEN_STAR_EQUAL, "*=")
    CASE(TOKEN_DIVIDE_EQUAL, "/=")
    CASE(TOKEN_MOD_EQUAL, "%=")
    CASE(TOKEN_AND_EQUAL, "&&=")
    CASE(TOKEN_OR_EQUAL, "||=")
    CASE(TOKEN_BITAND_EQUAL, "&=")
    CASE(TOKEN_BITOR_EQUAL, "|=")
    CASE(TOKEN_BITXOR_EQUAL, "^=")
    CASE(TOKEN_AND, "&&")
    CASE(TOKEN_OR, "||")
    CASE(TOKEN_PLUS_PLUS, "++")
    CASE(TOKEN_MINUS_MINUS, "--")
    CASE(TOKEN_SHIFT_RIGHT, ">>")
    CASE(TOKEN_SHIFT_RIGHT_EQUAL, ">>=")
    CASE(TOKEN_SHIFT_LEFT, "<<")
    CASE(TOKEN_SHIFT_LEFT_EQUAL, "<<=")
    CASE(TOKEN_ELLIPSES, "...")
    CASE(TOKEN_COLON_COLON, "::")
#undef CASE
    }
    putchar(')');
}

void print_tokens(struct tu *tu) {
    for (int i = 0; i < tu->tokens_len; i += 1) {
        struct token *t = &tu->tokens[i];

        fputs("token", stdout);
        print_token_type(t);
        printf("@(%i:%i) '%.*s'\n", t->line, t->column, t->len, &tu->source[t->index]);

        print_and_highlight(tu->source, t);
    }
}

const char *token_type_string(int token_type) {
    if (token_type >= TOKEN_FIRST_KEYWORD && token_type < TOKEN_LAST_KEYWORD) {
        return keywords[token_type - TOKEN_FIRST_KEYWORD];
    }
    switch (token_type) {
    case '+': return "+";
    case '-': return "-";
    case '*': return "*";
    case '/': return "/";
    case '%': return "%";
    case '!': return "!";
    case '.': return ".";
    case '<': return "<";
    case '>': return ">";
    case '[': return "[";
    case ']': return "]";
    case '(': return "(";
    case ')': return ")";
    case '^': return "^";
    case '&': return "&";
    case '|': return "|";
    case '~': return "~";
    case ',': return ",";
    case ':': return ":";
    case ';': return ";";
#define CASE(tt, str) case (tt): return str;
    CASE(TOKEN_NULL, "null")
    CASE(TOKEN_IDENT, "ident")
    CASE(TOKEN_INT, "int")
    CASE(TOKEN_FLOAT, "float")
    CASE(TOKEN_STRING, "string")
    CASE(TOKEN_EOF, "eof")
    CASE(TOKEN_ARROW, "->")
    CASE(TOKEN_EQUAL_EQUAL, "==")
    CASE(TOKEN_NOT_EQUAL, "!=")
    CASE(TOKEN_GREATER_EQUAL, ">=")
    CASE(TOKEN_LESS_EQUAL, "<=")
    CASE(TOKEN_PLUS_EQUAL, "+=")
    CASE(TOKEN_MINUS_EQUAL, "-=")
    CASE(TOKEN_STAR_EQUAL, "*=")
    CASE(TOKEN_DIVIDE_EQUAL, "/=")
    CASE(TOKEN_MOD_EQUAL, "%=")
    CASE(TOKEN_AND_EQUAL, "&&=")
    CASE(TOKEN_OR_EQUAL, "||=")
    CASE(TOKEN_BITAND_EQUAL, "&=")
    CASE(TOKEN_BITOR_EQUAL, "|=")
    CASE(TOKEN_BITXOR_EQUAL, "^=")
    CASE(TOKEN_AND, "&&")
    CASE(TOKEN_OR, "||")
    CASE(TOKEN_PLUS_PLUS, "++")
    CASE(TOKEN_MINUS_MINUS, "--")
    CASE(TOKEN_SHIFT_RIGHT, ">>")
    CASE(TOKEN_SHIFT_RIGHT_EQUAL, ">>=")
    CASE(TOKEN_SHIFT_LEFT, "<<")
    CASE(TOKEN_SHIFT_LEFT_EQUAL, "<<=")
    CASE(TOKEN_ELLIPSES, "...")
    CASE(TOKEN_COLON_COLON, "::")
#undef CASE
    default:
        return "unknown token decl_spec";
    }
}