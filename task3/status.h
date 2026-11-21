#ifndef STATUS_H
#define STATUS_H

typedef enum {
    ST_OK = 0,
    ST_NULL_POINTER,
    ST_INVALID_ARGUMENT,
    ST_NO_MEMORY,
    ST_FILE_OPEN,
    ST_FILE_READ,
    ST_FILE_CLOSE,
    ST_HEAP_EMPTY,
    ST_HEAP_FULL,
    ST_CODE_TOO_LONG,
    ST_OVERFLOW
} Status;

#endif