#include "script.h"

#include "polynomial.h"
#include "polynomial_parser.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ScriptStatus from_polynomial_status(Status status)
{
    switch (status) {
    case ST_OK:
        return SCRIPT_OK;
    case ST_INVALID_ARGUMENT:
        return SCRIPT_INVALID_SYNTAX;
    case ST_OVERFLOW:
        return SCRIPT_OVERFLOW;
    case ST_NO_MEMORY:
        return SCRIPT_NO_MEMORY;
    case ST_OUTPUT_ERROR:
        return SCRIPT_OUTPUT_ERROR;
    case ST_DIV_ZERO:
        return SCRIPT_DIV_ZERO;
    }
    return SCRIPT_INVALID_SYNTAX;
}

static ScriptStatus read_script_file(const char *path, char **text)
{
    FILE *file;
    char *buffer;
    size_t capacity = 256;
    size_t length = 0;
    int character;
    ScriptStatus status = SCRIPT_OK;

    if (path == NULL || path[0] == '\0' || text == NULL) {
        return SCRIPT_INVALID_ARGUMENT;
    }
    *text = NULL;

    file = fopen(path, "rb");
    if (file == NULL) {
        return SCRIPT_FILE_OPEN;
    }

    buffer = malloc(capacity);
    if (buffer == NULL) {
        if (fclose(file) != 0) {
            return SCRIPT_FILE_READ;
        }
        return SCRIPT_NO_MEMORY;
    }

    while ((character = fgetc(file)) != EOF) {
        if (character == '\0') {
            status = SCRIPT_INVALID_SYNTAX;
            break;
        }
        if (length >= capacity - 1) {
            size_t new_capacity;
            char *expanded;

            if (capacity > SIZE_MAX / 2) {
                status = SCRIPT_OVERFLOW;
                break;
            }
            new_capacity = capacity * 2;
            expanded = realloc(buffer, new_capacity);
            if (expanded == NULL) {
                status = SCRIPT_NO_MEMORY;
                break;
            }
            buffer = expanded;
            capacity = new_capacity;
        }
        buffer[length++] = (char)character;
    }

    if (status == SCRIPT_OK && ferror(file)) {
        status = SCRIPT_FILE_READ;
    }
    if (fclose(file) != 0 && status == SCRIPT_OK) {
        status = SCRIPT_FILE_READ;
    }
    if (status != SCRIPT_OK) {
        free(buffer);
        return status;
    }

    buffer[length] = '\0';
    *text = buffer;
    return SCRIPT_OK;
}

static ScriptStatus remove_comments(const char *source, char **result)
{
    size_t length;
    size_t read_position = 0;
    size_t write_position = 0;
    int in_block = 0;
    char *clean;

    if (source == NULL || result == NULL) {
        return SCRIPT_INVALID_ARGUMENT;
    }
    *result = NULL;
    length = strlen(source);
    clean = malloc(length + 1);
    if (clean == NULL) {
        return SCRIPT_NO_MEMORY;
    }

    while (read_position < length) {
        char character = source[read_position];
        if (in_block) {
            if (character == '[') {
                free(clean);
                return SCRIPT_INVALID_SYNTAX;
            }
            if (character == ']') {
                in_block = 0;
            }
            clean[write_position++] = character == '\n' ? '\n' : ' ';
            ++read_position;
        } else if (character == '[') {
            in_block = 1;
            clean[write_position++] = ' ';
            ++read_position;
        } else if (character == ']') {
            free(clean);
            return SCRIPT_INVALID_SYNTAX;
        } else if (character == '%') {
            while (read_position < length && source[read_position] != '\n') {
                clean[write_position++] = ' ';
                ++read_position;
            }
        } else {
            clean[write_position++] = character;
            ++read_position;
        }
    }

    if (in_block) {
        free(clean);
        return SCRIPT_INVALID_SYNTAX;
    }
    clean[write_position] = '\0';
    *result = clean;
    return SCRIPT_OK;
}

static const char *skip_spaces(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static ScriptStatus copy_argument(const char *start, size_t length,
                                  char **argument)
{
    while (length > 0 && isspace((unsigned char)*start)) {
        ++start;
        --length;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        --length;
    }
    if (length == 0) {
        return SCRIPT_INVALID_SYNTAX;
    }

    *argument = malloc(length + 1);
    if (*argument == NULL) {
        return SCRIPT_NO_MEMORY;
    }
    memcpy(*argument, start, length);
    (*argument)[length] = '\0';
    return SCRIPT_OK;
}

static ScriptStatus parse_arguments(const char *start, const char *end,
                                    char **first, char **second, size_t *count)
{
    const char *comma = NULL;

    *first = NULL;
    *second = NULL;
    *count = 0;
    for (const char *position = start; position < end; ++position) {
        if (*position == ',') {
            if (comma != NULL) {
                return SCRIPT_INVALID_SYNTAX;
            }
            comma = position;
        }
    }

    if (comma == NULL) {
        if (skip_spaces(start) == end) {
            return SCRIPT_OK;
        }
        ScriptStatus status = copy_argument(start, (size_t)(end - start), first);
        if (status == SCRIPT_OK) {
            *count = 1;
        }
        return status;
    }

    ScriptStatus status = copy_argument(start, (size_t)(comma - start), first);
    if (status != SCRIPT_OK) {
        return status;
    }
    status = copy_argument(comma + 1, (size_t)(end - comma - 1), second);
    if (status != SCRIPT_OK) {
        free(*first);
        *first = NULL;
        return status;
    }
    *count = 2;
    return SCRIPT_OK;
}

static ScriptStatus store_and_print(polynomial_t *accumulator,
                                    const polynomial_t *value)
{
    Status status = polynomial_copy(accumulator, value);
    if (status != ST_OK) {
        return from_polynomial_status(status);
    }
    if (polynomial_print(stdout, accumulator) != ST_OK || putchar('\n') == EOF ||
        fflush(stdout) == EOF) {
        return SCRIPT_OUTPUT_ERROR;
    }
    return SCRIPT_OK;
}

static ScriptStatus execute_binary(const char *name, char *first, char *second,
                                   size_t count, polynomial_t *accumulator)
{
    polynomial_t left = {0};
    polynomial_t right = {0};
    polynomial_t result = {0};
    const polynomial_t *left_operand = accumulator;
    ScriptStatus status;

    if (count == 2) {
        status = from_polynomial_status(
            polynomial_parse_expression(first, &left));
        if (status == SCRIPT_OK) {
            left_operand = &left;
        }
    } else if (count == 1) {
        status = SCRIPT_OK;
    } else {
        status = SCRIPT_INVALID_SYNTAX;
    }
    if (status == SCRIPT_OK) {
        const char *right_text = count == 2 ? second : first;
        status = from_polynomial_status(
            polynomial_parse_expression(right_text, &right));
    }

    if (status == SCRIPT_OK) {
        Status operation_status;
        if (strcmp(name, "Add") == 0) {
            operation_status = polynomial_add(left_operand, &right, &result);
        } else if (strcmp(name, "Sub") == 0) {
            operation_status = polynomial_sub(left_operand, &right, &result);
        } else if (strcmp(name, "Mult") == 0) {
            operation_status = polynomial_mult(left_operand, &right, &result);
        } else if (strcmp(name, "Div") == 0) {
            operation_status = polynomial_div(left_operand, &right, &result);
        } else if (strcmp(name, "Mod") == 0) {
            operation_status = polynomial_mod(left_operand, &right, &result);
        } else {
            operation_status = polynomial_cmps(left_operand, &right, &result);
        }
        status = from_polynomial_status(operation_status);
    }
    if (status == SCRIPT_OK) {
        status = store_and_print(accumulator, &result);
    }

    polynomial_clear(&left);
    polynomial_clear(&right);
    polynomial_clear(&result);
    return status;
}

static ScriptStatus parse_integer(const char *text, intmax_t *value)
{
    uintmax_t magnitude = 0;
    uintmax_t limit;
    int negative = 0;
    const char *position = skip_spaces(text);

    if (*position == '+' || *position == '-') {
        negative = *position == '-';
        ++position;
    }
    if (!isdigit((unsigned char)*position)) {
        return SCRIPT_INVALID_SYNTAX;
    }

    limit = (uintmax_t)INTMAX_MAX + (negative ? 1u : 0u);
    while (isdigit((unsigned char)*position)) {
        uintmax_t digit = (uintmax_t)(*position - '0');
        if (magnitude > (limit - digit) / 10) {
            return SCRIPT_OVERFLOW;
        }
        magnitude = magnitude * 10 + digit;
        ++position;
    }
    if (*skip_spaces(position) != '\0') {
        return SCRIPT_INVALID_SYNTAX;
    }

    if (negative) {
        *value = magnitude == limit ? INTMAX_MIN : -(intmax_t)magnitude;
    } else {
        *value = (intmax_t)magnitude;
    }
    return SCRIPT_OK;
}

static ScriptStatus execute_eval(char *first, char *second, size_t count,
                                 polynomial_t *accumulator)
{
    polynomial_t explicit_polynomial = {0};
    const polynomial_t *polynomial = accumulator;
    const char *point_text;
    intmax_t point;
    intmax_t value;
    ScriptStatus status;

    if (count == 2) {
        status = from_polynomial_status(
            polynomial_parse_expression(first, &explicit_polynomial));
        if (status == SCRIPT_OK) {
            polynomial = &explicit_polynomial;
        }
        point_text = second;
    } else if (count == 1) {
        status = SCRIPT_OK;
        point_text = first;
    } else {
        status = SCRIPT_INVALID_SYNTAX;
        point_text = "";
    }
    if (status == SCRIPT_OK) {
        status = parse_integer(point_text, &point);
    }
    if (status == SCRIPT_OK) {
        status = from_polynomial_status(polynomial_eval(polynomial, point, &value));
    }
    if (status == SCRIPT_OK &&
        (printf("%" PRIdMAX "\n", value) < 0 || fflush(stdout) == EOF)) {
        status = SCRIPT_OUTPUT_ERROR;
    }

    polynomial_clear(&explicit_polynomial);
    return status;
}

static ScriptStatus execute_diff(char *first, size_t count,
                                 polynomial_t *accumulator)
{
    polynomial_t explicit_polynomial = {0};
    polynomial_t result = {0};
    const polynomial_t *polynomial = accumulator;
    ScriptStatus status = SCRIPT_OK;

    if (count == 1) {
        status = from_polynomial_status(
            polynomial_parse_expression(first, &explicit_polynomial));
        if (status == SCRIPT_OK) {
            polynomial = &explicit_polynomial;
        }
    } else if (count != 0) {
        status = SCRIPT_INVALID_SYNTAX;
    }
    if (status == SCRIPT_OK) {
        status = from_polynomial_status(polynomial_diff(polynomial, &result));
    }
    if (status == SCRIPT_OK) {
        status = store_and_print(accumulator, &result);
    }

    polynomial_clear(&explicit_polynomial);
    polynomial_clear(&result);
    return status;
}

static ScriptStatus execute_command(const char *name, char *first, char *second,
                                    size_t count, polynomial_t *accumulator)
{
    if (strcmp(name, "Add") == 0 || strcmp(name, "Sub") == 0 ||
        strcmp(name, "Mult") == 0 || strcmp(name, "Div") == 0 ||
        strcmp(name, "Mod") == 0 || strcmp(name, "Cmps") == 0) {
        return execute_binary(name, first, second, count, accumulator);
    }
    if (strcmp(name, "Eval") == 0) {
        return execute_eval(first, second, count, accumulator);
    }
    if (strcmp(name, "Diff") == 0) {
        return execute_diff(first, count, accumulator);
    }
    return SCRIPT_INVALID_SYNTAX;
}

static ScriptStatus execute_script(const char *script)
{
    char *clean = NULL;
    const char *position;
    polynomial_t accumulator = {0};
    ScriptStatus status = remove_comments(script, &clean);

    if (status != SCRIPT_OK) {
        return status;
    }

    position = clean;
    while (status == SCRIPT_OK) {
        char name[16];
        size_t name_length = 0;
        const char *arguments_start;
        const char *arguments_end;
        char *first = NULL;
        char *second = NULL;
        size_t argument_count = 0;

        position = skip_spaces(position);
        if (*position == '\0') {
            break;
        }
        while (isalpha((unsigned char)*position)) {
            if (name_length + 1 >= sizeof(name)) {
                status = SCRIPT_INVALID_SYNTAX;
                break;
            }
            name[name_length++] = *position++;
        }
        if (status != SCRIPT_OK || name_length == 0) {
            status = SCRIPT_INVALID_SYNTAX;
            break;
        }
        name[name_length] = '\0';
        position = skip_spaces(position);
        if (*position != '(') {
            status = SCRIPT_INVALID_SYNTAX;
            break;
        }
        arguments_start = ++position;
        while (*position != '\0' && *position != ')') {
            if (*position == '(') {
                status = SCRIPT_INVALID_SYNTAX;
                break;
            }
            ++position;
        }
        if (status != SCRIPT_OK || *position != ')') {
            status = SCRIPT_INVALID_SYNTAX;
            break;
        }
        arguments_end = position++;
        status = parse_arguments(arguments_start, arguments_end,
                                 &first, &second, &argument_count);
        position = skip_spaces(position);
        if (status == SCRIPT_OK && *position != ';') {
            status = SCRIPT_INVALID_SYNTAX;
        }
        if (status == SCRIPT_OK) {
            ++position;
            status = execute_command(name, first, second, argument_count,
                                     &accumulator);
        }
        free(first);
        free(second);
    }

    free(clean);
    polynomial_clear(&accumulator);
    return status;
}

ScriptStatus script_run_file(const char *path)
{
    char *script = NULL;
    ScriptStatus status = read_script_file(path, &script);

    if (status == SCRIPT_OK) {
        status = execute_script(script);
    }
    free(script);
    return status;
}
