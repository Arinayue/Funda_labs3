#ifndef TASK7_LIVERS_H
#define TASK7_LIVERS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned int day;
    unsigned int month;
    unsigned int year;
} BirthDate;

typedef struct {
    char *last_name;
    char *first_name;
    char *patronymic;
    BirthDate birth_date;
    char sex;
    double monthly_income;
} Liver;

typedef struct LiverNode {
    Liver value;
    struct LiverNode *next;
} LiverNode;

enum {
    LIVER_FIELD_LAST_NAME = 1u << 0,
    LIVER_FIELD_FIRST_NAME = 1u << 1,
    LIVER_FIELD_PATRONYMIC = 1u << 2,
    LIVER_FIELD_BIRTH_DATE = 1u << 3,
    LIVER_FIELD_SEX = 1u << 4,
    LIVER_FIELD_MONTHLY_INCOME = 1u << 5,
    LIVER_FIELD_ALL = LIVER_FIELD_LAST_NAME | LIVER_FIELD_FIRST_NAME |
                      LIVER_FIELD_PATRONYMIC | LIVER_FIELD_BIRTH_DATE |
                      LIVER_FIELD_SEX | LIVER_FIELD_MONTHLY_INCOME
};

typedef struct {
    unsigned int fields;
    Liver values;
} LiverFilter;

typedef enum {
    CHANGE_ADD,
    CHANGE_DELETE,
    CHANGE_EDIT
} LiverChangeKind;

typedef struct LiverChange {
    LiverChangeKind kind;
    LiverNode *node;
    LiverNode *previous;
    Liver old_value;
    struct LiverChange *next;
} LiverChange;

typedef struct {
    LiverNode *head;
    LiverChange *history;
    size_t change_count;
} LiverList;

typedef enum {
    ST_OK,
    ST_END_OF_INPUT,
    ST_INVALID_ARGUMENT,
    ST_INVALID_INPUT,
    ST_NOT_FOUND,
    ST_OVERFLOW,
    ST_NO_MEMORY,
    ST_FILE_OPEN,
    ST_FILE_READ,
    ST_FILE_WRITE,
    ST_FILE_CLOSE
} Status;

void liver_destroy(Liver *value);
void liver_list_destroy(LiverList *list);
Status liver_validate(const Liver *value, unsigned int fields);
Status liver_insert_loaded(LiverList *list, Liver *value);
Status liver_find_next(const LiverNode *start, const LiverFilter *filter, double epsilon,
                         const LiverNode **match);
Status liver_add(LiverList *list, Liver *value);
Status liver_edit(LiverList *list, const LiverNode *target, Liver *value, double epsilon,
                  bool *changed);
Status liver_delete(LiverList *list, const LiverNode *target);
Status liver_undo(LiverList *list, size_t *undone);

#endif
