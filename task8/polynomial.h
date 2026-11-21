#ifndef TASK8_POLYNOMIAL_H
#define TASK8_POLYNOMIAL_H

#include <stdint.h>
#include <stdio.h>

typedef struct polynomial_term {
    intmax_t coefficient;
    uintmax_t degree;
    struct polynomial_term *next;
} polynomial_term_t;

typedef struct {
    polynomial_term_t *head;
} polynomial_t;

typedef enum {
    ST_OK,
    ST_INVALID_ARGUMENT,
    ST_OVERFLOW,
    ST_NO_MEMORY,
    ST_OUTPUT_ERROR,
    ST_DIV_ZERO
} Status;

Status polynomial_add_term(polynomial_t *polynomial,
                           intmax_t coefficient, uintmax_t degree);

Status polynomial_copy(polynomial_t *destination, const polynomial_t *source);

void polynomial_clear(polynomial_t *polynomial);

Status polynomial_print(FILE *stream, const polynomial_t *polynomial);

Status polynomial_add(const polynomial_t *a, const polynomial_t *b, polynomial_t *out);
Status polynomial_sub(const polynomial_t *a, const polynomial_t *b, polynomial_t *out);
Status polynomial_mult(const polynomial_t *a, const polynomial_t *b, polynomial_t *out);
Status polynomial_div(const polynomial_t *a, const polynomial_t *b, polynomial_t *out);
Status polynomial_mod(const polynomial_t *a, const polynomial_t *b, polynomial_t *out);
Status polynomial_eval(const polynomial_t *p, intmax_t x, intmax_t *out);
Status polynomial_diff(const polynomial_t *p, polynomial_t *out);
Status polynomial_cmps(const polynomial_t *p, const polynomial_t *q, polynomial_t *out);

#endif
