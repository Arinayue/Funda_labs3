#include "io.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE " \t\r\n\v\f"

static Status grow_line(char **buffer, size_t *capacity)
{
    if (*capacity == SIZE_MAX)
        return ST_OVERFLOW;

    size_t next = *capacity == 0 ? 64 :
        (*capacity > SIZE_MAX / 2 ? SIZE_MAX : *capacity * 2);
    char *expanded = realloc(*buffer, next);
    if (expanded == NULL)
        return ST_NO_MEMORY;

    *buffer = expanded;
    *capacity = next;
    return ST_OK;
}

Status read_line(FILE *stream, char **line)
{
    if (stream == NULL || line == NULL)
        return ST_INVALID_ARGUMENT;

    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    Status status = ST_OK;
    int character;

    while ((character = fgetc(stream)) != EOF && character != '\n') {
        if (character == '\0')
            status = ST_INVALID_FORMAT;
        if (status == ST_INVALID_FORMAT)
            continue;

        if (capacity - length < 2) {
            status = grow_line(&buffer, &capacity);
            if (status != ST_OK)
                break;
        }
        ((unsigned char *)buffer)[length++] = (unsigned char)character;
    }

    if (character == EOF && ferror(stream))
        status = ST_FILE_READ;
    else if (character == EOF && length == 0 && status == ST_OK)
        status = ST_END_OF_INPUT;

    if (status == ST_OK && buffer == NULL)
        status = grow_line(&buffer, &capacity);
    if (status != ST_OK) {
        free(buffer);
        return status;
    }

    buffer[length] = '\0';
    *line = buffer;
    return ST_OK;
}

static Status parse_unsigned(const char *text, size_t length, uintmax_t limit,
                               uintmax_t *value)
{
    if (length == 0 || strspn(text, "0123456789") < length)
        return ST_INVALID_FORMAT;

    uintmax_t result = 0;
    for (size_t index = 0; index < length; ++index) {
        unsigned int digit = (unsigned int)(text[index] - '0');
        if (result > limit / 10 || (result == limit / 10 && digit > limit % 10))
            return ST_OVERFLOW;
        result = result * 10 + digit;
    }

    *value = result;
    return ST_OK;
}

static Status parse_single_field(const char *text, size_t *offset, size_t *length)
{
    size_t start = strspn(text, WHITESPACE);
    size_t field_length = strcspn(text + start, WHITESPACE);
    const char *end = text + start + field_length;
    if (field_length == 0 || end[strspn(end, WHITESPACE)] != '\0')
        return ST_INVALID_FORMAT;

    *offset = start;
    *length = field_length;
    return ST_OK;
}

Status parse_id(const char *text, uintmax_t *id)
{
    if (text == NULL || id == NULL)
        return ST_INVALID_ARGUMENT;

    size_t offset;
    size_t length;
    Status status = parse_single_field(text, &offset, &length);
    if (status != ST_OK)
        return status;

    return parse_unsigned(text + offset, length, UINTMAX_MAX, id);
}

Status parse_text_key(char *line, const char **key)
{
    if (line == NULL || key == NULL)
        return ST_INVALID_ARGUMENT;

    size_t offset;
    size_t length;
    Status status = parse_single_field(line, &offset, &length);
    if (status != ST_OK)
        return status;

    line[offset + length] = '\0';
    *key = line + offset;
    return ST_OK;
}

static char *copy_field(const char *text, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static Status parse_student(const char *line, Student *student)
{
    enum {
        ID_FIELD,
        FIRST_NAME_FIELD,
        LAST_NAME_FIELD,
        GROUP_FIELD,
        FIRST_GRADE_FIELD,
        FIELD_COUNT = FIRST_GRADE_FIELD + EXAM_COUNT
    };
    const char *fields[FIELD_COUNT];
    size_t lengths[FIELD_COUNT];

    for (size_t index = 0; index < FIELD_COUNT; ++index) {
        line += strspn(line, WHITESPACE);
        fields[index] = line;
        lengths[index] = strcspn(line, WHITESPACE);
        if (lengths[index] == 0)
            return ST_INVALID_FORMAT;
        line += lengths[index];
    }
    if (line[strspn(line, WHITESPACE)] != '\0')
        return ST_INVALID_FORMAT;

    const char *latin = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    if (strspn(fields[FIRST_NAME_FIELD], latin) != lengths[FIRST_NAME_FIELD] ||
        strspn(fields[LAST_NAME_FIELD], latin) != lengths[LAST_NAME_FIELD])
        return ST_INVALID_FORMAT;

    Student parsed = {0};
    Status status = parse_unsigned(fields[ID_FIELD], lengths[ID_FIELD],
                                     UINTMAX_MAX, &parsed.id);
    if (status != ST_OK)
        return status;

    unsigned char grades[EXAM_COUNT];
    for (size_t index = 0; index < EXAM_COUNT; ++index) {
        size_t field = FIRST_GRADE_FIELD + index;
        uintmax_t grade;
        status = parse_unsigned(fields[field], lengths[field], UCHAR_MAX, &grade);
        if (status != ST_OK)
            return status;
        grades[index] = (unsigned char)grade;
    }

    parsed.first_name = copy_field(fields[FIRST_NAME_FIELD], lengths[FIRST_NAME_FIELD]);
    parsed.last_name = copy_field(fields[LAST_NAME_FIELD], lengths[LAST_NAME_FIELD]);
    parsed.group = copy_field(fields[GROUP_FIELD], lengths[GROUP_FIELD]);
    parsed.grades = malloc(sizeof(grades));
    if (parsed.first_name == NULL || parsed.last_name == NULL ||
        parsed.group == NULL || parsed.grades == NULL) {
        student_destroy(&parsed);
        return ST_NO_MEMORY;
    }

    memcpy(parsed.grades, grades, sizeof(grades));
    *student = parsed;
    return ST_OK;
}

static Status read_students(FILE *stream, StudentArray *students, size_t *error_line)
{
    size_t line_number = 0;
    for (;;) {
        char *line = NULL;
        Status status = read_line(stream, &line);
        if (status == ST_END_OF_INPUT)
            return ST_OK;
        if (line_number == SIZE_MAX) {
            free(line);
            return ST_OVERFLOW;
        }
        ++line_number;
        if (status != ST_OK) {
            *error_line = line_number;
            return status;
        }

        if (line[strspn(line, WHITESPACE)] == '\0') {
            free(line);
            continue;
        }

        Student student = {0};
        status = parse_student(line, &student);
        free(line);
        if (status == ST_OK)
            status = student_array_append(students, &student);
        student_destroy(&student);
        if (status != ST_OK) {
            *error_line = line_number;
            return status;
        }
    }
}

Status load_students(const char *path, StudentArray *students, size_t *error_line)
{
    if (path == NULL || *path == '\0' || students == NULL || error_line == NULL)
        return ST_INVALID_ARGUMENT;

    *error_line = 0;
    FILE *stream = fopen(path, "rb");
    if (stream == NULL)
        return ST_FILE_OPEN;

    StudentArray loaded = {0};
    Status status = read_students(stream, &loaded, error_line);
    if (fclose(stream) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;

    if (status != ST_OK) {
        student_array_destroy(&loaded);
        return status;
    }

    student_array_destroy(students);
    *students = loaded;
    return ST_OK;
}

static Status print_student(FILE *stream, const Student *student)
{
    if (student == NULL || student->first_name == NULL || student->last_name == NULL ||
        student->group == NULL || student->grades == NULL)
        return ST_INVALID_ARGUMENT;

    if (fprintf(stream, "%" PRIuMAX " %s %s %s", student->id, student->first_name,
                student->last_name, student->group) < 0)
        return ST_FILE_WRITE;
    for (size_t index = 0; index < EXAM_COUNT; ++index) {
        if (fprintf(stream, " %u", (unsigned int)student->grades[index]) < 0)
            return ST_FILE_WRITE;
    }

    return fputc('\n', stream) == EOF ? ST_FILE_WRITE : ST_OK;
}

Status print_students(FILE *stream, const StudentArray *students)
{
    if (stream == NULL || students == NULL || students->size > students->capacity ||
        (students->size != 0 && students->items == NULL))
        return ST_INVALID_ARGUMENT;

    if (students->size == 0 && fputs("Список студентов пуст.\n", stream) == EOF)
        return ST_FILE_WRITE;

    for (size_t index = 0; index < students->size; ++index) {
        Status status = print_student(stream, &students->items[index]);
        if (status != ST_OK)
            return status;
    }
    return fflush(stream) == EOF ? ST_FILE_WRITE : ST_OK;
}

static bool matches_are_valid(const StudentMatches *matches)
{
    return matches != NULL && matches->size <= matches->capacity &&
           (matches->size == 0 || matches->items != NULL);
}

Status print_matches(FILE *stream, const StudentMatches *matches)
{
    if (stream == NULL || !matches_are_valid(matches))
        return ST_INVALID_ARGUMENT;

    if (matches->size == 0 && fputs("Совпадений нет.\n", stream) == EOF)
        return ST_FILE_WRITE;

    for (size_t index = 0; index < matches->size; ++index) {
        Status status = print_student(stream, matches->items[index]);
        if (status != ST_OK)
            return status;
    }
    return fflush(stream) == EOF ? ST_FILE_WRITE : ST_OK;
}

static Status write_trace_student(FILE *stream, const Student *student,
                                    bool include_details)
{
    if (student == NULL || student->last_name == NULL || student->first_name == NULL)
        return ST_INVALID_ARGUMENT;

    if (!include_details) {
        int written = fprintf(stream, "%s %s\n", student->last_name, student->first_name);
        return written < 0 ? ST_FILE_WRITE : ST_OK;
    }

    if (student->group == NULL)
        return ST_INVALID_ARGUMENT;

    double average;
    Status status = student_average(student, &average);
    if (status != ST_OK)
        return status;

    int written = fprintf(stream, "%s %s\t%s\t%.2f\n", student->last_name,
                          student->first_name, student->group, average);
    return written < 0 ? ST_FILE_WRITE : ST_OK;
}

Status append_trace_report(const char *path, const StudentMatches *matches,
                             bool include_details)
{
    if (path == NULL || *path == '\0' || !matches_are_valid(matches))
        return ST_INVALID_ARGUMENT;
    if (matches->size == 0)
        return ST_OK;

    FILE *stream = fopen(path, "a");
    if (stream == NULL)
        return ST_FILE_OPEN;

    Status status = ST_OK;
    for (size_t index = 0; index < matches->size; ++index) {
        status = write_trace_student(stream, matches->items[index], include_details);
        if (status != ST_OK)
            break;
    }

    if (status == ST_OK && fflush(stream) == EOF)
        status = ST_FILE_WRITE;
    if (fclose(stream) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    return status;
}
