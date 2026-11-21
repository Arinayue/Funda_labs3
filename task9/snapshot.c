#include "snapshot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Status write_snapshot(FILE *output, const tree_t *tree)
{
    if (fputs("BST1\n", output) == EOF)
        return ST_FILE_WRITE;

    const word_node_t *node = tree->root;
    const word_node_t *parent = NULL;
    int right = 0;
    for (;;) {
        if (node != NULL) {
            if (fprintf(output, "1 %zu %zu\n", node->count, node->length) < 0 ||
                fwrite(node->word, 1, node->length, output) != node->length ||
                fputc('\n', output) == EOF)
                return ST_FILE_WRITE;
            parent = node;
            node = node->left;
            right = 0;
            continue;
        }
        if (fputs("0\n", output) == EOF)
            return ST_FILE_WRITE;
        while (parent != NULL && right) {
            const word_node_t *child = parent;
            parent = parent->parent;
            right = parent != NULL && parent->right == child;
        }
        if (parent == NULL)
            return ST_OK;
        node = parent->right;
        right = 1;
    }
}

static Status expect_byte(FILE *input, int expected)
{
    int character = fgetc(input);
    if (character == EOF && ferror(input))
        return ST_FILE_READ;
    return character == expected ? ST_OK : ST_INVALID_FORMAT;
}

static Status read_positive(FILE *input, int delimiter, size_t *value)
{
    size_t result = 0;
    int character = fgetc(input);
    while (character >= '0' && character <= '9') {
        size_t digit = (size_t)(character - '0');
        if (result > (SIZE_MAX - digit) / 10)
            return ST_OVERFLOW;
        result = result * 10 + digit;
        character = fgetc(input);
    }
    if (character == EOF && ferror(input))
        return ST_FILE_READ;
    if (result == 0 || character != delimiter)
        return ST_INVALID_FORMAT;
    *value = result;
    return ST_OK;
}

static Status read_node(FILE *input, word_node_t *parent, word_node_t **slot)
{
    size_t count;
    size_t length;
    Status status = expect_byte(input, ' ');
    if (status == ST_OK)
        status = read_positive(input, ' ', &count);
    if (status == ST_OK)
        status = read_positive(input, '\n', &length);
    if (status != ST_OK)
        return status;
    if (length == SIZE_MAX)
        return ST_OVERFLOW;

    word_node_t *node = malloc(sizeof(*node));
    if (node == NULL)
        return ST_NO_MEMORY;
    char *word = malloc(length + 1);
    if (word == NULL) {
        free(node);
        return ST_NO_MEMORY;
    }
    if (fread(word, 1, length, input) != length)
        status = ferror(input) ? ST_FILE_READ : ST_INVALID_FORMAT;
    else if (memchr(word, '\0', length) != NULL)
        status = ST_INVALID_FORMAT;
    else
        status = expect_byte(input, '\n');
    if (status != ST_OK) {
        free(word);
        free(node);
        return status;
    }
    word[length] = '\0';
    *node = (word_node_t){word, length, count, NULL, NULL, parent};
    *slot = node;
    return ST_OK;
}

static Status validate_order(const tree_t *tree)
{
    const word_node_t *node;
    const word_node_t *previous = NULL;
    Status status = tree_first(tree, &node);
    while (status == ST_OK) {
        if (previous != NULL && strcmp(previous->word, node->word) >= 0)
            return ST_INVALID_FORMAT;
        previous = node;
        status = tree_next(node, &node);
    }
    return status == ST_END ? ST_OK : status;
}

static Status read_snapshot(FILE *input, tree_t *tree)
{
    for (const char *byte = "BST1\n"; *byte != '\0'; ++byte) {
        Status status = expect_byte(input, *byte);
        if (status != ST_OK)
            return status;
    }
    word_node_t **slot = &tree->root;
    word_node_t *parent = NULL;
    do {
        int marker = fgetc(input);
        if (marker == '1') {
            if (tree->size == SIZE_MAX)
                return ST_OVERFLOW;
            Status status = read_node(input, parent, slot);
            if (status != ST_OK)
                return status;
            ++tree->size;
            parent = *slot;
            slot = &parent->left;
        } else if (marker == '0') {
            Status status = expect_byte(input, '\n');
            if (status != ST_OK)
                return status;
            while (parent != NULL && slot == &parent->right) {
                word_node_t *child = parent;
                parent = parent->parent;
                if (parent != NULL)
                    slot = parent->left == child ? &parent->left : &parent->right;
            }
            if (parent != NULL)
                slot = &parent->right;
        } else {
            return marker == EOF && ferror(input) ?
                ST_FILE_READ : ST_INVALID_FORMAT;
        }
    } while (parent != NULL);

    Status status = expect_byte(input, EOF);
    return status == ST_OK ? validate_order(tree) : status;
}

Status tree_save(const tree_t *tree, const char *path)
{
    if (tree == NULL || path == NULL || *path == '\0')
        return ST_INVALID_ARGUMENT;

    FILE *output = fopen(path, "wb");
    if (output == NULL)
        return ST_FILE_OPEN;
    Status status = write_snapshot(output, tree);
    if (fclose(output) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    return status;
}

Status tree_load(const char *path, tree_t *tree)
{
    if (path == NULL || *path == '\0' || tree == NULL)
        return ST_INVALID_ARGUMENT;

    FILE *input = fopen(path, "rb");
    if (input == NULL)
        return ST_FILE_OPEN;
    tree_t replacement = {0};
    Status status = read_snapshot(input, &replacement);
    if (fclose(input) != 0 && status == ST_OK)
        status = ST_FILE_CLOSE;
    if (status == ST_OK) {
        tree_t previous = *tree;
        *tree = replacement;
        replacement = previous;
    }
    Status cleanup = tree_destroy(&replacement);
    return status == ST_OK ? cleanup : status;
}
