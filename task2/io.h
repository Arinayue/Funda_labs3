#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stddef.h>
#include "status.h"
#include "func.h"

Status parse_time(const char *text, int *minutes);
Status read_talks(FILE *in, talk_t **talks, size_t *n, size_t *halls);

#endif