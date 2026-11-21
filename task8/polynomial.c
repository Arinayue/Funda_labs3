#include "polynomial.h"

#include <inttypes.h>
#include <stdlib.h>

Status polynomial_add_term(polynomial_t *polynomial,
                                        intmax_t coefficient, uintmax_t degree)
{
    if (polynomial == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (coefficient == 0) {
        return ST_OK;
    }

    polynomial_term_t **position = &polynomial->head;
    while (*position != NULL && (*position)->degree > degree) {
        position = &(*position)->next;
    }

    polynomial_term_t *term = *position;
    if (term != NULL && term->degree == degree) {
        if ((coefficient > 0 && term->coefficient > INTMAX_MAX - coefficient) ||
            (coefficient < 0 && term->coefficient < INTMAX_MIN - coefficient)) {
            return ST_OVERFLOW;
        }

        term->coefficient += coefficient;
        if (term->coefficient == 0) {
            *position = term->next;
            free(term);
        }
        return ST_OK;
    }

    polynomial_term_t *new_term = malloc(sizeof(*new_term));
    if (new_term == NULL) {
        return ST_NO_MEMORY;
    }

    *new_term = (polynomial_term_t){coefficient, degree, term};
    *position = new_term;
    return ST_OK;
}

Status polynomial_copy(polynomial_t *destination,
                                    const polynomial_t *source)
{
    if (destination == NULL || source == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (destination == source) {
        return ST_OK;
    }

    polynomial_t copy = {0};
    polynomial_term_t **tail = &copy.head;
    for (const polynomial_term_t *term = source->head; term != NULL; term = term->next) {
        polynomial_term_t *new_term = malloc(sizeof(*new_term));
        if (new_term == NULL) {
            polynomial_clear(&copy);
            return ST_NO_MEMORY;
        }

        *new_term = (polynomial_term_t){term->coefficient, term->degree, NULL};
        *tail = new_term;
        tail = &new_term->next;
    }

    polynomial_clear(destination);
    *destination = copy;
    return ST_OK;
}

void polynomial_clear(polynomial_t *polynomial)
{
    if (polynomial == NULL) {
        return;
    }

    polynomial_term_t *term = polynomial->head;
    while (term != NULL) {
        polynomial_term_t *next = term->next;
        free(term);
        term = next;
    }
    polynomial->head = NULL;
}

Status polynomial_print(FILE *stream, const polynomial_t *polynomial)
{
    if (stream == NULL || polynomial == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (polynomial->head == NULL) {
        return fputc('0', stream) == EOF ? ST_OUTPUT_ERROR : ST_OK;
    }

    for (const polynomial_term_t *term = polynomial->head; term != NULL; term = term->next) {
        if (term != polynomial->head && term->coefficient > 0 && fputc('+', stream) == EOF) {
            return ST_OUTPUT_ERROR;
        }

        if (term->degree == 0 || (term->coefficient != 1 && term->coefficient != -1)) {
            if (fprintf(stream, "%" PRIdMAX, term->coefficient) < 0) {
                return ST_OUTPUT_ERROR;
            }
        } else if (term->coefficient < 0 && fputc('-', stream) == EOF) {
            return ST_OUTPUT_ERROR;
        }

        if (term->degree > 0 && fputc('x', stream) == EOF) {
            return ST_OUTPUT_ERROR;
        }
        if (term->degree > 1 && fprintf(stream, "^%" PRIuMAX, term->degree) < 0) {
            return ST_OUTPUT_ERROR;
        }
    }
    return ST_OK;
}

static Status save_result(polynomial_t *out, polynomial_t *result,
                                       Status status)
{
    if (status == ST_OK) {
        polynomial_clear(out);
        *out = *result;
    } else {
        polynomial_clear(result);
    }
    return status;
}

static Status push_term(polynomial_term_t ***tail, intmax_t value, uintmax_t degree)
{
    if (value == 0) {
        return ST_OK;
    }
    polynomial_term_t *term = malloc(sizeof(*term));
    if (term == NULL) {
        return ST_NO_MEMORY;
    }
    *term = (polynomial_term_t){value, degree, NULL};
    **tail = term;
    *tail = &term->next;
    return ST_OK;
}

static Status sum_values(intmax_t a, intmax_t b, int sub, intmax_t *out)
{
    if (sub) {
        if ((b > 0 && a < INTMAX_MIN + b) || (b < 0 && a > INTMAX_MAX + b)) {
            return ST_OVERFLOW;
        }
        *out = a - b;
    } else {
        if ((b > 0 && a > INTMAX_MAX - b) || (b < 0 && a < INTMAX_MIN - b)) {
            return ST_OVERFLOW;
        }
        *out = a + b;
    }
    return ST_OK;
}

static Status mult_values(intmax_t a, intmax_t b, intmax_t *out)
{
    if ((b > 0 && (a > INTMAX_MAX / b || a < INTMAX_MIN / b)) ||
        (b < 0 && ((a > 0 && b < INTMAX_MIN / a) || (a < 0 && a < INTMAX_MAX / b)))) {
        return ST_OVERFLOW;
    }
    *out = a * b;
    return ST_OK;
}

static Status sum_polynomials(const polynomial_t *a, const polynomial_t *b,
                                           int sub, polynomial_t *out)
{
    if (a == NULL || b == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    polynomial_t result = {0};
    polynomial_term_t **tail = &result.head;
    const polynomial_term_t *left = a->head;
    const polynomial_term_t *right = b->head;
    Status status = ST_OK;
    while ((left != NULL || right != NULL) && status == ST_OK) {
        uintmax_t degree = left == NULL ? right->degree : left->degree;
        if (right != NULL && right->degree > degree) {
            degree = right->degree;
        }
        intmax_t x = 0;
        intmax_t y = 0;
        if (left != NULL && left->degree == degree) {
            x = left->coefficient;
            left = left->next;
        }
        if (right != NULL && right->degree == degree) {
            y = right->coefficient;
            right = right->next;
        }
        intmax_t value;
        status = sum_values(x, y, sub, &value);
        if (status == ST_OK) {
            status = push_term(&tail, value, degree);
        }
    }
    return save_result(out, &result, status);
}

Status polynomial_add(const polynomial_t *a, const polynomial_t *b, polynomial_t *out)
{
    return sum_polynomials(a, b, 0, out);
}

Status polynomial_sub(const polynomial_t *a, const polynomial_t *b, polynomial_t *out)
{
    return sum_polynomials(a, b, 1, out);
}

Status polynomial_mult(const polynomial_t *a, const polynomial_t *b, polynomial_t *out)
{
    if (a == NULL || b == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    polynomial_t result = {0};
    for (const polynomial_term_t *left = a->head; left != NULL; left = left->next) {
        for (const polynomial_term_t *right = b->head; right != NULL; right = right->next) {
            if (left->degree > UINTMAX_MAX - right->degree) {
                return save_result(out, &result, ST_OVERFLOW);
            }
            intmax_t value;
            Status status = mult_values(left->coefficient, right->coefficient, &value);
            if (status == ST_OK) {
                status = polynomial_add_term(&result, value, left->degree + right->degree);
            }
            if (status != ST_OK) {
                return save_result(out, &result, status);
            }
        }
    }
    return save_result(out, &result, ST_OK);
}

static Status div_polynomials(const polynomial_t *a, const polynomial_t *b,
                                           int want_rest, polynomial_t *out)
{
    if (a == NULL || b == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (b->head == NULL) {
        return ST_DIV_ZERO;
    }

    polynomial_t rest = {0};
    polynomial_t part = {0};
    polynomial_term_t **tail = &part.head;
    Status status = polynomial_copy(&rest, a);
    const polynomial_term_t *first = b->head;
    while (status == ST_OK && rest.head != NULL && rest.head->degree >= first->degree) {
        if (rest.head->coefficient == INTMAX_MIN && first->coefficient == -1) {
            status = ST_OVERFLOW;
            break;
        }
        intmax_t value = rest.head->coefficient / first->coefficient;
        if (value == 0) {
            break;
        }
        uintmax_t degree = rest.head->degree - first->degree;
        if (!want_rest) {
            status = push_term(&tail, value, degree);
        }

        polynomial_t step = {0};
        polynomial_term_t **step_tail = &step.head;
        for (const polynomial_term_t *term = first; term != NULL && status == ST_OK;
             term = term->next) {
            intmax_t coef;
            status = mult_values(value, term->coefficient, &coef);
            if (status == ST_OK) {
                status = push_term(&step_tail, coef, degree + term->degree);
            }
        }
        if (status == ST_OK) {
            status = polynomial_sub(&rest, &step, &rest);
        }
        polynomial_clear(&step);
    }

    if (want_rest) {
        return save_result(out, &rest, status);
    }
    polynomial_clear(&rest);
    return save_result(out, &part, status);
}

Status polynomial_div(const polynomial_t *a, const polynomial_t *b, polynomial_t *out)
{
    return div_polynomials(a, b, 0, out);
}

Status polynomial_mod(const polynomial_t *a, const polynomial_t *b, polynomial_t *out)
{
    return div_polynomials(a, b, 1, out);
}

static Status eval_power(intmax_t *value, intmax_t x, uintmax_t degree)
{
    if (degree == 0 || *value == 0 || x == 1) {
        return ST_OK;
    }
    if (x == 0) {
        *value = 0;
        return ST_OK;
    }
    if (x == -1) {
        return degree % 2 == 0 ? ST_OK : mult_values(*value, x, value);
    }

    while (degree > 0) {
        Status status = mult_values(*value, x, value);
        if (status != ST_OK) {
            return status;
        }
        --degree;
    }
    return ST_OK;
}

Status polynomial_eval(const polynomial_t *p, intmax_t x, intmax_t *out)
{
    if (p == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    intmax_t value = 0;
    uintmax_t degree = p->head == NULL ? 0 : p->head->degree;
    for (const polynomial_term_t *term = p->head; term != NULL; term = term->next) {
        Status status = eval_power(&value, x, degree - term->degree);
        if (status == ST_OK) {
            status = sum_values(value, term->coefficient, 0, &value);
        }
        if (status != ST_OK) {
            return status;
        }
        degree = term->degree;
    }
    Status status = eval_power(&value, x, degree);
    if (status == ST_OK) {
        *out = value;
    }
    return status;
}

Status polynomial_diff(const polynomial_t *p, polynomial_t *out)
{
    if (p == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    polynomial_t result = {0};
    polynomial_term_t **tail = &result.head;
    for (const polynomial_term_t *term = p->head; term != NULL && term->degree > 0;
         term = term->next) {
        intmax_t value;
        Status status;
        if (term->degree <= (uintmax_t)INTMAX_MAX) {
            status = mult_values(term->coefficient, (intmax_t)term->degree, &value);
        } else if (term->coefficient == -1 && term->degree == (uintmax_t)INTMAX_MAX + 1 &&
                   INTMAX_MIN < -INTMAX_MAX) {
            value = INTMAX_MIN;
            status = ST_OK;
        } else {
            status = ST_OVERFLOW;
        }
        if (status == ST_OK) {
            status = push_term(&tail, value, term->degree - 1);
        }
        if (status != ST_OK) {
            return save_result(out, &result, status);
        }
    }
    return save_result(out, &result, ST_OK);
}

static Status mult_power(polynomial_t *p, const polynomial_t *q, uintmax_t degree)
{
    if (degree == 0 || p->head == NULL) {
        return ST_OK;
    }

    polynomial_t power = {0};
    Status status = polynomial_copy(&power, q);
    while (degree > 0 && status == ST_OK) {
        if (degree % 2 != 0) {
            status = polynomial_mult(p, &power, p);
        }
        degree /= 2;
        if (degree > 0 && status == ST_OK) {
            status = polynomial_mult(&power, &power, &power);
        }
    }
    polynomial_clear(&power);
    return status;
}

Status polynomial_cmps(const polynomial_t *p, const polynomial_t *q, polynomial_t *out)
{
    if (p == NULL || q == NULL || out == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    polynomial_t result = {0};
    Status status = ST_OK;
    uintmax_t degree = p->head == NULL ? 0 : p->head->degree;
    for (const polynomial_term_t *term = p->head; term != NULL && status == ST_OK;
         term = term->next) {
        status = mult_power(&result, q, degree - term->degree);
        if (status == ST_OK) {
            status = polynomial_add_term(&result, term->coefficient, 0);
        }
        degree = term->degree;
    }
    if (status == ST_OK) {
        status = mult_power(&result, q, degree);
    }
    return save_result(out, &result, status);
}
