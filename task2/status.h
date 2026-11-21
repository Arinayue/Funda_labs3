#ifndef STATUS_H
#define STATUS_H

typedef enum {
    ST_OK = 0,
    ST_NULL_POINTER,
    ST_INVALID_ARGUMENT,
    ST_INVALID_FORMAT,
    ST_INVALID_TIME,
    ST_INVALID_ID,
    ST_DUPLICATE_ID,
    ST_NO_MEMORY,
    ST_INPUT
} Status;

#endif