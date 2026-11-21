#define _POSIX_C_SOURCE 200809L

#include "func.h"
#include "io.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static Status prompt(int input, int output, const char *label, String *value) {
    if (write_text(output, label) != ST_OK || write_text(output, ": ") != ST_OK) {
        return ST_IO;
    }
    return string_read_line(value, input);
}

static Status parse_positive_int(const String *text, int *number) {
    int value = 0;
    for (int i = 0; i < text->length; i++) {
        int digit = text->data[i] - '0';
        if (digit < 0 || digit > 9 || value > (INT_MAX - digit) / 10) {
            return ST_INVALID_INPUT;
        }
        value = value * 10 + digit;
    }
    if (value == 0) return ST_INVALID_INPUT;
    *number = value;
    return ST_OK;
}

static Status parse_weight(const String *text, double *weight) {
    char *end;
    double value = strtod(text->data, &end);
    if (end == text->data) return ST_INVALID_INPUT;
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0' || !isfinite(value) || value < 0.0) {
        return ST_INVALID_INPUT;
    }

    if (value == 0.0) {
        const char *digit = text->data;
        while (isspace((unsigned char)*digit)) digit++;
        if (*digit == '+' || *digit == '-') digit++;
        int hexadecimal = digit[0] == '0' && (digit[1] == 'x' || digit[1] == 'X');
        if (hexadecimal) digit += 2;
        for (; digit < end; digit++) {
            if (hexadecimal ? (*digit == 'p' || *digit == 'P') :
                              (*digit == 'e' || *digit == 'E')) break;
            if (*digit != '0' && *digit != '.' && !isspace((unsigned char)*digit)) {
                return ST_INVALID_INPUT;
            }
        }
    }
    *weight = value;
    return ST_OK;
}

static Status add_mail(int input, int output, Post *post) {
    enum { CITY, STREET, HOUSE, BUILDING, APARTMENT, POSTAL, WEIGHT, ID,
           CREATED, DEADLINE, DELIVERED, FIELD_COUNT };
    const char *labels[] = {
        "Город", "Улица", "Дом", "Корпус (можно пропустить)", "Квартира",
        "Индекс (6 цифр)", "Вес, кг", "Трек-номер (14 цифр)",
        "Создано (дд:мм:гггг чч:мм:сс)", "Срок доставки (дд:мм:гггг чч:мм:сс)",
        "Вручено (дд:мм:гггг чч:мм:сс, пусто если ещё не вручено)"
    };
    String fields[FIELD_COUNT] = {0};
    Address recipient = {0};
    Mail mail = {0};
    int house = 0, apartment = 0;
    double weight = 0.0;

    Status st = ST_OK;
    for (int i = 0; i < FIELD_COUNT && st == ST_OK; i++) {
        st = prompt(input, output, labels[i], &fields[i]);
        if (st != ST_OK) break;
        if (i == HOUSE) st = parse_positive_int(&fields[i], &house);
        if (i == APARTMENT) st = parse_positive_int(&fields[i], &apartment);
        if (i == WEIGHT) st = parse_weight(&fields[i], &weight);
    }
    if (st == ST_OK) {
        st = address_init(&recipient, fields[CITY].data, fields[STREET].data,
                          house, fields[BUILDING].data, apartment, fields[POSTAL].data);
    }
    if (st == ST_OK) {
        st = mail_init(&mail, &recipient, weight, fields[ID].data, fields[CREATED].data,
                       fields[DEADLINE].data, fields[DELIVERED].data);
    }
    if (st == ST_OK) st = post_add_mail(post, &mail);

    mail_free(&mail);
    address_free(&recipient);
    for (int i = 0; i < FIELD_COUNT; i++) string_free(&fields[i]);
    if (st == ST_OK) st = write_text(output, "Отправление добавлено.\n");
    return st;
}

static Status print_mail(const Mail *mail) {
    const Address *address = &mail->recipient;
    if (printf("%s | %s, %s, %s, д. %d", mail->tracking_id.data,
               address->postal_code.data, address->city.data, address->street.data,
               address->house_number) < 0) return ST_IO;
    if (address->building.length != 0 && printf(", корп. %s", address->building.data) < 0) {
        return ST_IO;
    }
    if (printf(", кв. %d | %.10g кг\n  Создано: %s | Срок: %s | Вручено: %s\n",
               address->apartment_number, mail->weight, mail->creation_time.data,
               mail->deadline.data, mail->delivery_time.length ? mail->delivery_time.data : "—") < 0 ||
        fflush(stdout) == EOF) {
        return ST_IO;
    }
    return ST_OK;
}

static Status handle_tracking_id(int input, int output, Post *post, int remove) {
    String id = {0};
    const Mail *mail = NULL;
    Status st = prompt(input, output, "Трек-номер (14 цифр)", &id);
    if (st == ST_OK && remove) {
        st = post_remove_mail(post, &id);
        if (st == ST_OK) st = write_text(output, "Отправление удалено.\n");
    } else if (st == ST_OK) {
        st = post_search_mail(post, &id, &mail);
        if (st == ST_OK) st = print_mail(mail);
    }
    string_free(&id);
    return st;
}

static Status list_mails(int output, const Post *post, char choice) {
    int count = post->count;
    const Mail **matches = NULL;
    Status st = ST_OK;

    if (choice != '1') {
        time_t current = time(NULL);
        if (current == (time_t)-1) return ST_CLOCK;
        struct tm local;
        char date[20];
        if (localtime_r(&current, &local) == NULL ||
            strftime(date, sizeof(date), "%d:%m:%Y %H:%M:%S", &local) != 19) {
            return ST_CLOCK;
        }
        String now = {date, 19};
        st = post_filter(post, &now, choice == '5', &matches, &count);
    }
    if (st == ST_OK && count == 0) st = write_text(output, "Отправлений нет.\n");
    for (int i = 0; i < count && st == ST_OK; i++) {
        st = print_mail(matches == NULL ? &post->mails[i] : matches[i]);
    }
    free(matches);
    return st;
}

static Status run_menu(int input, int output, Post *post) {
    String command = {0};
    Status st = write_text(output, "1 Все | 2 Добавить | 3 Найти | 4 Просроченные\n"
             "5 Доставленные | 6 Удалить | 0 Выход\n");
    if (st != ST_OK) return st;

    for (;;) {
        st = prompt(input, output, "Команда", &command);
        if (st == ST_END_OF_INPUT) {
            st = ST_OK;
            break;
        }
        if (st != ST_OK && st != ST_INVALID_INPUT) break;
        char choice = st == ST_OK && command.length == 1 ? command.data[0] : '\0';
        if (choice == '0') break;
        switch (choice) {
            case '1': case '4': case '5': st = list_mails(output, post, choice); break;
            case '2': st = add_mail(input, output, post); break;
            case '3': case '6': st = handle_tracking_id(input, output, post, choice == '6'); break;
            default:
                st = write_text(output, "Введите номер команды от 0 до 6.\n");
                break;
        }
        const char *message = NULL;
        switch (st) {
            case ST_OK: continue;
            case ST_INVALID_INPUT: message = "Некорректные данные.\n"; break;
            case ST_NOT_FOUND: message = "Отправление не найдено.\n"; break;
            case ST_DUPLICATE: message = "Этот трек-номер уже используется.\n"; break;
            case ST_END_OF_INPUT: st = ST_OK; break;
            default: break;
        }
        if (message == NULL) break;
        st = write_text(output, message);
        if (st != ST_OK) break;
    }
    string_free(&command);
    return st;
}

int main(void) {
#ifdef SIGPIPE
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) return EXIT_FAILURE;
#endif
    Address office = {0};
    Post post = {0};
    Status st = address_init(&office, "Москва", "Тверская", 1, "", 1, "101000");
    if (st == ST_OK) st = post_init(&post, &office);
    address_free(&office);
    if (st == ST_OK) st = run_menu(STDIN_FILENO, STDOUT_FILENO, &post);
    post_free(&post);

    const char *message;
    switch (st) {
        case ST_OK: return EXIT_SUCCESS;
        case ST_NO_MEMORY: message = "Не удалось выделить память.\n"; break;
        case ST_IO: message = "Ошибка ввода или вывода.\n"; break;
        case ST_CLOCK: message = "Не удалось прочитать системное время.\n"; break;
        default: message = "Не удалось выполнить операцию.\n"; break;
    }
    if (write_text(STDERR_FILENO, message) != ST_OK) return ST_IO;
    return EXIT_FAILURE;
}
