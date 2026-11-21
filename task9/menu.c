#include "menu.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "input.h"
#include "snapshot.h"

static Status print_word(FILE *output, const char *word)
{
    const unsigned char *byte = (const unsigned char *)word;
    for (; *byte != '\0'; ++byte) {
        const char *escape = NULL;
        switch (*byte) {
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\\': escape = "\\\\"; break;
        }
        if ((escape != NULL ? fputs(escape, output) : fputc(*byte, output)) == EOF)
            return ST_FILE_WRITE;
    }
    return ST_OK;
}

static Status print_entry(FILE *output, const word_node_t *node)
{
    Status status = print_word(output, node->word);
    if (status == ST_OK && fprintf(output, ": %zu\n", node->count) < 0)
        return ST_FILE_WRITE;
    return status;
}

Status print_statistics(FILE *output, const tree_t *tree)
{
    if (output == NULL || tree == NULL)
        return ST_INVALID_ARGUMENT;

    const word_node_t *node;
    Status status = tree_first(tree, &node);
    while (status == ST_OK) {
        status = print_entry(output, node);
        if (status != ST_OK)
            return status;
        status = tree_next(node, &node);
    }
    return status == ST_END ? ST_OK : status;
}

static Status read_line(FILE *input, char **line)
{
    Status status = read_field(input, "\n", line);
    if (status != ST_INVALID_INPUT)
        return status;

    int character;
    do {
        character = fgetc(input);
    } while (character != EOF && character != '\n');
    return character == EOF && ferror(input) ? ST_FILE_READ : status;
}

static Status parse_limit(const char *line, size_t *limit)
{
    size_t result = 0;
    for (; *line != '\0'; ++line) {
        if (*line < '0' || *line > '9')
            return ST_INVALID_INPUT;
        size_t digit = (size_t)(*line - '0');
        if (result > (SIZE_MAX - digit) / 10)
            return ST_OVERFLOW;
        result = result * 10 + digit;
    }
    if (result == 0)
        return ST_INVALID_INPUT;
    *limit = result;
    return ST_OK;
}

static Status decode_word(char *word)
{
    if (*word == '\0')
        return ST_INVALID_INPUT;
    char *write = word;
    for (const char *read = word; *read != '\0'; ++read) {
        char character = *read;
        if (character == '\\') {
            switch (*++read) {
                case 'n': character = '\n'; break;
                case 'r': character = '\r'; break;
                case '\\': character = '\\'; break;
                default: return ST_INVALID_INPUT;
            }
        }
        *write++ = character;
    }
    *write = '\0';
    return ST_OK;
}

static Status print_frequency(FILE *output, const tree_t *tree, char *word)
{
    size_t count;
    Status status = decode_word(word);
    if (status == ST_OK)
        status = tree_find(tree, word, &count);
    if (status == ST_OK && fprintf(output, "Frequency: %zu\n", count) < 0)
        return ST_FILE_WRITE;
    return status;
}

static Status print_top(FILE *output, const tree_t *tree, const char *line)
{
    size_t limit;
    Status status = parse_limit(line, &limit);
    if (status != ST_OK)
        return status;
    const word_node_t **items;
    size_t count;
    status = tree_top(tree, limit, &items, &count);
    if (status != ST_OK)
        return status;
    for (size_t index = 0; index < count && status == ST_OK; ++index)
        status = print_entry(output, items[index]);
    free(items);
    if (count == 0 && fputs("No words.\n", output) == EOF)
        return ST_FILE_WRITE;
    return status;
}

static Status print_extremes(FILE *output, const tree_t *tree)
{
    const word_node_t *shortest;
    const word_node_t *longest;
    Status status = tree_extremes(tree, &shortest, &longest);
    if (status != ST_OK)
        return status;
    if (fputs("Shortest: ", output) == EOF)
        return ST_FILE_WRITE;
    status = print_entry(output, shortest);
    if (status != ST_OK)
        return status;
    if (fputs("Longest: ", output) == EOF)
        return ST_FILE_WRITE;
    return print_entry(output, longest);
}

static Status print_depth(FILE *output, const tree_t *tree)
{
    size_t depth;
    Status status = tree_depth(tree, &depth);
    if (status == ST_OK && fprintf(output, "Depth: %zu\n", depth) < 0)
        return ST_FILE_WRITE;
    return status;
}

static Status prompt(FILE *output, const char *text)
{
    if (fputs(text, output) == EOF || fflush(output) == EOF)
        return ST_FILE_WRITE;
    return ST_OK;
}

static bool is_same_file(const char *path_a, const char *path_b)
{
    struct stat first;
    struct stat second;
    if (stat(path_a, &first) != 0 || stat(path_b, &second) != 0)
        return false;

    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

static Status run_with_argument(FILE *input, FILE *output, tree_t *tree,
                                char command, const char *source_path)
{
    const char *label = command == '1' ? "Word: " :
                        command == '2' ? "Number of words: " : "File path: ";
    Status status = prompt(output, label);
    if (status != ST_OK)
        return status;

    char *argument;
    status = read_line(input, &argument);
    if (status != ST_OK)
        return status;
    if (command == '1')
        status = print_frequency(output, tree, argument);
    else if (command == '2')
        status = print_top(output, tree, argument);
    else if (*argument == '\0')
        status = ST_INVALID_INPUT;
    else if (command == '5' && is_same_file(argument, source_path)) {
        status = prompt(output, "Source and snapshot files must be different.\n");
    } else {
        status = command == '5' ? tree_save(tree, argument) :
                                  tree_load(argument, tree);
        if (status == ST_OK)
            status = prompt(output, command == '5' ? "Saved.\n" : "Loaded.\n");
        else if (status == ST_FILE_WRITE || status == ST_FILE_READ) {
            const char *message = status == ST_FILE_WRITE ?
                                  "Cannot write snapshot.\n" :
                                  "Cannot read snapshot.\n";
            status = prompt(output, message);
        }
    }
    free(argument);
    return status;
}

static Status show_feedback(FILE *output, Status status)
{
    const char *message;
    switch (status) {
        case ST_OK: return ST_OK;
        case ST_INVALID_INPUT: message = "Invalid input.\n"; break;
        case ST_OVERFLOW: message = "Value is too large.\n"; break;
        case ST_NO_MEMORY: message = "Out of memory.\n"; break;
        case ST_NOT_FOUND: message = "Word not found.\n"; break;
        case ST_EMPTY_DATA: message = "No words.\n"; break;
        case ST_FILE_OPEN: message = "Cannot open snapshot.\n"; break;
        case ST_FILE_CLOSE: message = "Cannot close snapshot.\n"; break;
        case ST_INVALID_FORMAT: message = "Invalid snapshot.\n"; break;
        default: return status;
    }
    return prompt(output, message);
}

Status run_menu(FILE *input, FILE *output, tree_t *tree, const char *source_path)
{
    if (input == NULL || output == NULL || tree == NULL ||
        source_path == NULL || *source_path == '\0')
        return ST_INVALID_ARGUMENT;

    for (;;) {
        Status status = prompt(output,
            "1 Frequency | 2 Top | 3 Extremes | 4 Depth | "
            "5 Save | 6 Load | 0 Exit\n> ");
        if (status != ST_OK)
            return status;

        char *line;
        status = read_line(input, &line);
        if (status == ST_OK) {
            char command = strlen(line) == 1 ? line[0] : '\0';
            free(line);
            switch (command) {
                case '0': return ST_OK;
                case '1': case '2': case '5': case '6':
                    status = run_with_argument(input, output, tree, command, source_path);
                    break;
                case '3': status = print_extremes(output, tree); break;
                case '4': status = print_depth(output, tree); break;
                default: status = ST_INVALID_INPUT; break;
            }
        }
        if (status == ST_END)
            return ST_OK;
        status = show_feedback(output, status);
        if (status != ST_OK)
            return status;
    }
}
