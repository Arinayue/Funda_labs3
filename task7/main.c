#include "io.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

static Status write_message(const char *message)
{
    if (printf("%s", message) < 0 || fflush(NULL) == EOF) {
        return ST_FILE_WRITE;
    }
    return ST_OK;
}

static const char *status_message(Status status)
{
    switch (status) {
        case ST_INVALID_ARGUMENT:
            return "Некорректный аргумент.";
        case ST_INVALID_INPUT:
            return "Неверный формат данных или неизвестная команда.";
        case ST_NOT_FOUND:
            return "Запись с таким номером не найдена.";
        case ST_OVERFLOW:
            return "Число или размер превышает допустимый предел.";
        case ST_NO_MEMORY:
            return "Не удалось выделить память.";
        case ST_FILE_OPEN:
            return "Не удалось открыть файл.";
        case ST_FILE_READ:
            return "Ошибка чтения.";
        case ST_FILE_WRITE:
            return "Ошибка записи.";
        case ST_FILE_CLOSE:
            return "Не удалось закрыть файл.";
        default:
            return "Неизвестная ошибка.";
    }
}

static Status print_error(Status status, const char *path, size_t line_number)
{
    const char *message = status_message(status);
    int written;

    if (path == NULL) {
        written = puts(message);
    } else if (line_number == 0) {
        written = printf("%s: %s\n", path, message);
    } else {
        written = printf("%s, строка %zu: %s\n", path, line_number, message);
    }
    if (written < 0 || fflush(NULL) == EOF) {
        return ST_FILE_WRITE;
    }
    return ST_OK;
}

static Status read_prompt(const char *prompt, char **line)
{
    Status status = write_message(prompt);
    if (status != ST_OK) {
        return status;
    }
    return read_console_line(line);
}

static Status read_number(const char *prompt, size_t *number)
{
    char *line = NULL;
    Status status = read_prompt(prompt, &line);
    if (status == ST_OK) {
        status = parse_size(line, number);
    }
    free(line);
    return status;
}

static Status print_matches(const LiverList *list, const LiverFilter *filter,
                              double epsilon, size_t *count)
{
    const LiverNode *start = list->head;
    const LiverNode *match = NULL;
    size_t found = 0;
    Status status;

    while ((status = liver_find_next(start, filter, epsilon, &match)) == ST_OK) {
        if (found == SIZE_MAX) {
            return ST_OVERFLOW;
        }
        ++found;
        if (printf("%zu. ", found) < 0) {
            return ST_FILE_WRITE;
        }
        status = print_liver_to_console(&match->value);
        if (status != ST_OK) {
            return status;
        }
        start = match->next;
    }
    if (status != ST_NOT_FOUND) {
        return status;
    }

    *count = found;
    if (found == 0) {
        return write_message("Совпадений нет.\n");
    }
    return ST_OK;
}

static Status read_match(const LiverList *list, const LiverFilter *filter,
                           double epsilon, size_t count, const LiverNode **selected)
{
    size_t number;
    Status status = read_number("Номер записи: ", &number);
    if (status != ST_OK) {
        return status;
    }
    if (number == 0 || number > count) {
        return ST_NOT_FOUND;
    }

    const LiverNode *start = list->head;
    const LiverNode *match = NULL;
    for (size_t index = 0; index < number; ++index) {
        status = liver_find_next(start, filter, epsilon, &match);
        if (status != ST_OK) {
            return status;
        }
        start = match->next;
    }
    *selected = match;
    return ST_OK;
}

static Status find_in_dialog(const LiverList *list, double epsilon, bool select_match,
                               const LiverNode **selected)
{
    const char *prompt = "Формат: фамилия;имя;отчество;день.месяц.год;пол;доход\n"
                         "Введите * для любого значения поля.\n"
                         "Фильтр: ";
    char *line = NULL;
    LiverFilter filter = {0};
    Status status = read_prompt(prompt, &line);
    if (status == ST_OK) {
        status = parse_filter(line, &filter);
    }
    free(line);

    size_t count = 0;
    if (status == ST_OK) {
        status = print_matches(list, &filter, epsilon, &count);
    }
    if (status == ST_OK && select_match && count > 0) {
        status = read_match(list, &filter, epsilon, count, selected);
    }

    liver_destroy(&filter.values);
    return status;
}

static Status write_record(LiverList *list, const LiverNode *target, double epsilon)
{
    const char *prompt = "Формат: фамилия;имя;отчество;день.месяц.год;пол;доход\n"
                         "Запись: ";
    char *line = NULL;
    Liver value = {0};
    Status status = read_prompt(prompt, &line);
    if (status == ST_OK) {
        status = parse_liver(line, &value);
    }
    free(line);
    if (status != ST_OK) {
        return status;
    }

    bool changed;
    if (target == NULL) {
        status = liver_add(list, &value);
        changed = true;
    } else {
        status = liver_edit(list, target, &value, epsilon, &changed);
    }
    liver_destroy(&value);
    if (status != ST_OK) {
        return status;
    }

    if (target == NULL) {
        return write_message("Запись добавлена.\n");
    }
    return write_message(changed ? "Запись изменена.\n" : "Изменений нет.\n");
}

static bool is_same_file(const char *path_a, const char *path_b)
{
    struct stat first;
    struct stat second;
    if (stat(path_a, &first) != 0 || stat(path_b, &second) != 0) {
        return false;
    }

    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

static Status modify_match(LiverList *list, double epsilon, bool edit)
{
    const LiverNode *selected = NULL;
    Status status = find_in_dialog(list, epsilon, true, &selected);
    if (status != ST_OK || selected == NULL) {
        return status;
    }
    if (edit) {
        return write_record(list, selected, epsilon);
    }

    status = liver_delete(list, selected);
    if (status != ST_OK) {
        return status;
    }
    return write_message("Запись удалена.\n");
}

static Status save_in_dialog(const LiverList *list, const char *input_path)
{
    char *path = NULL;
    Status status = read_prompt("Путь к файлу для выгрузки: ", &path);
    if (status == ST_OK && is_same_file(path, input_path)) {
        free(path);
        return write_message("Входной и выходной файлы должны различаться.\n");
    }
    if (status == ST_OK) {
        status = save_livers(path, list);
    }
    free(path);
    if (status != ST_OK) {
        return status;
    }
    return write_message("Список выгружен.\n");
}

static Status undo_in_dialog(LiverList *list)
{
    size_t undone;
    Status status = liver_undo(list, &undone);
    if (status != ST_OK) {
        return status;
    }
    if (printf("Отменено операций: %zu\n", undone) < 0 || fflush(NULL) == EOF) {
        return ST_FILE_WRITE;
    }
    return ST_OK;
}

static Status run_dialog(LiverList *list, double epsilon, const char *input_path)
{
    const char *menu = "\n1. Найти жителей\n"
                       "2. Добавить жителя\n"
                       "3. Изменить запись\n"
                       "4. Удалить запись\n"
                       "5. Выгрузить список\n"
                       "6. Отменить изменения (Undo)\n"
                       "0. Выйти\n"
                       "Выберите команду: ";

    for (;;) {
        size_t command;
        Status status = read_number(menu, &command);
        if (status == ST_OK) {
            switch (command) {
                case 0:
                    return ST_OK;
                case 1:
                    status = find_in_dialog(list, epsilon, false, NULL);
                    break;
                case 2:
                    status = write_record(list, NULL, epsilon);
                    break;
                case 3:
                case 4:
                    status = modify_match(list, epsilon, command == 3);
                    break;
                case 5:
                    status = save_in_dialog(list, input_path);
                    break;
                case 6:
                    status = undo_in_dialog(list);
                    break;
                default:
                    status = ST_INVALID_INPUT;
                    break;
            }
        }

        if (status == ST_END_OF_INPUT) {
            return ST_OK;
        }
        if (status == ST_OK) {
            continue;
        }
        if (status != ST_INVALID_INPUT && status != ST_NOT_FOUND &&
            status != ST_OVERFLOW && status != ST_FILE_OPEN) {
            return status;
        }
        status = print_error(status, NULL, 0);
        if (status != ST_OK) {
            return status;
        }
    }
}

static Status check_arguments(int argc, char *argv[])
{
    if (argc == 2 && argv[1][0] != '\0') {
        return ST_OK;
    }
    Status status = write_message("Использование: livers входной_файл\n");
    return status == ST_OK ? ST_INVALID_ARGUMENT : status;
}

int main(int argc, char *argv[])
{
    if (check_arguments(argc, argv) != ST_OK) {
        return EXIT_FAILURE;
    }

    const double epsilon = 1e-6;
    const char *error_path = argv[1];
    size_t error_line = 0;
    LiverList list = {0};
    Status status = load_livers(argv[1], &list, &error_line);
    if (status == ST_OK) {
        error_path = NULL;
        status = run_dialog(&list, epsilon, argv[1]);
    }
    liver_list_destroy(&list);

    if (status != ST_OK && print_error(status, error_path, error_line) != ST_OK) {
        return EXIT_FAILURE;
    }
    return status == ST_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
