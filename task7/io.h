#ifndef TASK7_IO_H
#define TASK7_IO_H

#include <stdio.h>

#include "livers.h"

Status read_line(FILE *stream, char **line);
Status read_console_line(char **line);
Status parse_size(const char *text, size_t *value);
Status parse_liver(char *line, Liver *value);
Status parse_filter(char *line, LiverFilter *filter);
Status load_livers(const char *path, LiverList *list, size_t *error_line);
Status print_liver(FILE *stream, const Liver *value);
Status print_liver_to_console(const Liver *value);
Status save_livers(const char *path, const LiverList *list);

#endif
