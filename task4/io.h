#ifndef TASK4_IO_H
#define TASK4_IO_H

#include "task_string.h"

Status string_read_line(String *str, int descriptor);
Status write_text(int descriptor, const char *text);

#endif
