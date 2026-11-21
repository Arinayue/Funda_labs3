#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include "io.h"

#define TIME_TOKEN_SIZE 32

Status parse_time(const char *text, int *minutes)
{
    if (text == NULL || minutes == NULL) {
        return ST_NULL_POINTER;
    }
    if (strlen(text) != 5 || text[2] != ':') {
        return ST_INVALID_FORMAT;
    }
    if (!isdigit((unsigned char)text[0]) || !isdigit((unsigned char)text[1]) ||
        !isdigit((unsigned char)text[3]) || !isdigit((unsigned char)text[4])) {
        return ST_INVALID_FORMAT;
    }
    int hours = (text[0] - '0') * 10 + (text[1] - '0');
    int mins = (text[3] - '0') * 10 + (text[4] - '0');
    if (hours > 23 || mins > 59) {
        return ST_INVALID_TIME;
    }
    *minutes = hours * 60 + mins;
    return ST_OK;
}

static Status read_header(FILE *in, size_t *n, size_t *halls)
{
    long long n_value = 0;
    long long k_value = 0;
    if (fscanf(in, "%lld %lld", &n_value, &k_value) != 2) {
        return ST_INPUT;
    }
    if (n_value <= 0 || k_value <= 0) {
        return ST_INVALID_ARGUMENT;
    }
    if ((unsigned long long)n_value > SIZE_MAX / sizeof(talk_t) ||
        (unsigned long long)k_value > SIZE_MAX) {
        return ST_INVALID_ARGUMENT;
    }
    *n = (size_t)n_value;
    *halls = (size_t)k_value;
    return ST_OK;
}

static Status read_one_talk(FILE *in, talk_t *talk)
{
    long long id = 0;
    char start_text[TIME_TOKEN_SIZE];
    char finish_text[TIME_TOKEN_SIZE];

    if (fscanf(in, "%lld %31s %31s", &id, start_text, finish_text) != 3) {
        return ST_INPUT;
    }
    if (id <= 0 || id > INT_MAX) {
        return ST_INVALID_ID;
    }

    int start = 0;
    int finish = 0;
    Status st = parse_time(start_text, &start);
    if (st != ST_OK) {
        return st;
    }
    st = parse_time(finish_text, &finish);
    if (st != ST_OK) {
        return st;
    }
    if (start >= finish) {
        return ST_INVALID_TIME;
    }

    talk->id = (int)id;
    talk->start = start;
    talk->finish = finish;
    return ST_OK;
}

Status read_talks(FILE *in, talk_t **talks, size_t *n, size_t *halls)
{
    if (in == NULL || talks == NULL || n == NULL || halls == NULL) {
        return ST_NULL_POINTER;
    }

    size_t count = 0;
    size_t hall_count = 0;
    Status st = read_header(in, &count, &hall_count);
    if (st != ST_OK) {
        return st;
    }

    talk_t *array = (talk_t *)malloc(count * sizeof(talk_t));
    if (array == NULL) {
        return ST_NO_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        st = read_one_talk(in, &array[i]);
        if (st != ST_OK) {
            free(array);
            return st;
        }
    }

    *talks = array;
    *n = count;
    *halls = hall_count;
    return ST_OK;
}