#include "students.h"

#include <stdlib.h>
#include <string.h>

static Status validate_student_array(const StudentArray *students)
{
    if (students == NULL || students->size > students->capacity ||
        (students->items == NULL) != (students->capacity == 0))
        return ST_INVALID_ARGUMENT;
    if (students->capacity > SIZE_MAX / sizeof(*students->items))
        return ST_OVERFLOW;
    return ST_OK;
}

static size_t grow_capacity(size_t capacity, size_t limit)
{
    if (capacity == 0)
        return 1;
    if (capacity > limit / 2)
        return limit;
    return capacity * 2;
}

Status student_array_append(StudentArray *students, Student *new_student)
{
    if (new_student == NULL ||
        new_student->first_name == NULL || new_student->last_name == NULL ||
        new_student->group == NULL || new_student->grades == NULL)
        return ST_INVALID_ARGUMENT;

    Status status = validate_student_array(students);
    if (status != ST_OK)
        return status;

    if (students->size == students->capacity) {
        const size_t max_capacity = SIZE_MAX / sizeof(*students->items);
        if (students->capacity == max_capacity)
            return ST_OVERFLOW;

        size_t new_capacity = grow_capacity(students->capacity, max_capacity);
        Student *items = realloc(students->items, new_capacity * sizeof(*items));
        if (items == NULL)
            return ST_NO_MEMORY;

        students->items = items;
        students->capacity = new_capacity;
    }

    students->items[students->size] = *new_student;
    ++students->size;
    *new_student = (Student){0};
    return ST_OK;
}

void student_destroy(Student *student)
{
    if (student == NULL)
        return;

    free(student->first_name);
    free(student->last_name);
    free(student->group);
    free(student->grades);
    *student = (Student){0};
}

void student_array_destroy(StudentArray *students)
{
    if (students == NULL)
        return;

    for (size_t index = 0; index < students->size; ++index)
        student_destroy(&students->items[index]);

    free(students->items);
    *students = (StudentArray){0};
}

void student_matches_destroy(StudentMatches *matches)
{
    if (matches == NULL)
        return;

    free(matches->items);
    *matches = (StudentMatches){0};
}

static Status append_match(StudentMatches *matches, const Student *student)
{
    if (matches->size == matches->capacity) {
        const size_t max_capacity = SIZE_MAX / sizeof(*matches->items);
        if (matches->capacity == max_capacity)
            return ST_OVERFLOW;

        size_t new_capacity = grow_capacity(matches->capacity, max_capacity);
        const Student **items = realloc(matches->items, new_capacity * sizeof(*items));
        if (items == NULL)
            return ST_NO_MEMORY;

        matches->items = items;
        matches->capacity = new_capacity;
    }

    matches->items[matches->size++] = student;
    return ST_OK;
}

static const char *student_text_field(const Student *student, StudentField field)
{
    switch (field) {
        case STUDENT_FIELD_LAST_NAME:
            return student->last_name;
        case STUDENT_FIELD_FIRST_NAME:
            return student->first_name;
        case STUDENT_FIELD_GROUP:
            return student->group;
        default:
            return NULL;
    }
}

static Status find_students(const StudentArray *students, StudentField field,
                              uintmax_t id, const char *query, StudentMatches *matches)
{
    if (matches == NULL)
        return ST_INVALID_ARGUMENT;

    Status status = validate_student_array(students);
    if (status != ST_OK)
        return status;

    StudentMatches found = {0};
    for (size_t index = 0; index < students->size; ++index) {
        const Student *student = &students->items[index];
        if (field == STUDENT_FIELD_ID) {
            if (student->id != id)
                continue;
        } else {
            const char *value = student_text_field(student, field);
            if (value == NULL) {
                status = ST_INVALID_ARGUMENT;
                break;
            }
            if (strcmp(value, query) != 0)
                continue;
        }

        status = append_match(&found, student);
        if (status != ST_OK)
            break;
    }

    if (status != ST_OK) {
        student_matches_destroy(&found);
        return status;
    }

    student_matches_destroy(matches);
    *matches = found;
    return ST_OK;
}

Status find_students_by_id(const StudentArray *students, uintmax_t id,
                             StudentMatches *matches)
{
    return find_students(students, STUDENT_FIELD_ID, id, NULL, matches);
}

Status find_students_by_text(const StudentArray *students, StudentField field,
                               const char *query, StudentMatches *matches)
{
    if (query == NULL || *query == '\0' ||
        (field != STUDENT_FIELD_LAST_NAME && field != STUDENT_FIELD_FIRST_NAME &&
         field != STUDENT_FIELD_GROUP))
        return ST_INVALID_ARGUMENT;

    return find_students(students, field, 0, query, matches);
}

static Status student_score_sum(const Student *student, uintmax_t *sum)
{
    if (student == NULL || student->grades == NULL || sum == NULL)
        return ST_INVALID_ARGUMENT;

    uintmax_t result = 0;
    for (size_t index = 0; index < EXAM_COUNT; ++index) {
        uintmax_t grade = student->grades[index];
        if (result > UINTMAX_MAX - grade)
            return ST_OVERFLOW;
        result += grade;
    }

    *sum = result;
    return ST_OK;
}

Status student_average(const Student *student, double *average)
{
    if (average == NULL)
        return ST_INVALID_ARGUMENT;

    uintmax_t sum;
    Status status = student_score_sum(student, &sum);
    if (status != ST_OK)
        return status;

    *average = (double)sum / EXAM_COUNT;
    return ST_OK;
}

Status find_students_above_average(const StudentArray *students, StudentMatches *matches)
{
    if (matches == NULL)
        return ST_INVALID_ARGUMENT;

    Status status = validate_student_array(students);
    if (status != ST_OK)
        return status;
    if (students->size == 0)
        return ST_EMPTY_DATA;

    uintmax_t total = 0;
    for (size_t index = 0; index < students->size; ++index) {
        uintmax_t sum;
        status = student_score_sum(&students->items[index], &sum);
        if (status != ST_OK)
            return status;
        if (total > UINTMAX_MAX - sum)
            return ST_OVERFLOW;
        total += sum;
    }

    uintmax_t threshold = total / students->size;
    StudentMatches found = {0};
    for (size_t index = 0; index < students->size; ++index) {
        const Student *student = &students->items[index];
        uintmax_t sum;
        status = student_score_sum(student, &sum);
        if (status != ST_OK)
            break;
        if (sum <= threshold)
            continue;

        status = append_match(&found, student);
        if (status != ST_OK)
            break;
    }

    if (status != ST_OK) {
        student_matches_destroy(&found);
        return status;
    }

    student_matches_destroy(matches);
    *matches = found;
    return ST_OK;
}

Status sort_students(StudentArray *students, int (*compare)(const void *, const void *))
{
    if (compare == NULL)
        return ST_INVALID_ARGUMENT;

    Status status = validate_student_array(students);
    if (status != ST_OK)
        return status;
    if (students->size > 1)
        qsort(students->items, students->size, sizeof(*students->items), compare);
    return ST_OK;
}

int compare_by_id(const void *left, const void *right)
{
    const Student *first = left;
    const Student *second = right;
    return (first->id > second->id) - (first->id < second->id);
}

int compare_by_last_name(const void *left, const void *right)
{
    const Student *first = left;
    const Student *second = right;
    return strcmp(first->last_name, second->last_name);
}

int compare_by_first_name(const void *left, const void *right)
{
    const Student *first = left;
    const Student *second = right;
    return strcmp(first->first_name, second->first_name);
}

int compare_by_group(const void *left, const void *right)
{
    const Student *first = left;
    const Student *second = right;
    return strcmp(first->group, second->group);
}
