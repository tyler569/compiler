#include <stdlib.h>

#define list(T) \
struct {        \
    T *data;    \
    size_t len; \
    size_t cap; \
}


#define list_init(list)     \
do {                        \
    (list)->data = nullptr; \
    (list)->len  = 0;       \
    (list)->cap  = 0;       \
} while (0)

#define list_init_one(list) \
do {                        \
    (list)->data = malloc(sizeof((list)->data[0])); \
    (list)->len = 1;        \
    (list)->cap = 1;        \
} while (0)

#define list_push(list, value) \
do {                           \
    if ((list)->len <= (list)->cap) {                                            \
        size_t new_len = (list)->cap ? (list)->cap * 2 : 16;                     \
        (list)->data = realloc((list)->data, new_len * sizeof((list)->data[0])); \
        (list)->cap = new_len;                                                   \
    }                                    \
    (list)->data[(list)->len++] = value; \
} while (0)

#define list_last_index(list) ((list)->len - 1)

#define list_at(list, index) (list)->data[(index)]
#define list_ptr(list, index) ((list)->data + (index))

#define list_begin(list) &(list)->data[0]
#define list_first(list) (list)->data[0]
#define list_end(list) &(list)->data[(list)->len]
#define list_last(list) (list)->data[(list)->len - 1]

#define list_islast(list, it) ((it) == list_last(list))

#define list_indexof(list, ptr) (ptr - (list)->data)

#define list_clear(list) do { \
    free((list)->data);       \
    (list)->len = 0;          \
    (list)->cap = 0;          \
} while(0)

#define for_each(list) for (typeof((list)->data) it = (list)->data; it < list_end(list); it += 1)
#define for_each_n(N, list) for (typeof((list)->data) N = (list)->data; N < list_end(list); N += 1)
// #define for_each_v(list) for (typeof((list)->data[0]) *_pit = (list)->data, it = *_pit; _pit < &list_end((list)); _pit += 1, it = *_pit)
// #define for_each_vn(N, list) for (typeof((list)->data[0]) *_p##N = (list)->data, N = *_p##N; _p##N < &list_end((list)); _p##N += 1, N = *_p##N)
