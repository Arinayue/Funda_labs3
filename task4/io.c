#include "io.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Status string_read_line(String *str, int descriptor) {
    if (str == NULL) return ST_NULL_POINTER;
    if (descriptor < 0) return ST_INVALID_ARGUMENT;
    size_t capacity = 64;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) return ST_NO_MEMORY;
    int invalid = 0;
    for (;;) {
        char ch;
        ssize_t bytes_read = read(descriptor, &ch, 1);
        if (bytes_read < 0) {
            free(buffer);
            return ST_IO;
        }
        if (bytes_read == 0 && length == 0 && !invalid) {
            free(buffer);
            return ST_END_OF_INPUT;
        }
        if (bytes_read == 0 || ch == '\n') break;
        if (ch == '\0' || length == INT_MAX) invalid = 1;
        if (invalid) continue;
        if (length + 1 == capacity) {
            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                return ST_NO_MEMORY;
            }
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                free(buffer);
                return ST_NO_MEMORY;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        buffer[length++] = ch;
    }
    if (invalid) {
        free(buffer);
        return ST_INVALID_INPUT;
    }
    if (length > 0 && buffer[length - 1] == '\r') length--;
    buffer[length] = '\0';
    free(str->data);
    str->data = buffer;
    str->length = (int)length;
    return ST_OK;
}

Status write_text(int descriptor, const char *text) {
    if (text == NULL) return ST_NULL_POINTER;
    if (descriptor < 0) return ST_INVALID_ARGUMENT;

    size_t length = strlen(text);
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(descriptor, text + written, length - written);
        if (result <= 0) return ST_IO;
        written += (size_t)result;
    }
    return ST_OK;
}
