#ifndef TASK5_STUDENTS_H
#define TASK5_STUDENTS_H

#include <stddef.h>
#include <stdint.h>

enum { EXAM_COUNT = 5 };

typedef struct {
    uintmax_t id;
    char *first_name;
    char *last_name;
    char *group;
    unsigned char *grades;
} Student;

typedef struct {
    Student *items;
    size_t size;
    size_t capacity;
} StudentArray;

typedef struct {
    const Student **items;
    size_t size;
    size_t capacity;
} StudentMatches;

typedef enum {
    STUDENT_FIELD_ID,
    STUDENT_FIELD_LAST_NAME,
    STUDENT_FIELD_FIRST_NAME,
    STUDENT_FIELD_GROUP
} StudentField;

typedef enum {
    ST_OK,
    ST_INVALID_ARGUMENT,
    ST_OVERFLOW,
    ST_NO_MEMORY,
    ST_END_OF_INPUT,
    ST_INVALID_FORMAT,
    ST_FILE_OPEN,
    ST_FILE_READ,
    ST_FILE_CLOSE,
    ST_FILE_WRITE,
    ST_EMPTY_DATA
} Status;

Status student_array_append(StudentArray *students, Student *new_student);
void student_destroy(Student *student);
void student_array_destroy(StudentArray *students);
void student_matches_destroy(StudentMatches *matches);

Status find_students_by_id(const StudentArray *students, uintmax_t id,
                             StudentMatches *matches);
Status find_students_by_text(const StudentArray *students, StudentField field,
                               const char *query, StudentMatches *matches);
Status student_average(const Student *student, double *average);
Status find_students_above_average(const StudentArray *students, StudentMatches *matches);
Status sort_students(StudentArray *students, int (*compare)(const void *, const void *));

int compare_by_id(const void *left, const void *right);
int compare_by_last_name(const void *left, const void *right);
int compare_by_first_name(const void *left, const void *right);
int compare_by_group(const void *left, const void *right);

#endif
