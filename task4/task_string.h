#ifndef TASK_STRING_H
#define TASK_STRING_H

#include "status.h"

typedef struct {
    char *data;
    int length;
} String;

Status string_create(String *str, const char *src);
Status string_free(String *str);
Status string_compare_order(const String *a, const String *b, int *result);
Status string_compare_eq(const String *a, const String *b, int *result);
Status string_copy(String *dest, const String *src);
Status string_copy_new(String **dest, const String *src);
Status string_concat(String *dest, const String *src);

#endif
