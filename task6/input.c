#include "input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HORIZONTAL_SPACE " \t"
#define DECIMAL_DIGITS "0123456789"

static Status reserve_line(char **buffer, size_t *capacity, size_t needed)
{
    if (buffer == NULL || capacity == NULL || needed == 0 ||
        ((*buffer == NULL) != (*capacity == 0))) {
        return ST_INVALID_ARGUMENT;
    }
    if (*capacity >= needed) {
        return ST_OK;
    }

    size_t next_capacity = 64;
    if (*capacity != 0) {
        next_capacity = *capacity > SIZE_MAX / 2 ? SIZE_MAX : *capacity * 2;
    }
    if (next_capacity < needed) {
        next_capacity = needed;
    }

    char *expanded = realloc(*buffer, next_capacity);
    if (expanded == NULL) {
        return ST_NO_MEMORY;
    }

    *buffer = expanded;
    *capacity = next_capacity;
    return ST_OK;
}

static Status read_file_line(FILE *file, char **buffer, size_t *capacity)
{
    if (file == NULL || buffer == NULL || capacity == NULL ||
        ((*buffer == NULL) != (*capacity == 0))) {
        return ST_INVALID_ARGUMENT;
    }

    size_t length = 0;
    int character;
    while ((character = fgetc(file)) != EOF && character != '\n') {
        if (character == '\0') {
            return ST_INVALID_FORMAT;
        }
        if (length > SIZE_MAX - 2) {
            return ST_RANGE_ERROR;
        }

        Status status = reserve_line(buffer, capacity, length + 2);
        if (status != ST_OK) {
            return status;
        }
        ((unsigned char *)*buffer)[length] = (unsigned char)character;
        ++length;
    }

    if (character == EOF) {
        if (ferror(file)) {
            return ST_FILE_READ;
        }
        if (length == 0) {
            return ST_END_OF_INPUT;
        }
    }
    if (character == '\n' && length > 0 && (*buffer)[length - 1] == '\r') {
        --length;
    }

    Status status = reserve_line(buffer, capacity, length + 1);
    if (status != ST_OK) {
        return status;
    }
    (*buffer)[length] = '\0';
    return ST_OK;
}

static Status parse_coordinate(const char **text, double *value)
{
    if (text == NULL || *text == NULL || value == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    const char *start = *text;
    const char *cursor = start;
    if (*cursor == '+' || *cursor == '-') {
        ++cursor;
    }
    size_t whole_length = strspn(cursor, DECIMAL_DIGITS);
    cursor += whole_length;
    size_t fractional_length = 0;
    if (*cursor == '.') {
        ++cursor;
        fractional_length = strspn(cursor, DECIMAL_DIGITS);
        cursor += fractional_length;
    }
    if (whole_length == 0 && fractional_length == 0) {
        return ST_INVALID_FORMAT;
    }

    const char *mantissa_end = cursor;
    if (*cursor == 'e' || *cursor == 'E') {
        ++cursor;
        if (*cursor == '+' || *cursor == '-') {
            ++cursor;
        }
        size_t exponent_length = strspn(cursor, DECIMAL_DIGITS);
        if (exponent_length == 0) {
            return ST_INVALID_FORMAT;
        }
        cursor += exponent_length;
    }
    if (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
        return ST_INVALID_FORMAT;
    }

    char *end;
    double parsed = strtod(start, &end);
    if (end != cursor) {
        return ST_INVALID_FORMAT;
    }
    if (!isfinite(parsed) || fabs(parsed) == HUGE_VAL ||
        (fpclassify(parsed) == FP_ZERO && start + strspn(start, "+-0.") < mantissa_end)) {
        return ST_RANGE_ERROR;
    }

    *text = cursor;
    *value = parsed;
    return ST_OK;
}

static Status parse_point(const char *line, point_t *point)
{
    if (line == NULL || point == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    const char *cursor = line + strspn(line, HORIZONTAL_SPACE);
    point_t parsed;
    Status status = parse_coordinate(&cursor, &parsed.x);
    if (status != ST_OK) {
        return status;
    }
    if (*cursor == '\0') {
        return ST_INVALID_FORMAT;
    }

    cursor += strspn(cursor, HORIZONTAL_SPACE);
    status = parse_coordinate(&cursor, &parsed.y);
    if (status != ST_OK) {
        return status;
    }
    if (cursor[strspn(cursor, HORIZONTAL_SPACE)] != '\0') {
        return ST_INVALID_FORMAT;
    }

    *point = parsed;
    return ST_OK;
}

static Status parse_timestamp(const char *text, size_t length, uintmax_t *seconds)
{
    if (text == NULL || seconds == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    const char format[] = "00.00.0000 00:00:00";
    if (length != sizeof(format) - 1) {
        return ST_INVALID_FORMAT;
    }

    unsigned int fields[6] = {0};
    size_t field = 0;
    for (size_t index = 0; index < length; ++index) {
        if (format[index] == '0') {
            if (text[index] < '0' || text[index] > '9') {
                return ST_INVALID_FORMAT;
            }
            fields[field] = fields[field] * 10 + (unsigned int)(text[index] - '0');
        } else if (text[index] != format[index]) {
            return ST_INVALID_FORMAT;
        } else {
            ++field;
        }
    }

    unsigned int day = fields[0];
    unsigned int month = fields[1];
    unsigned int year = fields[2];
    unsigned int hour = fields[3];
    unsigned int minute = fields[4];
    unsigned int second = fields[5];
    if (year == 0 || month == 0 || month > 12 || day == 0 ||
        hour > 23 || minute > 59 || second > 59) {
        return ST_INVALID_FORMAT;
    }

    unsigned int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
        month_days[1] = 29;
    }
    if (day > month_days[month - 1]) {
        return ST_INVALID_FORMAT;
    }

    uintmax_t previous_years = year - 1;
    uintmax_t days = previous_years * 365 + previous_years / 4 -
                     previous_years / 100 + previous_years / 400 + day - 1;
    for (unsigned int index = 0; index < month - 1; ++index) {
        days += month_days[index];
    }

    *seconds = ((days * 24 + hour) * 60 + minute) * 60 + second;
    return ST_OK;
}

static Status parse_stop_line(const char *line, const point_t *point, const char **id,
                                size_t *id_length, stop_data_t *stop)
{
    if (line == NULL || point == NULL || id == NULL || id_length == NULL || stop == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    const char *fields[4] = {line};
    size_t lengths[3];
    for (size_t index = 0; index < 3; ++index) {
        lengths[index] = strcspn(fields[index], ";");
        if (lengths[index] == 0 || fields[index][lengths[index]] != ';') {
            return ST_INVALID_FORMAT;
        }
        fields[index + 1] = fields[index] + lengths[index] + 1;
    }
    if (fields[3][0] == '\0' || fields[3][1] != '\0') {
        return ST_INVALID_FORMAT;
    }

    stop_data_t parsed;
    parsed.point = *point;
    switch (fields[3][0]) {
        case 'S':
            parsed.kind = STOP_START;
            break;
        case 'I':
            parsed.kind = STOP_INTERMEDIATE;
            break;
        case 'E':
            parsed.kind = STOP_END;
            break;
        default:
            return ST_INVALID_FORMAT;
    }

    Status status = parse_timestamp(fields[1], lengths[1], &parsed.arrival);
    if (status != ST_OK) {
        return status;
    }
    status = parse_timestamp(fields[2], lengths[2], &parsed.departure);
    if (status != ST_OK) {
        return status;
    }
    if (parsed.arrival > parsed.departure) {
        return ST_INVALID_FORMAT;
    }

    *id = fields[0];
    *id_length = lengths[0];
    *stop = parsed;
    return ST_OK;
}

Status load_stop_file(const char *path, transport_t **head, size_t *error_line)
{
    if (path == NULL || path[0] == '\0' || head == NULL || error_line == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *error_line = 0;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return ST_FILE_OPEN;
    }

    char *buffer = NULL;
    size_t capacity = 0;
    size_t line_number = 1;
    point_t point;
    Status status = read_file_line(file, &buffer, &capacity);
    if (status == ST_OK) {
        status = parse_point(buffer, &point);
    } else if (status == ST_END_OF_INPUT) {
        status = ST_INVALID_FORMAT;
    }

    while (status == ST_OK) {
        status = read_file_line(file, &buffer, &capacity);
        if (status == ST_END_OF_INPUT) {
            status = ST_OK;
            break;
        }
        if (line_number == SIZE_MAX) {
            line_number = 0;
            if (status == ST_OK) {
                status = ST_RANGE_ERROR;
            }
            break;
        }
        ++line_number;

        if (status == ST_OK) {
            const char *id;
            size_t id_length;
            stop_data_t stop;
            status = parse_stop_line(buffer, &point, &id, &id_length, &stop);
            if (status == ST_OK) {
                status = insert_stop(head, id, id_length, &stop);
            }
        }
    }

    if (status != ST_OK && status != ST_FILE_READ) {
        *error_line = line_number;
    }
    if (fclose(file) == EOF && status == ST_OK) {
        status = ST_FILE_CLOSE;
    }
    free(buffer);
    return status;
}
