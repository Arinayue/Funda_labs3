#ifndef STATUS_H
#define STATUS_H

typedef enum {
    ST_OK = 0,
    ST_INVALID_ARGUMENT,
    ST_NO_MEMORY,
    ST_OVERFLOW,
    ST_FILE_OPEN,
    ST_FILE_READ,
    ST_FILE_WRITE,
    ST_INVALID_SYNTAX,
    ST_EMPTY_LINE
} Status;

#endif