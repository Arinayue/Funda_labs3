#include "io.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE " \t\r\n\v\f"
#define DIGITS "0123456789"
#define LIVER_FORMAT "%s;%s;%s;%02u.%02u.%04u;%c;%.*g\n"

enum {
    RECORD_LAST_NAME,
    RECORD_FIRST_NAME,
    RECORD_PATRONYMIC,
    RECORD_BIRTH_DATE,
    RECORD_SEX,
    RECORD_MONTHLY_INCOME,
    RECORD_FIELD_COUNT
};

static Status grow_line(char **buffer, size_t *capacity)
{
    if (*capacity == SIZE_MAX) {
        return ST_OVERFLOW;
    }

    size_t next = 64;
    if (*capacity != 0) {
        next = *capacity > SIZE_MAX / 2 ? SIZE_MAX : *capacity * 2;
    }

    char *expanded = realloc(*buffer, next);
    if (expanded == NULL) {
        return ST_NO_MEMORY;
    }

    *buffer = expanded;
    *capacity = next;
    return ST_OK;
}

static Status read_line_from(FILE *stream, char **line)
{
    if (line == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    Status status = ST_OK;
    int character;

    for (;;) {
        character = stream == NULL ? getchar() : fgetc(stream);
        if (character == EOF || character == '\n') {
            break;
        }
        if (character == '\0') {
            status = ST_INVALID_INPUT;
        }
        if (status == ST_INVALID_INPUT) {
            continue;
        }
        if (capacity - length < 2) {
            status = grow_line(&buffer, &capacity);
            if (status != ST_OK) {
                break;
            }
        }

        ((unsigned char *)buffer)[length++] = (unsigned char)character;
    }

    if (stream != NULL && character == EOF && ferror(stream)) {
        status = ST_FILE_READ;
    } else if (character == EOF && length == 0 && status == ST_OK) {
        status = ST_END_OF_INPUT;
    }
    if (status == ST_OK && buffer == NULL) {
        status = grow_line(&buffer, &capacity);
    }
    if (status != ST_OK) {
        free(buffer);
        return status;
    }

    buffer[length] = '\0';
    *line = buffer;
    return ST_OK;
}

Status read_line(FILE *stream, char **line)
{
    if (stream == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    return read_line_from(stream, line);
}

Status read_console_line(char **line)
{
    return read_line_from(NULL, line);
}

static Status parse_unsigned(const char **text, uintmax_t limit, uintmax_t *value)
{
    const char *cursor = *text;
    size_t length = strspn(cursor, DIGITS);
    if (length == 0) {
        return ST_INVALID_INPUT;
    }

    uintmax_t result = 0;
    for (size_t index = 0; index < length; ++index) {
        unsigned int digit = (unsigned int)(cursor[index] - '0');
        if (result > limit / 10 || (result == limit / 10 && digit > limit % 10)) {
            return ST_OVERFLOW;
        }
        result = result * 10 + digit;
    }

    *text = cursor + length;
    *value = result;
    return ST_OK;
}

Status parse_size(const char *text, size_t *value)
{
    if (text == NULL || value == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    text += strspn(text, WHITESPACE);
    uintmax_t number;
    Status status = parse_unsigned(&text, SIZE_MAX, &number);
    if (status != ST_OK) {
        return status;
    }
    if (text[strspn(text, WHITESPACE)] != '\0') {
        return ST_INVALID_INPUT;
    }

    *value = (size_t)number;
    return ST_OK;
}

static Status parse_birth_date(const char *text, BirthDate *date)
{
    uintmax_t parts[3];

    for (size_t index = 0; index < 3; ++index) {
        Status status = parse_unsigned(&text, UINT_MAX, &parts[index]);
        if (status != ST_OK) {
            return status;
        }
        if (index < 2) {
            if (*text != '.') {
                return ST_INVALID_INPUT;
            }
            ++text;
        }
    }
    if (*text != '\0') {
        return ST_INVALID_INPUT;
    }

    *date = (BirthDate){(unsigned int)parts[0], (unsigned int)parts[1],
                        (unsigned int)parts[2]};
    return ST_OK;
}

static Status parse_income(const char *text, double *income)
{
    const char *cursor = text;

    if (*cursor == '+' || *cursor == '-') {
        ++cursor;
    }
    const char *significand = cursor;
    size_t whole_digits = strspn(cursor, DIGITS);
    size_t fraction_digits = 0;
    cursor += whole_digits;
    if (*cursor == '.') {
        ++cursor;
        fraction_digits = strspn(cursor, DIGITS);
        cursor += fraction_digits;
    }
    if (whole_digits == 0 && fraction_digits == 0) {
        return ST_INVALID_INPUT;
    }

    const char *significand_end = cursor;
    if (*cursor == 'e' || *cursor == 'E') {
        ++cursor;
        if (*cursor == '+' || *cursor == '-') {
            ++cursor;
        }
        size_t digits = strspn(cursor, DIGITS);
        if (digits == 0) {
            return ST_INVALID_INPUT;
        }
        cursor += digits;
    }
    if (*cursor != '\0') {
        return ST_INVALID_INPUT;
    }

    const char *nonzero_digit = strpbrk(significand, "123456789");
    if (nonzero_digit == NULL || nonzero_digit >= significand_end) {
        *income = *text == '-' ? -0.0 : 0.0;
        return ST_OK;
    }
    if (*text == '-') {
        return ST_INVALID_INPUT;
    }

    char *end;
    double converted = strtod(text, &end);
    if (end != cursor) {
        return ST_INVALID_INPUT;
    }
    if (!isfinite(converted) || converted == 0.0) {
        return ST_OVERFLOW;
    }

    *income = converted;
    return ST_OK;
}

static Status split_fields(char *line, char *fields[RECORD_FIELD_COUNT])
{
    for (size_t index = 0; index < RECORD_FIELD_COUNT; ++index) {
        char *separator = strchr(line, ';');
        if ((separator == NULL) != (index == RECORD_FIELD_COUNT - 1)) {
            return ST_INVALID_INPUT;
        }
        if (separator != NULL) {
            *separator = '\0';
        }

        line += strspn(line, WHITESPACE);
        size_t length = strlen(line);
        while (length > 0 && strchr(WHITESPACE, line[length - 1]) != NULL) {
            --length;
        }
        line[length] = '\0';
        fields[index] = line;
        if (separator != NULL) {
            line = separator + 1;
        }
    }

    return ST_OK;
}

static char *copy_text(const char *text)
{
    if (text == NULL) {
        return NULL;
    }

    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

static Status parse_liver_fields(char *line, bool allow_wildcards, Liver *value,
                                   unsigned int *fields)
{
    if (line == NULL || value == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    char *tokens[RECORD_FIELD_COUNT];
    Status status = split_fields(line, tokens);
    if (status != ST_OK) {
        return status;
    }

    const unsigned int field_bits[RECORD_FIELD_COUNT] = {
        [RECORD_LAST_NAME] = LIVER_FIELD_LAST_NAME,
        [RECORD_FIRST_NAME] = LIVER_FIELD_FIRST_NAME,
        [RECORD_PATRONYMIC] = LIVER_FIELD_PATRONYMIC,
        [RECORD_BIRTH_DATE] = LIVER_FIELD_BIRTH_DATE,
        [RECORD_SEX] = LIVER_FIELD_SEX,
        [RECORD_MONTHLY_INCOME] = LIVER_FIELD_MONTHLY_INCOME
    };
    unsigned int selected = LIVER_FIELD_ALL;
    if (allow_wildcards) {
        for (size_t index = 0; index < RECORD_FIELD_COUNT; ++index) {
            if (strcmp(tokens[index], "*") == 0) {
                tokens[index] = NULL;
                selected &= ~field_bits[index];
            }
        }
    }
    if (selected == 0) {
        return ST_INVALID_INPUT;
    }

    Liver parsed = {
        .last_name = tokens[RECORD_LAST_NAME],
        .first_name = tokens[RECORD_FIRST_NAME],
        .patronymic = tokens[RECORD_PATRONYMIC]
    };
    if (tokens[RECORD_SEX] != NULL) {
        if (strlen(tokens[RECORD_SEX]) != 1) {
            return ST_INVALID_INPUT;
        }
        parsed.sex = tokens[RECORD_SEX][0];
    }
    if (tokens[RECORD_BIRTH_DATE] != NULL) {
        status = parse_birth_date(tokens[RECORD_BIRTH_DATE], &parsed.birth_date);
        if (status != ST_OK) {
            return status;
        }
    }
    if (tokens[RECORD_MONTHLY_INCOME] != NULL) {
        status = parse_income(tokens[RECORD_MONTHLY_INCOME], &parsed.monthly_income);
        if (status != ST_OK) {
            return status;
        }
    }
    status = liver_validate(&parsed, selected);
    if (status != ST_OK) {
        return status;
    }

    parsed.last_name = copy_text(parsed.last_name);
    parsed.first_name = copy_text(parsed.first_name);
    parsed.patronymic = copy_text(parsed.patronymic);
    if (((selected & LIVER_FIELD_LAST_NAME) && parsed.last_name == NULL) ||
        ((selected & LIVER_FIELD_FIRST_NAME) && parsed.first_name == NULL) ||
        ((selected & LIVER_FIELD_PATRONYMIC) && parsed.patronymic == NULL)) {
        liver_destroy(&parsed);
        return ST_NO_MEMORY;
    }

    *value = parsed;
    *fields = selected;
    return ST_OK;
}

Status parse_liver(char *line, Liver *value)
{
    unsigned int fields;
    return parse_liver_fields(line, false, value, &fields);
}

Status parse_filter(char *line, LiverFilter *filter)
{
    if (filter == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    return parse_liver_fields(line, true, &filter->values, &filter->fields);
}

static Status read_livers(FILE *stream, LiverList *list, size_t *error_line)
{
    size_t line_number = 0;

    for (;;) {
        char *line = NULL;
        Status status = read_line(stream, &line);
        if (status == ST_END_OF_INPUT) {
            return ST_OK;
        }
        if (line_number == SIZE_MAX) {
            free(line);
            return ST_OVERFLOW;
        }
        ++line_number;

        Liver value = {0};
        if (status == ST_OK && line[strspn(line, WHITESPACE)] != '\0') {
            status = parse_liver(line, &value);
            if (status == ST_OK) {
                status = liver_insert_loaded(list, &value);
            }
        }
        free(line);
        liver_destroy(&value);
        if (status != ST_OK) {
            *error_line = line_number;
            return status;
        }
    }
}

Status load_livers(const char *path, LiverList *list, size_t *error_line)
{
    if (path == NULL || list == NULL || error_line == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *error_line = 0;
    if (*path == '\0') {
        return ST_INVALID_INPUT;
    }

    FILE *stream = fopen(path, "r");
    if (stream == NULL) {
        return ST_FILE_OPEN;
    }

    LiverList loaded = {0};
    Status status = read_livers(stream, &loaded, error_line);
    if (fclose(stream) != 0 && status == ST_OK) {
        status = ST_FILE_CLOSE;
    }
    if (status != ST_OK) {
        liver_list_destroy(&loaded);
        return status;
    }

    liver_list_destroy(list);
    *list = loaded;
    return ST_OK;
}

static Status write_liver(FILE *stream, const Liver *value)
{
    Status status = liver_validate(value, LIVER_FIELD_ALL);
    if (status != ST_OK) {
        return status;
    }

    int written;
    if (stream == NULL) {
        written = printf(LIVER_FORMAT, value->last_name, value->first_name, value->patronymic,
                         value->birth_date.day, value->birth_date.month, value->birth_date.year,
                         value->sex, DECIMAL_DIG, value->monthly_income);
    } else {
        written = fprintf(stream, LIVER_FORMAT, value->last_name, value->first_name, value->patronymic,
                          value->birth_date.day, value->birth_date.month, value->birth_date.year,
                          value->sex, DECIMAL_DIG, value->monthly_income);
    }
    return written < 0 ? ST_FILE_WRITE : ST_OK;
}

Status print_liver(FILE *stream, const Liver *value)
{
    if (stream == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    return write_liver(stream, value);
}

Status print_liver_to_console(const Liver *value)
{
    return write_liver(NULL, value);
}

Status save_livers(const char *path, const LiverList *list)
{
    if (path == NULL || list == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    if (*path == '\0') {
        return ST_INVALID_INPUT;
    }

    FILE *stream = fopen(path, "w");
    if (stream == NULL) {
        return ST_FILE_OPEN;
    }

    Status status = ST_OK;
    for (const LiverNode *node = list->head; node != NULL && status == ST_OK;
         node = node->next) {
        status = print_liver(stream, &node->value);
    }
    if (status == ST_OK && fflush(stream) != 0) {
        status = ST_FILE_WRITE;
    }
    if (fclose(stream) != 0 && status == ST_OK) {
        status = ST_FILE_CLOSE;
    }

    return status;
}
