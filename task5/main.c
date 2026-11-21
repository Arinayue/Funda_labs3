#define _POSIX_C_SOURCE 200809L

#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    FILE *input;
    FILE *output;
} console_t;

static Status write_message(FILE *stream, const char *message)
{
    if (fputs(message, stream) == EOF || fflush(stream) == EOF)
        return ST_FILE_WRITE;
    return ST_OK;
}

static const char *status_message(Status status)
{
    switch (status) {
        case ST_INVALID_ARGUMENT:
            return "Некорректный аргумент.";
        case ST_INVALID_FORMAT:
            return "Неверный формат данных или неизвестная команда.";
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
        case ST_EMPTY_DATA:
            return "Общее среднее недоступно: список студентов пуст.";
        default:
            return "Неизвестный статус.";
    }
}

static Status print_error(Status status, const char *path, size_t line_number)
{
    const char *message = status_message(status);
    int written;
    if (path == NULL)
        written = fprintf(stderr, "%s\n", message);
    else if (line_number != 0)
        written = fprintf(stderr, "%s, строка %zu: %s\n", path, line_number, message);
    else
        written = fprintf(stderr, "%s: %s\n", path, message);

    if (written < 0)
        return ST_FILE_WRITE;
    return ST_OK;
}

static Status read_prompt(const console_t *console, const char *prompt, char **line)
{
    Status status = write_message(console->output, prompt);
    if (status != ST_OK)
        return status;
    return read_line(console->input, line);
}

static Status read_choice(const console_t *console, const char *prompt, uintmax_t min_choice,
                            uintmax_t max_choice, uintmax_t *choice)
{
    char *line = NULL;
    Status status = read_prompt(console, prompt, &line);
    if (status != ST_OK)
        return status;

    status = parse_id(line, choice);
    free(line);
    if (status == ST_OK && (*choice < min_choice || *choice > max_choice))
        return ST_INVALID_FORMAT;
    return status;
}

static Status select_student_field(const console_t *console, StudentField *field)
{
    const char *prompt = "1. ID\n2. Фамилия\n3. Имя\n4. Группа\nВыберите поле: ";
    uintmax_t choice;
    Status status = read_choice(console, prompt, 1, 4, &choice);
    if (status != ST_OK)
        return status;

    *field = (StudentField)(choice - 1);
    return ST_OK;
}

static Status read_search_matches(const console_t *console,
                                    const StudentArray *students, StudentField field,
                                    StudentMatches *matches)
{
    const char *prompt = field == STUDENT_FIELD_ID ? "Введите ID: " : "Введите значение: ";
    char *line = NULL;
    Status status = read_prompt(console, prompt, &line);
    if (status != ST_OK)
        return status;

    if (field == STUDENT_FIELD_ID) {
        uintmax_t id;
        status = parse_id(line, &id);
        if (status == ST_OK)
            status = find_students_by_id(students, id, matches);
    } else {
        const char *query;
        status = parse_text_key(line, &query);
        if (status == ST_OK)
            status = find_students_by_text(students, field, query, matches);
    }

    free(line);
    return status;
}

static Status run_search(const console_t *console, const StudentArray *students)
{
    StudentField field;
    Status status = select_student_field(console, &field);
    if (status != ST_OK)
        return status;

    StudentMatches matches = {0};
    status = read_search_matches(console, students, field, &matches);
    if (status == ST_OK)
        status = print_matches(console->output, &matches);
    student_matches_destroy(&matches);
    return status;
}

static Status run_sort(const console_t *console, StudentArray *students)
{
    StudentField field;
    Status status = select_student_field(console, &field);
    if (status != ST_OK)
        return status;

    int (*const comparators[])(const void *, const void *) = {
        [STUDENT_FIELD_ID] = compare_by_id,
        [STUDENT_FIELD_LAST_NAME] = compare_by_last_name,
        [STUDENT_FIELD_FIRST_NAME] = compare_by_first_name,
        [STUDENT_FIELD_GROUP] = compare_by_group
    };
    status = sort_students(students, comparators[field]);
    if (status != ST_OK)
        return status;
    return print_students(console->output, students);
}

static Status run_report(const console_t *console, const StudentArray *students,
                           const char *trace_path,
                           bool above_average)
{
    StudentMatches matches = {0};
    Status status = above_average
        ? find_students_above_average(students, &matches)
        : read_search_matches(console, students, STUDENT_FIELD_ID, &matches);

    if (status == ST_OK && matches.size == 0) {
        status = write_message(console->output, above_average
            ? "Нет студентов со средним баллом выше общего.\n"
            : "Совпадений нет.\n");
    } else if (status == ST_OK) {
        status = append_trace_report(trace_path, &matches, !above_average);
        if (status == ST_OK)
            status = write_message(console->output, "Отчёт записан.\n");
    }

    student_matches_destroy(&matches);
    return status;
}

static Status run_dialog(const console_t *console, StudentArray *students,
                           const char *trace_path)
{
    const char *menu = "\n1. Найти студентов\n"
                       "2. Сортировать студентов\n"
                       "3. Записать отчёт по ID\n"
                       "4. Записать студентов со средним баллом выше общего\n"
                       "0. Выйти\n"
                       "Выберите команду: ";

    for (;;) {
        uintmax_t choice;
        Status status = read_choice(console, menu, 0, 4, &choice);
        if (status == ST_OK) {
            switch (choice) {
                case 0:
                    return ST_OK;
                case 1:
                    status = run_search(console, students);
                    break;
                case 2:
                    status = run_sort(console, students);
                    break;
                case 3:
                    status = run_report(console, students, trace_path, false);
                    break;
                case 4:
                    status = run_report(console, students, trace_path, true);
                    break;
            }
        }

        if (status == ST_END_OF_INPUT)
            return ST_OK;
        if (status == ST_OK)
            continue;
        if (status != ST_INVALID_FORMAT && status != ST_OVERFLOW &&
            status != ST_EMPTY_DATA)
            return status;

        status = print_error(status, NULL, 0);
        if (status != ST_OK)
            return status;
    }
}

static Status run_console(StudentArray *students, const char *trace_path)
{
    console_t console;
    console.input = fdopen(STDIN_FILENO, "r");
    if (console.input == NULL)
        return ST_FILE_OPEN;

    console.output = fdopen(STDOUT_FILENO, "w");
    Status status = ST_FILE_OPEN;
    if (console.output != NULL) {
        status = run_dialog(&console, students, trace_path);
        if (fclose(console.output) != 0 && status == ST_OK)
            status = ST_FILE_CLOSE;
    }
    if (fclose(console.input) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    return status;
}

static bool is_same_file(const char *path_a, const char *path_b)
{
    struct stat first, second;
    if (stat(path_a, &first) != 0 || stat(path_b, &second) != 0)
        return false;

    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

static Status check_arguments(int argc, char *argv[])
{
    const char *message = NULL;
    if (argc != 3 || *argv[1] == '\0' || *argv[2] == '\0')
        message = "Использование: students входной_файл файл_отчёта\n";
    else if (is_same_file(argv[1], argv[2]))
        message = "Входной файл и файл отчёта должны быть разными файлами.\n";
    if (message == NULL)
        return ST_OK;

    return fprintf(stderr, "%s", message) < 0
        ? ST_FILE_WRITE : ST_INVALID_ARGUMENT;
}

int main(int argc, char *argv[])
{
    Status status = check_arguments(argc, argv);
    if (status != ST_OK)
        return EXIT_FAILURE;

    StudentArray students = {0};
    size_t error_line = 0;
    const char *error_path = argv[1];
    status = load_students(argv[1], &students, &error_line);
    if (status == ST_OK) {
        status = run_console(&students, argv[2]);
        error_path = status == ST_FILE_OPEN || status == ST_FILE_CLOSE
            ? argv[2] : NULL;
    }
    student_array_destroy(&students);

    if (status != ST_OK && print_error(status, error_path, error_line) != ST_OK)
        return EXIT_FAILURE;
    return status == ST_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
