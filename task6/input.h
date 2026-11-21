#ifndef TASK6_INPUT_H
#define TASK6_INPUT_H

#include "transport.h"

Status load_stop_file(const char *path, transport_t **head, size_t *error_line);

#endif
