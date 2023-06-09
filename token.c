#include "token.h"

#include <ctype.h>
#include <stdio.h>

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


struct token *tokenize(size_t len, const char *source, const char *filename) {
    struct state *state = &(struct state){
        .len = len,
        .source = source,
        .filename = filename,
    };

    while (more_data(state)) {
        skip_whitespace(state);

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

    return state->ta.tokens;
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

static void read_ident(struct state *state) {
    struct token *token = new(state, TOKEN_IDENT);

    while (is_ident(CHAR(state))) {
        pass(state);
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

    char c = CHAR(state);
    if (c == '0' && PEEK(state) == 'x') {
        eat(state, '0');
        eat(state, 'x');
        while (is_xdigit(CHAR(state))) {
            pass(state);
        }
    } else {
        while (is_digit(CHAR(state))) {
            pass(state);
        }
    }

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
    struct token *token = new(state, TOKEN_CHAR);

    state->position += 1;
    // TODO: escaping 's
    while (CHAR(state) != '\'') {
        pass(state);
    }
    eat(state, '\'');

    end(state, token);
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
        putchar(token->type);
    } else switch (token->type) {
#define CASE(tt, str) case (tt): fputs((str), stdout); break;
    CASE(TOKEN_NULL, "null")
    CASE(TOKEN_IDENT, "ident")
    CASE(TOKEN_INT, "int")
    CASE(TOKEN_FLOAT, "float")
    CASE(TOKEN_STRING, "string")
    CASE(TOKEN_CHAR, "char")
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
#undef CASE
    }
    putchar(')');
}