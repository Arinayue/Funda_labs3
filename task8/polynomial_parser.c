#include "polynomial_parser.h"

#include <ctype.h>
#include <stdint.h>

static const char *skip_spaces(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static Status parse_unsigned(const char **text, uintmax_t limit,
                             uintmax_t *value)
{
    const char *position = *text;
    uintmax_t parsed = 0;

    if (!isdigit((unsigned char)*position)) {
        return ST_INVALID_ARGUMENT;
    }
    while (isdigit((unsigned char)*position)) {
        uintmax_t digit = (uintmax_t)(*position - '0');
        if (parsed > (limit - digit) / 10) {
            return ST_OVERFLOW;
        }
        parsed = parsed * 10 + digit;
        ++position;
    }
    *text = position;
    *value = parsed;
    return ST_OK;
}

Status polynomial_parse_expression(const char *text, polynomial_t *polynomial)
{
    polynomial_t parsed = {0};
    const char *position;
    int first_term = 1;
    Status status = ST_OK;

    if (text == NULL || polynomial == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    position = skip_spaces(text);
    while (*position != '\0' && status == ST_OK) {
        int negative = 0;
        int has_coefficient = 0;
        uintmax_t magnitude = 0;
        uintmax_t degree = 0;

        if (*position == '+' || *position == '-') {
            negative = *position == '-';
            ++position;
            position = skip_spaces(position);
        } else if (!first_term) {
            status = ST_INVALID_ARGUMENT;
            break;
        }
        if (*position == '\0' || *position == '+' || *position == '-') {
            status = ST_INVALID_ARGUMENT;
            break;
        }

        if (isdigit((unsigned char)*position)) {
            uintmax_t limit = (uintmax_t)INTMAX_MAX + (negative ? 1u : 0u);
            has_coefficient = 1;
            status = parse_unsigned(&position, limit, &magnitude);
            if (status != ST_OK) {
                break;
            }
        }

        position = skip_spaces(position);
        if (*position == 'x' || *position == 'X') {
            ++position;
            degree = 1;
            position = skip_spaces(position);
            if (*position == '^') {
                ++position;
                position = skip_spaces(position);
                status = parse_unsigned(&position, UINTMAX_MAX, &degree);
                if (status != ST_OK) {
                    break;
                }
            }
            if (!has_coefficient) {
                magnitude = 1;
            }
        } else if (!has_coefficient) {
            status = ST_INVALID_ARGUMENT;
            break;
        }

        intmax_t coefficient;
        uintmax_t negative_limit = (uintmax_t)INTMAX_MAX + 1u;
        if (negative) {
            coefficient = magnitude == negative_limit
                              ? INTMAX_MIN
                              : -(intmax_t)magnitude;
        } else {
            coefficient = (intmax_t)magnitude;
        }
        status = polynomial_add_term(&parsed, coefficient, degree);
        if (status != ST_OK) {
            break;
        }

        first_term = 0;
        position = skip_spaces(position);
        if (*position != '\0' && *position != '+' && *position != '-') {
            status = ST_INVALID_ARGUMENT;
        }
    }

    if (status == ST_OK && first_term) {
        status = ST_INVALID_ARGUMENT;
    }
    if (status == ST_OK) {
        polynomial_clear(polynomial);
        *polynomial = parsed;
    } else {
        polynomial_clear(&parsed);
    }
    return status;
}
