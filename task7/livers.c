#include "livers.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool valid_name(const char *text, bool allow_empty)
{
    const char *letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    return text != NULL && (allow_empty || text[0] != '\0') &&
           text[strspn(text, letters)] == '\0';
}

static bool valid_birth_date(const BirthDate *date)
{
    const unsigned int month_lengths[] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};
    unsigned int max_day;

    if (date->year == 0 || date->month == 0 || date->month > 12) {
        return false;
    }

    max_day = month_lengths[date->month - 1];
    if (date->month == 2 && date->year % 4 == 0 &&
        (date->year % 100 != 0 || date->year % 400 == 0)) {
        ++max_day;
    }

    return date->day > 0 && date->day <= max_day;
}

Status liver_validate(const Liver *value, unsigned int fields)
{
    if (value == NULL || fields == 0 || (fields & ~(unsigned int)LIVER_FIELD_ALL) != 0) {
        return ST_INVALID_ARGUMENT;
    }

    if (((fields & LIVER_FIELD_LAST_NAME) && !valid_name(value->last_name, false)) ||
        ((fields & LIVER_FIELD_FIRST_NAME) && !valid_name(value->first_name, false)) ||
        ((fields & LIVER_FIELD_PATRONYMIC) && !valid_name(value->patronymic, true)) ||
        ((fields & LIVER_FIELD_BIRTH_DATE) && !valid_birth_date(&value->birth_date)) ||
        ((fields & LIVER_FIELD_SEX) && value->sex != 'M' && value->sex != 'W') ||
        ((fields & LIVER_FIELD_MONTHLY_INCOME) &&
         (!isfinite(value->monthly_income) || value->monthly_income < 0.0))) {
        return ST_INVALID_INPUT;
    }

    return ST_OK;
}

void liver_destroy(Liver *value)
{
    if (value == NULL) {
        return;
    }

    free(value->last_name);
    free(value->first_name);
    free(value->patronymic);
    *value = (Liver){0};
}

static void clear_history(LiverList *list)
{
    while (list->history != NULL) {
        LiverChange *change = list->history;
        list->history = change->next;

        if (change->kind == CHANGE_DELETE) {
            liver_destroy(&change->node->value);
            free(change->node);
        } else if (change->kind == CHANGE_EDIT) {
            liver_destroy(&change->old_value);
        }

        free(change);
    }

    list->change_count = 0;
}

void liver_list_destroy(LiverList *list)
{
    if (list == NULL) {
        return;
    }

    clear_history(list);
    while (list->head != NULL) {
        LiverNode *node = list->head;
        list->head = node->next;
        liver_destroy(&node->value);
        free(node);
    }
}

static int compare_birth_dates(const BirthDate *left, const BirthDate *right)
{
    if (left->year != right->year) {
        return left->year < right->year ? -1 : 1;
    }
    if (left->month != right->month) {
        return left->month < right->month ? -1 : 1;
    }
    if (left->day != right->day) {
        return left->day < right->day ? -1 : 1;
    }

    return 0;
}

static bool liver_matches(const Liver *value, const LiverFilter *filter, double epsilon)
{
    unsigned int fields = filter->fields;
    const Liver *key = &filter->values;

    return (!(fields & LIVER_FIELD_LAST_NAME) || strcmp(value->last_name, key->last_name) == 0) &&
           (!(fields & LIVER_FIELD_FIRST_NAME) || strcmp(value->first_name, key->first_name) == 0) &&
           (!(fields & LIVER_FIELD_PATRONYMIC) || strcmp(value->patronymic, key->patronymic) == 0) &&
           (!(fields & LIVER_FIELD_BIRTH_DATE) ||
            compare_birth_dates(&value->birth_date, &key->birth_date) == 0) &&
           (!(fields & LIVER_FIELD_SEX) || value->sex == key->sex) &&
           (!(fields & LIVER_FIELD_MONTHLY_INCOME) ||
            fabs(value->monthly_income - key->monthly_income) <= epsilon);
}

Status liver_find_next(const LiverNode *start, const LiverFilter *filter, double epsilon,
                         const LiverNode **match)
{
    if (filter == NULL || match == NULL || !isfinite(epsilon) || epsilon < 0.0) {
        return ST_INVALID_ARGUMENT;
    }

    Status status = liver_validate(&filter->values, filter->fields);
    if (status != ST_OK) {
        return status;
    }

    for (const LiverNode *node = start; node != NULL; node = node->next) {
        if (liver_matches(&node->value, filter, epsilon)) {
            *match = node;
            return ST_OK;
        }
    }

    *match = NULL;
    return ST_NOT_FOUND;
}

static void insert_ordered(LiverNode **head, LiverNode *node)
{
    LiverNode **link = head;

    while (*link != NULL &&
           compare_birth_dates(&(*link)->value.birth_date, &node->value.birth_date) <= 0) {
        link = &(*link)->next;
    }

    node->next = *link;
    *link = node;
}

static Status insert_new_node(LiverList *list, Liver *value, LiverNode **inserted)
{
    LiverNode *node;

    node = malloc(sizeof(*node));
    if (node == NULL) {
        return ST_NO_MEMORY;
    }

    node->value = *value;
    insert_ordered(&list->head, node);
    *value = (Liver){0};
    *inserted = node;
    return ST_OK;
}

Status liver_insert_loaded(LiverList *list, Liver *value)
{
    if (list == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    Status status = liver_validate(value, LIVER_FIELD_ALL);
    if (status != ST_OK) {
        return status;
    }

    LiverNode *node;
    return insert_new_node(list, value, &node);
}

static LiverNode **find_node_link(LiverNode **head, const LiverNode *target,
                                  LiverNode **previous)
{
    LiverNode **link = head;
    LiverNode *prior = NULL;

    while (*link != NULL && *link != target) {
        prior = *link;
        link = &prior->next;
    }
    if (previous != NULL) {
        *previous = prior;
    }

    return *link != NULL ? link : NULL;
}

static bool liver_equal(const Liver *left, const Liver *right, double epsilon)
{
    return strcmp(left->last_name, right->last_name) == 0 &&
           strcmp(left->first_name, right->first_name) == 0 &&
           strcmp(left->patronymic, right->patronymic) == 0 &&
           compare_birth_dates(&left->birth_date, &right->birth_date) == 0 &&
           left->sex == right->sex &&
           fabs(left->monthly_income - right->monthly_income) <= epsilon;
}

static Status prepare_change(const LiverList *list, LiverChange **change)
{
    if (list->change_count == SIZE_MAX) {
        return ST_OVERFLOW;
    }

    LiverChange *entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        return ST_NO_MEMORY;
    }

    *change = entry;
    return ST_OK;
}

static void push_change(LiverList *list, LiverChange *change)
{
    change->next = list->history;
    list->history = change;
    ++list->change_count;
}

Status liver_add(LiverList *list, Liver *value)
{
    if (list == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    Status status = liver_validate(value, LIVER_FIELD_ALL);
    if (status != ST_OK) {
        return status;
    }

    LiverChange *change;
    status = prepare_change(list, &change);
    if (status != ST_OK) {
        return status;
    }

    LiverNode *node;
    status = insert_new_node(list, value, &node);
    if (status != ST_OK) {
        free(change);
        return status;
    }

    *change = (LiverChange){.kind = CHANGE_ADD, .node = node};
    push_change(list, change);
    return ST_OK;
}

Status liver_edit(LiverList *list, const LiverNode *target, Liver *value, double epsilon,
                  bool *changed)
{
    if (list == NULL || target == NULL || changed == NULL ||
        !isfinite(epsilon) || epsilon < 0.0) {
        return ST_INVALID_ARGUMENT;
    }

    Status status = liver_validate(value, LIVER_FIELD_ALL);
    if (status != ST_OK) {
        return status;
    }

    LiverNode *previous;
    LiverNode **link = find_node_link(&list->head, target, &previous);
    if (link == NULL) {
        return ST_NOT_FOUND;
    }

    LiverNode *node = *link;
    if (liver_equal(&node->value, value, epsilon)) {
        *changed = false;
        return ST_OK;
    }

    LiverChange *change;
    status = prepare_change(list, &change);
    if (status != ST_OK) {
        return status;
    }

    *change = (LiverChange){.kind = CHANGE_EDIT, .node = node, .previous = previous,
                            .old_value = node->value};
    node->value = *value;
    *value = (Liver){0};
    if (compare_birth_dates(&change->old_value.birth_date, &node->value.birth_date) != 0) {
        *link = node->next;
        insert_ordered(&list->head, node);
    }

    push_change(list, change);
    *changed = true;
    return ST_OK;
}

Status liver_delete(LiverList *list, const LiverNode *target)
{
    if (list == NULL || target == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    LiverNode *previous;
    LiverNode **link = find_node_link(&list->head, target, &previous);
    if (link == NULL) {
        return ST_NOT_FOUND;
    }

    LiverChange *change;
    Status status = prepare_change(list, &change);
    if (status != ST_OK) {
        return status;
    }

    LiverNode *node = *link;
    *link = node->next;
    node->next = NULL;
    *change = (LiverChange){.kind = CHANGE_DELETE, .node = node, .previous = previous};
    push_change(list, change);
    return ST_OK;
}

static void undo_change(LiverList *list, const LiverChange *change)
{
    LiverNode *node = change->node;

    if (change->kind != CHANGE_DELETE) {
        LiverNode **link = find_node_link(&list->head, node, NULL);
        *link = node->next;
        liver_destroy(&node->value);
        if (change->kind == CHANGE_ADD) {
            free(node);
            return;
        }
        node->value = change->old_value;
    }

    LiverNode **original_link = change->previous == NULL ? &list->head : &change->previous->next;
    node->next = *original_link;
    *original_link = node;
}

Status liver_undo(LiverList *list, size_t *undone)
{
    if (list == NULL || undone == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    size_t count = list->change_count / 2;
    for (size_t index = 0; index < count; ++index) {
        LiverChange *change = list->history;
        list->history = change->next;
        undo_change(list, change);
        free(change);
    }

    clear_history(list);
    *undone = count;
    return ST_OK;
}
