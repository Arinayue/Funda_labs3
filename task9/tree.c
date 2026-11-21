#include "tree.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Status create_node(const char *word, word_node_t *parent,
                            word_node_t **result)
{
    size_t length = strlen(word);
    if (length == SIZE_MAX)
        return ST_OVERFLOW;

    word_node_t *node = malloc(sizeof(*node));
    if (node == NULL)
        return ST_NO_MEMORY;

    char *copy = malloc(length + 1);
    if (copy == NULL) {
        free(node);
        return ST_NO_MEMORY;
    }
    memcpy(copy, word, length + 1);
    *node = (word_node_t){copy, length, 1, NULL, NULL, parent};
    *result = node;
    return ST_OK;
}

Status tree_add(tree_t *tree, const char *word)
{
    if (tree == NULL || word == NULL || *word == '\0')
        return ST_INVALID_ARGUMENT;

    word_node_t **slot = &tree->root;
    word_node_t *parent = NULL;
    while (*slot != NULL) {
        parent = *slot;
        int order = strcmp(word, parent->word);
        if (order == 0) {
            if (parent->count == SIZE_MAX)
                return ST_OVERFLOW;
            ++parent->count;
            return ST_OK;
        }
        slot = order < 0 ? &parent->left : &parent->right;
    }
    if (tree->size == SIZE_MAX)
        return ST_OVERFLOW;

    Status status = create_node(word, parent, slot);
    if (status == ST_OK)
        ++tree->size;
    return status;
}

Status tree_find(const tree_t *tree, const char *word, size_t *count)
{
    if (tree == NULL || word == NULL || *word == '\0' || count == NULL)
        return ST_INVALID_ARGUMENT;

    const word_node_t *node = tree->root;
    while (node != NULL) {
        int order = strcmp(word, node->word);
        if (order == 0) {
            *count = node->count;
            return ST_OK;
        }
        node = order < 0 ? node->left : node->right;
    }
    return ST_NOT_FOUND;
}

Status tree_destroy(tree_t *tree)
{
    if (tree == NULL)
        return ST_INVALID_ARGUMENT;

    word_node_t *node = tree->root;
    while (node != NULL) {
        if (node->left != NULL) {
            node = node->left;
        } else if (node->right != NULL) {
            node = node->right;
        } else {
            word_node_t *parent = node->parent;
            if (parent != NULL) {
                if (parent->left == node)
                    parent->left = NULL;
                else
                    parent->right = NULL;
            }
            free(node->word);
            free(node);
            node = parent;
        }
    }
    *tree = (tree_t){0};
    return ST_OK;
}

static const word_node_t *leftmost(const word_node_t *node)
{
    while (node != NULL && node->left != NULL)
        node = node->left;
    return node;
}

Status tree_first(const tree_t *tree, const word_node_t **node)
{
    if (tree == NULL || node == NULL)
        return ST_INVALID_ARGUMENT;

    *node = leftmost(tree->root);
    return *node == NULL ? ST_END : ST_OK;
}

Status tree_next(const word_node_t *node, const word_node_t **next)
{
    if (node == NULL || next == NULL)
        return ST_INVALID_ARGUMENT;

    if (node->right != NULL) {
        *next = leftmost(node->right);
        return ST_OK;
    }
    while (node->parent != NULL && node == node->parent->right)
        node = node->parent;
    *next = node->parent;
    return *next == NULL ? ST_END : ST_OK;
}

static int compare_frequency(const void *left, const void *right)
{
    const word_node_t *first = *(const word_node_t *const *)left;
    const word_node_t *second = *(const word_node_t *const *)right;
    if (first->count != second->count)
        return first->count > second->count ? -1 : 1;
    return strcmp(first->word, second->word);
}

Status tree_top(const tree_t *tree, size_t limit,
                  const word_node_t ***items, size_t *count)
{
    if (tree == NULL || limit == 0 || items == NULL || count == NULL)
        return ST_INVALID_ARGUMENT;

    if (tree->size == 0) {
        *items = NULL;
        *count = 0;
        return ST_OK;
    }
    const word_node_t **result;
    if (tree->size > SIZE_MAX / sizeof(*result))
        return ST_OVERFLOW;
    result = malloc(tree->size * sizeof(*result));
    if (result == NULL)
        return ST_NO_MEMORY;

    const word_node_t *node;
    size_t used = 0;
    Status status = tree_first(tree, &node);
    while (status == ST_OK && used < tree->size) {
        result[used++] = node;
        status = tree_next(node, &node);
    }
    if (status != ST_END || used != tree->size) {
        free(result);
        return ST_INVALID_ARGUMENT;
    }
    qsort(result, used, sizeof(*result), compare_frequency);
    *items = result;
    *count = limit < used ? limit : used;
    return ST_OK;
}

Status tree_extremes(const tree_t *tree, const word_node_t **shortest,
                       const word_node_t **longest)
{
    if (tree == NULL || shortest == NULL || longest == NULL ||
        shortest == longest)
        return ST_INVALID_ARGUMENT;

    const word_node_t *node;
    Status status = tree_first(tree, &node);
    if (status != ST_OK)
        return status == ST_END ? ST_EMPTY_DATA : status;

    const word_node_t *minimum = node;
    const word_node_t *maximum = node;
    while ((status = tree_next(node, &node)) == ST_OK) {
        if (node->length < minimum->length)
            minimum = node;
        if (node->length > maximum->length)
            maximum = node;
    }
    if (status != ST_END)
        return status;
    *shortest = minimum;
    *longest = maximum;
    return ST_OK;
}

Status tree_depth(const tree_t *tree, size_t *depth)
{
    if (tree == NULL || depth == NULL)
        return ST_INVALID_ARGUMENT;

    const word_node_t *node = tree->root;
    const word_node_t *previous = NULL;
    size_t level = 0;
    size_t maximum = 0;
    while (node != NULL) {
        const word_node_t *next;
        if (previous == node->parent) {
            if (level == SIZE_MAX)
                return ST_OVERFLOW;
            ++level;
            if (level > maximum)
                maximum = level;
            next = node->left != NULL ? node->left : node->right;
        } else {
            next = previous == node->left ? node->right : NULL;
        }
        if (next == NULL) {
            next = node->parent;
            --level;
        }
        previous = node;
        node = next;
    }
    *depth = maximum;
    return ST_OK;
}
