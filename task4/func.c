#include "func.h"
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

Status validate_postal_code(const String *code) {
    if (code == NULL || code->data == NULL) return ST_NULL_POINTER;
    if (code->length != 6) return ST_INVALID_INPUT;
    for (int i = 0; i < 6; i++) {
        if (code->data[i] < '0' || code->data[i] > '9') {
            return ST_INVALID_INPUT;
        }
    }
    return ST_OK;
}

Status validate_tracking_id(const String *id) {
    if (id == NULL || id->data == NULL) return ST_NULL_POINTER;
    if (id->length != 14) return ST_INVALID_INPUT;
    for (int i = 0; i < 14; i++) {
        if (id->data[i] < '0' || id->data[i] > '9') {
            return ST_INVALID_INPUT;
        }
    }
    return ST_OK;
}

Status validate_datetime(const String *dt) {
    if (dt == NULL || dt->data == NULL) return ST_NULL_POINTER;
    if (dt->length != 19) return ST_INVALID_INPUT;

    const char *d = dt->data;
    if (d[2] != ':' || d[5] != ':' || d[10] != ' ' || d[13] != ':' || d[16] != ':') {
        return ST_INVALID_INPUT;
    }

    for (int i = 0; i < 19; i++) {
        if (i == 2 || i == 5 || i == 10 || i == 13 || i == 16) continue;
        if (d[i] < '0' || d[i] > '9') return ST_INVALID_INPUT;
    }
    int day = (d[0] - '0') * 10 + d[1] - '0';
    int month = (d[3] - '0') * 10 + d[4] - '0';
    int year = (d[6] - '0') * 1000 + (d[7] - '0') * 100
        + (d[8] - '0') * 10 + d[9] - '0';
    int hour = (d[11] - '0') * 10 + d[12] - '0';
    int minute = (d[14] - '0') * 10 + d[15] - '0';
    int second = (d[17] - '0') * 10 + d[18] - '0';
    if (year == 0 || month < 1 || month > 12 || day < 1 ||
        hour > 23 || minute > 59 || second > 59) {
        return ST_INVALID_INPUT;
    }
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
        days[1]++;
    }
    if (day > days[month - 1]) return ST_INVALID_INPUT;
    return ST_OK;
}

static int compare_datetime(const String *left, const String *right) {
    int comparison = memcmp(left->data + 6, right->data + 6, 4);
    if (comparison == 0) comparison = memcmp(left->data + 3, right->data + 3, 2);
    if (comparison == 0) comparison = memcmp(left->data, right->data, 2);
    if (comparison == 0) comparison = memcmp(left->data + 11, right->data + 11, 8);
    return comparison;
}

Status address_init(Address *addr, const char *city, const char *street, int house, const char *building, int apt, const char *postal) {
    if (addr == NULL || city == NULL || street == NULL || building == NULL || postal == NULL) {
        return ST_NULL_POINTER;
    }
    if (house <= 0 || apt <= 0) {
        return ST_INVALID_INPUT;
    }

    Status st = string_create(&addr->city, city);
    if (st != ST_OK) return st;
    if (addr->city.length == 0) {
        string_free(&addr->city);
        return ST_INVALID_INPUT;
    }

    st = string_create(&addr->street, street);
    if (st != ST_OK) {
        string_free(&addr->city);
        return st;
    }
    if (addr->street.length == 0) {
        string_free(&addr->street);
        string_free(&addr->city);
        return ST_INVALID_INPUT;
    }

    addr->house_number = house;

    st = string_create(&addr->building, building);
    if (st != ST_OK) {
        string_free(&addr->street);
        string_free(&addr->city);
        return st;
    }

    addr->apartment_number = apt;

    st = string_create(&addr->postal_code, postal);
    if (st != ST_OK) {
        string_free(&addr->building);
        string_free(&addr->street);
        string_free(&addr->city);
        return st;
    }

    st = validate_postal_code(&addr->postal_code);
    if (st != ST_OK) {
        string_free(&addr->postal_code);
        string_free(&addr->building);
        string_free(&addr->street);
        string_free(&addr->city);
        return ST_INVALID_INPUT;
    }

    return ST_OK;
}

Status address_free(Address *addr) {
    if (addr == NULL) return ST_NULL_POINTER;
    string_free(&addr->city);
    string_free(&addr->street);
    string_free(&addr->building);
    string_free(&addr->postal_code);
    return ST_OK;
}

Status mail_init(Mail *mail, const Address *rec, double weight, const char *id,
                 const char *created, const char *deadline, const char *delivered) {
    if (mail == NULL || rec == NULL || id == NULL || created == NULL ||
        deadline == NULL || delivered == NULL) {
        return ST_NULL_POINTER;
    }
    if (!isfinite(weight) || weight < 0.0) return ST_INVALID_INPUT;

    Mail value = {0};
    Status st = address_init(&value.recipient, rec->city.data, rec->street.data,
                             rec->house_number, rec->building.data,
                             rec->apartment_number, rec->postal_code.data);
    if (st == ST_OK) st = string_create(&value.tracking_id, id);
    if (st == ST_OK) st = validate_tracking_id(&value.tracking_id);
    if (st == ST_OK) st = string_create(&value.creation_time, created);
    if (st == ST_OK) st = validate_datetime(&value.creation_time);
    if (st == ST_OK) st = string_create(&value.deadline, deadline);
    if (st == ST_OK) st = validate_datetime(&value.deadline);
    if (st == ST_OK) st = string_create(&value.delivery_time, delivered);
    if (st == ST_OK && value.delivery_time.length != 0) {
        st = validate_datetime(&value.delivery_time);
    }
    if (st == ST_OK &&
        (compare_datetime(&value.deadline, &value.creation_time) < 0 ||
         (value.delivery_time.length != 0 &&
          compare_datetime(&value.delivery_time, &value.creation_time) < 0))) {
        st = ST_INVALID_INPUT;
    }
    if (st != ST_OK) {
        mail_free(&value);
        return st;
    }
    value.weight = weight;
    *mail = value;
    return ST_OK;
}

Status mail_free(Mail *mail) {
    if (mail == NULL) return ST_NULL_POINTER;
    address_free(&mail->recipient);
    string_free(&mail->tracking_id);
    string_free(&mail->creation_time);
    string_free(&mail->delivery_time);
    string_free(&mail->deadline);
    return ST_OK;
}

Status post_init(Post *post, const Address *office_addr) {
    if (post == NULL || office_addr == NULL) return ST_NULL_POINTER;
    post->post_office_address = (Address *)malloc(sizeof(Address));
    if (post->post_office_address == NULL) return ST_NO_MEMORY;

    Status st = address_init(post->post_office_address, office_addr->city.data, office_addr->street.data, office_addr->house_number, office_addr->building.data, office_addr->apartment_number, office_addr->postal_code.data);
    if (st != ST_OK) {
        free(post->post_office_address);
        post->post_office_address = NULL;
        return st;
    }

    post->capacity = 4;
    post->count = 0;
    post->mails = (Mail *)malloc(post->capacity * sizeof(Mail));
    if (post->mails == NULL) {
        address_free(post->post_office_address);
        free(post->post_office_address);
        post->post_office_address = NULL;
        return ST_NO_MEMORY;
    }
    return ST_OK;
}

Status post_free(Post *post) {
    if (post == NULL) return ST_NULL_POINTER;
    if (post->post_office_address != NULL) {
        address_free(post->post_office_address);
        free(post->post_office_address);
        post->post_office_address = NULL;
    }
    if (post->mails != NULL) {
        for (int i = 0; i < post->count; i++) {
            mail_free(&post->mails[i]);
        }
        free(post->mails);
        post->mails = NULL;
    }
    post->count = 0;
    post->capacity = 0;
    return ST_OK;
}

static int compare_mail(const Mail *left, const Mail *right) {
    int comparison = strcmp(left->recipient.postal_code.data, right->recipient.postal_code.data);
    if (comparison == 0) comparison = strcmp(left->tracking_id.data, right->tracking_id.data);
    return comparison;
}

Status post_add_mail(Post *post, const Mail *mail) {
    if (post == NULL || mail == NULL || post->mails == NULL) return ST_NULL_POINTER;

    const Mail *found = NULL;
    Status st = post_search_mail(post, &mail->tracking_id, &found);
    if (st == ST_OK) return ST_DUPLICATE;
    if (st != ST_NOT_FOUND) return st;

    Mail copy;
    st = mail_init(&copy, &mail->recipient, mail->weight, mail->tracking_id.data,
                   mail->creation_time.data, mail->deadline.data, mail->delivery_time.data);
    if (st != ST_OK) return st;

    if (post->count == post->capacity) {
        if (post->capacity > INT_MAX / 2 ||
            (size_t)post->capacity > SIZE_MAX / sizeof(Mail) / 2) {
            mail_free(&copy);
            return ST_NO_MEMORY;
        }
        int capacity = post->capacity * 2;
        Mail *mails = realloc(post->mails, (size_t)capacity * sizeof(*mails));
        if (mails == NULL) {
            mail_free(&copy);
            return ST_NO_MEMORY;
        }
        post->mails = mails;
        post->capacity = capacity;
    }

    int index = post->count;
    while (index > 0 && compare_mail(&post->mails[index - 1], &copy) > 0) {
        post->mails[index] = post->mails[index - 1];
        index--;
    }
    post->mails[index] = copy;
    post->count++;
    return ST_OK;
}

Status post_remove_mail(Post *post, const String *tracking_id) {
    const Mail *found = NULL;
    Status st = post_search_mail(post, tracking_id, &found);
    if (st != ST_OK) return st;

    int index = (int)(found - post->mails);
    mail_free(&post->mails[index]);
    post->count--;
    for (int i = index; i < post->count; i++) {
        post->mails[i] = post->mails[i + 1];
    }
    post->mails[post->count] = (Mail){0};
    return ST_OK;
}

Status post_search_mail(const Post *post, const String *tracking_id, const Mail **out_mail) {
    if (post == NULL || out_mail == NULL) return ST_NULL_POINTER;
    *out_mail = NULL;
    Status st = validate_tracking_id(tracking_id);
    if (st != ST_OK) return st;

    for (int i = 0; i < post->count; i++) {
        if (strcmp(post->mails[i].tracking_id.data, tracking_id->data) == 0) {
            *out_mail = &post->mails[i];
            return ST_OK;
        }
    }
    return ST_NOT_FOUND;
}

static int compare_creation(const void *left, const void *right) {
    const Mail *const *a = left;
    const Mail *const *b = right;
    int comparison = compare_datetime(&(*a)->creation_time, &(*b)->creation_time);
    if (comparison == 0) comparison = compare_mail(*a, *b);
    return comparison;
}

Status post_filter(const Post *post, const String *now, int delivered,
                   const Mail ***out_list, int *out_count) {
    if (post == NULL || out_list == NULL || out_count == NULL) return ST_NULL_POINTER;
    if (delivered != 0 && delivered != 1) return ST_INVALID_ARGUMENT;
    *out_list = NULL;
    *out_count = 0;
    Status st = validate_datetime(now);
    if (st != ST_OK || post->count == 0) return st;

    if ((size_t)post->count > SIZE_MAX / sizeof(const Mail *)) {
        return ST_NO_MEMORY;
    }
    const Mail **list = malloc((size_t)post->count * sizeof(*list));
    if (list == NULL) return ST_NO_MEMORY;

    int count = 0;
    for (int i = 0; i < post->count; i++) {
        const Mail *mail = &post->mails[i];
        int handed_over = mail->delivery_time.length != 0 &&
            compare_datetime(&mail->delivery_time, now) <= 0;
        if ((delivered && handed_over) ||
            (!delivered && !handed_over && compare_datetime(now, &mail->deadline) >= 0)) {
            list[count++] = mail;
        }
    }
    qsort(list, (size_t)count, sizeof(*list), compare_creation);
    *out_list = list;
    *out_count = count;
    return ST_OK;
}
