#ifndef TASK8_SCRIPT_H
#define TASK8_SCRIPT_H

typedef enum {
    SCRIPT_OK,
    SCRIPT_INVALID_ARGUMENT,
    SCRIPT_FILE_OPEN,
    SCRIPT_FILE_READ,
    SCRIPT_INVALID_SYNTAX,
    SCRIPT_OVERFLOW,
    SCRIPT_NO_MEMORY,
    SCRIPT_DIV_ZERO,
    SCRIPT_OUTPUT_ERROR
} ScriptStatus;

ScriptStatus script_run_file(const char *path);

#endif
