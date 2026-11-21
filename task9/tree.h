#ifndef TASK9_TREE_H
#define TASK9_TREE_H

#include <stddef.h>

#include "status.h"

typedef struct word_node {
    char *word;
    size_t length;
    size_t count;
    struct word_node *left;
    struct word_node *right;
    struct word_node *parent;
} word_node_t;

typedef struct {
    word_node_t *root;
    size_t size;
} tree_t;

Status tree_add(tree_t *tree, const char *word);
Status tree_find(const tree_t *tree, const char *word, size_t *count);
Status tree_destroy(tree_t *tree);

Status tree_first(const tree_t *tree, const word_node_t **node);
Status tree_next(const word_node_t *node, const word_node_t **next);

Status tree_top(const tree_t *tree, size_t limit,
                  const word_node_t ***items, size_t *count);
Status tree_extremes(const tree_t *tree, const word_node_t **shortest,
                       const word_node_t **longest);
Status tree_depth(const tree_t *tree, size_t *depth);

#endif
