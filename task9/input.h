#ifndef TASK9_INPUT_H
#define TASK9_INPUT_H

#include <stdio.h>

#include "tree.h"

Status parse_args(int argc, char *const argv[], const char **path,
                    char **separators);
Status read_words(const char *path, const char *separators, tree_t *tree);
Status read_field(FILE *input, const char *separators, char **field);

#endif
