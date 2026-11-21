#include "script.h"

#include <stdio.h>

static const char *script_status_message(ScriptStatus status)
{
    switch (status) {
    case SCRIPT_FILE_OPEN:
        return "Cannot open script file";
    case SCRIPT_FILE_READ:
        return "Script file read error";
    case SCRIPT_INVALID_SYNTAX:
        return "Invalid script syntax";
    case SCRIPT_OVERFLOW:
        return "Arithmetic overflow";
    case SCRIPT_NO_MEMORY:
        return "Memory allocation failed";
    case SCRIPT_DIV_ZERO:
        return "Division by zero polynomial";
    case SCRIPT_OUTPUT_ERROR:
        return "Output error";
    case SCRIPT_INVALID_ARGUMENT:
        return "Invalid argument";
    case SCRIPT_OK:
        return "Success";
    }
    return "Unknown error";
}

int main(int argc, char *argv[])
{
    ScriptStatus status;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <script_file>\n", argv[0]);
        return 1;
    }

    status = script_run_file(argv[1]);
    if (status != SCRIPT_OK) {
        fprintf(stderr, "Error: %s\n", script_status_message(status));
        return 1;
    }
    return 0;
}
