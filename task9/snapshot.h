#ifndef TASK9_SNAPSHOT_H
#define TASK9_SNAPSHOT_H

#include "tree.h"

Status tree_save(const tree_t *tree, const char *path);
Status tree_load(const char *path, tree_t *tree);

#endif
