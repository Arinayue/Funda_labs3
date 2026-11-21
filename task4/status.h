#ifndef STATUS_H
#define STATUS_H

typedef enum {
    ST_OK = 0,
    ST_NULL_POINTER,
    ST_NO_MEMORY,
    ST_INVALID_ARGUMENT,
    ST_OUT_OF_BOUNDS,
    ST_NOT_FOUND,
    ST_IO,
    ST_INVALID_INPUT,
    ST_END_OF_INPUT,
    ST_DUPLICATE,
    ST_CLOCK
} Status;

#endif
