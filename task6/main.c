#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"

static Status validate_args(int argc, char *const argv[])
{
    if (argc < 2 || argv == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    for (int index = 1; index < argc; ++index) {
        if (argv[index] == NULL || argv[index][0] == '\0') {
            return ST_INVALID_ARGUMENT;
        }
    }

    return ST_OK;
}

static Status report_load_error(const char *path, size_t line, Status status)
{
    if (path == NULL || path[0] == '\0') {
        return ST_INVALID_ARGUMENT;
    }

    const char *message;
    switch (status) {
        case ST_INVALID_FORMAT:
            message = "Некорректный формат входных данных.";
            break;
        case ST_RANGE_ERROR:
            message = "Превышен допустимый диапазон значений или размеров.";
            break;
        case ST_NO_MEMORY:
            message = "Не удалось выделить память.";
            break;
        case ST_FILE_OPEN:
            message = "Не удалось открыть файл.";
            break;
        case ST_FILE_READ:
            message = "Не удалось прочитать файл.";
            break;
        case ST_FILE_CLOSE:
            message = "Не удалось закрыть файл.";
            break;
        case ST_INVALID_ARGUMENT:
            message = "Некорректный параметр загрузки.";
            break;
        default:
            return ST_INVALID_ARGUMENT;
    }

    int written = line > 0 ? printf("%s:%zu: %s\n", path, line, message)
                           : printf("%s: %s\n", path, message);
    if (written < 0 || fflush(NULL) == EOF) {
        return ST_OUTPUT_ERROR;
    }
    return ST_OK;
}

static Status read_command(int *command)
{
    if (command == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    int character = getchar();
    if (character == EOF) {
        return ST_END_OF_INPUT;
    }

    int digit = -1;
    bool invalid = false;
    for (; character != '\n' && character != EOF; character = getchar()) {
        if (character == '\r') {
            character = getchar();
            if (character == '\n') {
                break;
            }
            invalid = true;
            if (character == EOF) {
                break;
            }
        }
        if (character == ' ' || character == '\t') {
            continue;
        }
        if (digit >= 0 || character < '0' || character > '9') {
            invalid = true;
        } else {
            digit = character - '0';
        }
    }

    if (invalid || digit < 0) {
        return ST_INVALID_FORMAT;
    }
    *command = digit;
    return ST_OK;
}

static Status run_dialog(const transport_t *head, double epsilon)
{
    if (!isfinite(epsilon) || epsilon <= 0.0) {
        return ST_INVALID_ARGUMENT;
    }

    const struct {
        metric_t metric;
        extreme_t extreme;
        const char *label;
    } queries[] = {
        {METRIC_ROUTE_COUNT, EXTREME_MAX, "Больше всего маршрутов"},
        {METRIC_ROUTE_COUNT, EXTREME_MIN, "Меньше всего маршрутов"},
        {METRIC_TOTAL_DISTANCE, EXTREME_MAX, "Самый длинный суммарный путь"},
        {METRIC_TOTAL_DISTANCE, EXTREME_MIN, "Самый короткий суммарный путь"},
        {METRIC_ROUTE_DISTANCE, EXTREME_MAX, "Самый длинный отдельный маршрут"},
        {METRIC_ROUTE_DISTANCE, EXTREME_MIN, "Самый короткий отдельный маршрут"},
        {METRIC_STOP_DURATION, EXTREME_MAX, "Самая долгая отдельная остановка"},
        {METRIC_STOP_DURATION, EXTREME_MIN, "Самая короткая отдельная остановка"},
        {METRIC_TOTAL_STOP_DURATION, EXTREME_MAX, "Наибольший суммарный простой"}
    };

    for (size_t index = 0; index < sizeof(queries) / sizeof(queries[0]); ++index) {
        if (printf("%zu. %s\n", index + 1, queries[index].label) < 0) {
            return ST_OUTPUT_ERROR;
        }
    }
    if (puts("0. Выход") < 0) {
        return ST_OUTPUT_ERROR;
    }

    for (;;) {
        if (puts("Введите номер команды.") < 0 || fflush(NULL) == EOF) {
            return ST_OUTPUT_ERROR;
        }

        int command;
        Status status = read_command(&command);
        if (status == ST_END_OF_INPUT || (status == ST_OK && command == 0)) {
            return fflush(NULL) == EOF ? ST_OUTPUT_ERROR : ST_OK;
        }
        if (status == ST_INVALID_FORMAT) {
            if (puts("Некорректная команда. Введите одну цифру от 0 до 9.") < 0) {
                return ST_OUTPUT_ERROR;
            }
            continue;
        }
        if (status != ST_OK) {
            return status;
        }

        size_t index = (size_t)(command - 1);
        const char *id;
        status = find_vehicle(head, queries[index].metric, queries[index].extreme, epsilon, &id);

        int written;
        switch (status) {
            case ST_OK:
                written = printf("Транспортное средство «%s».\n", id);
                break;
            case ST_NOT_FOUND:
                written = puts("Нет данных о транспортных средствах.");
                break;
            case ST_RANGE_ERROR:
                written = puts("Результат расчёта выходит за допустимый диапазон.");
                break;
            default:
                return status;
        }
        if (written < 0) {
            return ST_OUTPUT_ERROR;
        }
    }
}

int main(int argc, char *argv[])
{
    Status status = validate_args(argc, argv);
    if (status != ST_OK) {
        if (puts("Укажите один или несколько непустых путей к файлам.") < 0 ||
            fflush(NULL) == EOF) {
            return ST_OUTPUT_ERROR;
        }
        return EXIT_FAILURE;
    }

    transport_t *head = NULL;
    size_t error_line = 0;
    int index;
    for (index = 1; index < argc; ++index) {
        status = load_stop_file(argv[index], &head, &error_line);
        if (status != ST_OK) {
            break;
        }
    }

    Status output_status = ST_OK;
    if (status != ST_OK) {
        output_status = report_load_error(argv[index], error_line, status);
    } else {
        const char *invalid_id;
        status = validate_routes(head, &invalid_id);
        if (status != ST_OK) {
            int written = status == ST_INVALID_ROUTE
                          ? printf("Транспортное средство «%s» имеет некорректный маршрут.\n",
                                   invalid_id)
                          : puts("Не удалось проверить маршруты.");
            if (written < 0 || fflush(NULL) == EOF) {
                output_status = ST_OUTPUT_ERROR;
            }
        }
    }

    if (status == ST_OK) {
        const double epsilon = 1e-9;
        status = run_dialog(head, epsilon);
        if (status == ST_OUTPUT_ERROR ||
            (status != ST_OK && (puts("Не удалось выполнить запрос.") < 0 ||
                                     fflush(NULL) == EOF))) {
            output_status = ST_OUTPUT_ERROR;
        }
    }

    if (clear_transport(&head) != ST_OK) {
        return EXIT_FAILURE;
    }
    if (output_status != ST_OK) {
        return output_status;
    }
    return status == ST_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
