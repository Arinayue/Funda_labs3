#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "input.h"
#include "menu.h"

static int report_error(const char *operation, Status status)
{
    if (fprintf(stderr, "%s failed (status %d).\n", operation, (int)status) < 0)
        return EXIT_FAILURE;
    return EXIT_FAILURE;
}

static Status run_console(tree_t *tree, const char *source_path)
{
    FILE *input = fdopen(STDIN_FILENO, "r");
    if (input == NULL)
        return ST_FILE_OPEN;

    FILE *output = fdopen(STDOUT_FILENO, "w");
    Status status = ST_FILE_OPEN;
    if (output != NULL) {
        status = print_statistics(output, tree);
        if (status == ST_OK)
            status = run_menu(input, output, tree, source_path);
        if (fclose(output) != 0 && status == ST_OK)
            status = ST_FILE_CLOSE;
    }
    if (fclose(input) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    return status;
}

int main(int argc, char *argv[])
{
    const char *path;
    char *separators;
    Status status = parse_args(argc, argv, &path, &separators);
    if (status == ST_INVALID_INPUT) {
        if (fprintf(stderr, "Usage: task9 FILE SEPARATOR [SEPARATOR ...]\n") < 0)
            return EXIT_FAILURE;
        return EXIT_FAILURE;
    }
    if (status != ST_OK)
        return report_error("Argument parsing", status);

    tree_t tree = {0};
    const char *operation = "Reading source file";
    status = read_words(path, separators, &tree);
    free(separators);
    if (status == ST_OK) {
        operation = "Interactive menu";
        status = run_console(&tree, path);
    }
    Status cleanup = tree_destroy(&tree);
    if (status != ST_OK)
        return report_error(operation, status);
    if (cleanup != ST_OK)
        return report_error("Freeing tree", cleanup);
    return EXIT_SUCCESS;
}
