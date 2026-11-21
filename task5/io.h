#ifndef TASK5_IO_H
#define TASK5_IO_H

#include <stdbool.h>
#include <stdio.h>

#include "students.h"

Status read_line(FILE *stream, char **line);
Status parse_id(const char *text, uintmax_t *id);
Status parse_text_key(char *line, const char **key);
Status load_students(const char *path, StudentArray *students, size_t *error_line);
Status print_students(FILE *stream, const StudentArray *students);
Status print_matches(FILE *stream, const StudentMatches *matches);
Status append_trace_report(const char *path, const StudentMatches *matches,
                             bool include_details);

#endif
