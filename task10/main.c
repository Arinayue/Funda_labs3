#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

#include "func.h"
#include "status.h"

static const char *status_message(Status st)
{
    switch (st) {
    case ST_OK:
        return "Success";
    case ST_INVALID_ARGUMENT:
        return "Invalid argument (input and output paths must differ)";
    case ST_NO_MEMORY:
        return "Memory allocation failed";
    case ST_OVERFLOW:
        return "Tree is too large";
    case ST_FILE_OPEN:
        return "Cannot open file";
    case ST_FILE_READ:
        return "File read error";
    case ST_FILE_WRITE:
        return "File write error";
    case ST_INVALID_SYNTAX:
        return "Invalid syntax";
    case ST_EMPTY_LINE:
        return "Empty line";
    }
    return "Unknown error";
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        if (fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]) < 0)
            return 1;
        return 1;
    }

    Status st = process_file(argv[1], argv[2]);
    if (st != ST_OK) {
        if (fprintf(stderr, "Error: %s\n", status_message(st)) < 0)
            return 1;
        return 1;
    }

    printf("Trees written to %s\n", argv[2]);
    return 0;
}
