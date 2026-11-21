#include "input.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Status parse_args(int argc, char *const argv[], const char **path,
                    char **separators)
{
    if (argv == NULL || path == NULL || separators == NULL)
        return ST_INVALID_ARGUMENT;
    if (argc < 3 || argv[1] == NULL || argv[1][0] == '\0')
        return ST_INVALID_INPUT;

    for (int index = 2; index < argc; ++index) {
        if (argv[index] == NULL || argv[index][0] == '\0' ||
            argv[index][1] != '\0')
            return ST_INVALID_INPUT;
    }
    if ((uintmax_t)argc - 1 > SIZE_MAX)
        return ST_OVERFLOW;
    char *result = malloc((size_t)((uintmax_t)argc - 1));
    if (result == NULL)
        return ST_NO_MEMORY;

    for (int index = 2; index < argc; ++index)
        result[index - 2] = argv[index][0];
    result[argc - 2] = '\0';
    *path = argv[1];
    *separators = result;
    return ST_OK;
}

static Status grow_field(char **buffer, size_t *capacity)
{
    if (*capacity == SIZE_MAX)
        return ST_OVERFLOW;
    size_t next = *capacity == 0 ? 64 :
        (*capacity > SIZE_MAX / 2 ? SIZE_MAX : *capacity * 2);
    char *expanded = realloc(*buffer, next);
    if (expanded == NULL)
        return ST_NO_MEMORY;
    *buffer = expanded;
    *capacity = next;
    return ST_OK;
}

Status read_field(FILE *input, const char *separators, char **field)
{
    if (input == NULL || separators == NULL || *separators == '\0' ||
        field == NULL)
        return ST_INVALID_ARGUMENT;

    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    Status status = ST_OK;
    int character;
    while ((character = fgetc(input)) != EOF) {
        if (character == '\0') {
            status = ST_INVALID_INPUT;
            break;
        }
        if (strchr(separators, character) != NULL)
            break;
        if (capacity - length <= 1) {
            status = grow_field(&buffer, &capacity);
            if (status != ST_OK)
                break;
        }
        ((unsigned char *)buffer)[length++] = (unsigned char)character;
    }
    if (character == EOF) {
        if (ferror(input))
            status = ST_FILE_READ;
        else if (length == 0)
            status = ST_END;
    }
    if (status == ST_OK && capacity == 0)
        status = grow_field(&buffer, &capacity);
    if (status != ST_OK) {
        free(buffer);
        return status;
    }
    buffer[length] = '\0';
    *field = buffer;
    return ST_OK;
}

static Status read_word(FILE *input, const char *separators, char **word)
{
    int character;
    do {
        character = fgetc(input);
    } while (character != EOF && character != '\0' &&
             strchr(separators, character) != NULL);

    if (character == EOF)
        return ferror(input) ? ST_FILE_READ : ST_END;
    if (character == '\0')
        return ST_INVALID_INPUT;
    if (ungetc(character, input) == EOF)
        return ST_FILE_READ;
    return read_field(input, separators, word);
}

Status read_words(const char *path, const char *separators, tree_t *tree)
{
    if (path == NULL || *path == '\0' || separators == NULL ||
        *separators == '\0' || tree == NULL)
        return ST_INVALID_ARGUMENT;

    FILE *input = fopen(path, "rb");
    if (input == NULL)
        return ST_FILE_OPEN;

    char *word;
    Status status;
    while ((status = read_word(input, separators, &word)) == ST_OK) {
        status = tree_add(tree, word);
        free(word);
        if (status != ST_OK)
            break;
    }
    if (status == ST_END)
        status = ST_OK;
    if (fclose(input) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    return status;
}
