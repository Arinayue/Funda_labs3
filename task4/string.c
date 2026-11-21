#include "task_string.h"
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

Status string_create(String *str, const char *src) {
    if (str == NULL || src == NULL) {
        return ST_NULL_POINTER;
    }

    size_t length = strlen(src);
    if (length > INT_MAX || length == SIZE_MAX) return ST_OUT_OF_BOUNDS;

    char *data = malloc(length + 1);
    if (data == NULL) return ST_NO_MEMORY;
    memcpy(data, src, length + 1);
    str->data = data;
    str->length = (int)length;
    return ST_OK;
}

Status string_free(String *str) {
    if (str == NULL) {
        return ST_NULL_POINTER;
    }
    if (str->data != NULL) {
        free(str->data);
        str->data = NULL;
    }
    str->length = 0;
    return ST_OK;
}

Status string_compare_order(const String *a, const String *b, int *result) {
    if (a == NULL || b == NULL || result == NULL || a->data == NULL || b->data == NULL) {
        return ST_NULL_POINTER;
    }

    if (a->length < b->length) {
        *result = -1;
        return ST_OK;
    }
    if (a->length > b->length) {
        *result = 1;
        return ST_OK;
    }

    int cmp = strcmp(a->data, b->data);
    if (cmp < 0) {
        *result = -1;
    } else if (cmp > 0) {
        *result = 1;
    } else {
        *result = 0;
    }

    return ST_OK;
}

Status string_compare_eq(const String *a, const String *b, int *result) {
    if (a == NULL || b == NULL || result == NULL || a->data == NULL || b->data == NULL) {
        return ST_NULL_POINTER;
    }

    if (a->length != b->length) {
        *result = 0;
        return ST_OK;
    }

    *result = (strcmp(a->data, b->data) == 0) ? 1 : 0;
    return ST_OK;
}

Status string_copy(String *dest, const String *src) {
    if (dest == NULL || src == NULL) {
        return ST_NULL_POINTER;
    }

    String copy;
    Status st = string_create(&copy, src->data);
    if (st != ST_OK) return st;
    free(dest->data);
    *dest = copy;
    return ST_OK;
}

Status string_copy_new(String **dest, const String *src) {
    if (dest == NULL || src == NULL) {
        return ST_NULL_POINTER;
    }

    String *new_str = (String *)malloc(sizeof(String));
    if (new_str == NULL) {
        return ST_NO_MEMORY;
    }

    Status st = string_create(new_str, src->data);
    if (st != ST_OK) {
        free(new_str);
        return st;
    }

    *dest = new_str;
    return ST_OK;
}

Status string_concat(String *dest, const String *src) {
    if (dest == NULL || src == NULL) {
        return ST_NULL_POINTER;
    }

    if (dest->data == NULL || src->data == NULL) return ST_NULL_POINTER;
    if (dest->length < 0 || src->length < 0 ||
        src->length > INT_MAX - dest->length ||
        (uintmax_t)dest->length + (uintmax_t)src->length >= SIZE_MAX) {
        return ST_OUT_OF_BOUNDS;
    }

    int length = dest->length + src->length;
    char *data = malloc((size_t)length + 1);
    if (data == NULL) return ST_NO_MEMORY;
    memcpy(data, dest->data, (size_t)dest->length);
    memcpy(data + dest->length, src->data, (size_t)src->length + 1);
    free(dest->data);
    dest->data = data;
    dest->length = length;
    return ST_OK;
}
