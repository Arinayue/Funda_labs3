#ifndef TASK9_MENU_H
#define TASK9_MENU_H

#include <stdio.h>

#include "tree.h"

Status print_statistics(FILE *output, const tree_t *tree);
Status run_menu(FILE *input, FILE *output, tree_t *tree, const char *source_path);

#endif
