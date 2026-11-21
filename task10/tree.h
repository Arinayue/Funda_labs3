#ifndef TREE_H
#define TREE_H

#include <stddef.h>
#include "status.h"

typedef struct Node {
    char *name;
    struct Node *parent;
    struct Node *first_child;
    struct Node *last_child;
    struct Node *next_sibling;
} Node;

Status node_create(const char *name, size_t length, Node **out);
Status node_append_child(Node *parent, Node *child);
void tree_free(Node *root);

#endif