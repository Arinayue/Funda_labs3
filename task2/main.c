#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include "status.h"
#include "func.h"
#include "io.h"

static const char *status_message(Status st)
{
    switch (st) {
        case ST_OK:
            return "Успешно";
        case ST_NULL_POINTER:
            return "Передан нулевой указатель";
        case ST_INVALID_ARGUMENT:
            return "Некорректный аргумент";
        case ST_INVALID_FORMAT:
            return "Некорректный формат данных";
        case ST_INVALID_TIME:
            return "Некорректное время";
        case ST_INVALID_ID:
            return "Некорректный идентификатор";
        case ST_DUPLICATE_ID:
            return "Идентификаторы докладов не уникальны";
        case ST_NO_MEMORY:
            return "Не удалось выделить память";
        case ST_INPUT:
            return "Ошибка чтения входных данных";
        default:
            return "Неизвестная ошибка";
    }
}

int main(void)
{
    talk_t *talks = NULL;
    size_t n = 0;
    size_t halls = 0;
    size_t result = 0;

    Status st = read_talks(stdin, &talks, &n, &halls);
    if (st != ST_OK) {
        if (fprintf(stderr, "Ошибка: %s\n", status_message(st)) < 0)
            return 1;
        return 1;
    }

    st = max_talks_count(talks, n, halls, &result);
    if (st != ST_OK) {
        free(talks);
        if (fprintf(stderr, "Ошибка: %s\n", status_message(st)) < 0)
            return 1;
        return 1;
    }

    printf("%zu\n", result);

    free(talks);
    return 0;
}
