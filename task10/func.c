#include "func.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef enum {
    STATE_EXPECT_NAME,
    STATE_AFTER_NAME,
    STATE_AFTER_CLOSE
} parse_state;

static bool is_name_char(unsigned char c)
{
    return !isspace(c) && c != '(' && c != ')' && c != ',';
}

static bool is_same_file(const char *path_a, const char *path_b) {
    struct stat sa, sb;

    if (stat(path_a, &sa) != 0 || stat(path_b, &sb) != 0) {
        return false; 
    }

    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static Status stack_push(Node ***stack, size_t *size, size_t *capacity, Node *node)
{
    if (*size == *capacity) {
        size_t new_capacity = 16;
        if (*capacity != 0) {
            if (*capacity > SIZE_MAX / 2) {
                return ST_OVERFLOW;
            }
            new_capacity = *capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(Node *)) {
            return ST_OVERFLOW;
        }
        Node **tmp = realloc(*stack, new_capacity * sizeof(Node *));
        if (tmp == NULL) {
            return ST_NO_MEMORY;
        }
        *stack = tmp;
        *capacity = new_capacity;
    }
    (*stack)[(*size)++] = node;
    return ST_OK;
}

Status parse_bracket_expression(const char *expression, Node **root)
{
    if (expression == NULL || root == NULL) {
        return ST_INVALID_ARGUMENT;
    }
    *root = NULL;

    Node *result = NULL;
    Node *last = NULL;
    Node **stack = NULL;
    size_t size = 0;
    size_t capacity = 0;
    parse_state state = STATE_EXPECT_NAME;
    Status st = ST_OK;
    bool has_content = false;
    const char *p = expression;

    while (*p != '\0' && st == ST_OK) {
        unsigned char c = (unsigned char)*p;

        if (isspace(c)) {
            p++;
            continue;
        }
        has_content = true;

        if (c == '(') {
            if (state != STATE_AFTER_NAME) {
                st = ST_INVALID_SYNTAX;
            } else {
                st = stack_push(&stack, &size, &capacity, last);
                state = STATE_EXPECT_NAME;
                p++;
            }
        } else if (c == ',') {
            if (state == STATE_EXPECT_NAME || size == 0) {
                st = ST_INVALID_SYNTAX;
            } else {
                state = STATE_EXPECT_NAME;
                p++;
            }
        } else if (c == ')') {
            if (state == STATE_EXPECT_NAME || size == 0) {
                st = ST_INVALID_SYNTAX;
            } else {
                size--;
                state = STATE_AFTER_CLOSE;
                p++;
            }
        } else {
            if (state != STATE_EXPECT_NAME) {
                st = ST_INVALID_SYNTAX;
            } else {
                const char *start = p;
                while (*p != '\0' && is_name_char((unsigned char)*p)) {
                    p++;
                }
                Node *node = NULL;
                st = node_create(start, (size_t)(p - start), &node);
                if (st == ST_OK) {
                    if (size == 0) {
                        result = node;
                    } else {
                        node_append_child(stack[size - 1], node);
                    }
                    last = node;
                    state = STATE_AFTER_NAME;
                }
            }
        }
    }

    if (st == ST_OK) {
        if (!has_content) {
            st = ST_EMPTY_LINE;
        } else if (state == STATE_EXPECT_NAME || size != 0) {
            st = ST_INVALID_SYNTAX;
        }
    }

    free(stack);

    if (st != ST_OK) {
        tree_free(result);
        return st;
    }

    *root = result;
    return ST_OK;
}

Status render_tree(FILE *out, const Node *root)
{
    if (out == NULL || root == NULL) {
        return ST_INVALID_ARGUMENT;
    }

    char *prefix = NULL;
    size_t capacity = 0;
    const Node *cur = root;
    size_t depth = 0;

    while (cur != NULL) {
        size_t length = (depth >= 2) ? (depth - 1) * 4 : 0;

        if (length + 1 > capacity) {
            size_t new_capacity = (length + 1) * 2;
            char *tmp = realloc(prefix, new_capacity);
            if (tmp == NULL) {
                free(prefix);
                return ST_NO_MEMORY;
            }
            prefix = tmp;
            capacity = new_capacity;
        }

        if (depth >= 2) {
            const Node *ancestor = cur->parent;
            for (size_t level = depth - 1; level >= 1; level--) {
                memcpy(prefix + (level - 1) * 4,
                       ancestor->next_sibling != NULL ? "|   " : "    ", 4);
                ancestor = ancestor->parent;
            }
        }
        prefix[length] = '\0';

        int written;
        if (depth == 0) {
            written = fprintf(out, "%s\n", cur->name);
        } else {
            written = fprintf(out, "%s%s%s\n", prefix,
                              cur->next_sibling != NULL ? "|-- " : "`-- ",
                              cur->name);
        }
        if (written < 0) {
            free(prefix);
            return ST_FILE_WRITE;
        }

        if (cur->first_child != NULL) {
            cur = cur->first_child;
            depth++;
        } else {
            while (cur != root && cur->next_sibling == NULL) {
                cur = cur->parent;
                depth--;
            }
            if (cur == root) {
                break;
            }
            cur = cur->next_sibling;
        }
    }

    free(prefix);
    return ST_OK;
}

static Status read_line(FILE *in, char **line, bool *eof)
{
    size_t capacity = 128;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return ST_NO_MEMORY;
    }

    int ch;
    while ((ch = fgetc(in)) != EOF && ch != '\n') {
        if (length + 2 > capacity) {
            char *tmp = realloc(buffer, capacity * 2);
            if (tmp == NULL) {
                free(buffer);
                return ST_NO_MEMORY;
            }
            buffer = tmp;
            capacity *= 2;
        }
        buffer[length++] = (char)ch;
    }

    if (ch == EOF && ferror(in)) {
        free(buffer);
        return ST_FILE_READ;
    }

    buffer[length] = '\0';
    *eof = (ch == EOF && length == 0);
    *line = buffer;
    return ST_OK;
}

static Status process_stream(FILE *in, FILE *out)
{
    size_t number = 0;

    while (1) {
        char *line = NULL;
        bool eof = false;
        Status st = read_line(in, &line, &eof);
        if (st != ST_OK) {
            return st;
        }
        if (eof) {
            free(line);
            return ST_OK;
        }

        number++;

        Node *root = NULL;
        st = parse_bracket_expression(line, &root);
        free(line);

        if (st == ST_EMPTY_LINE) {
            continue;
        }

        if (st == ST_INVALID_SYNTAX) {
            if (fprintf(out, "Expression #%zu: syntax error\n\n", number) < 0) {
                return ST_FILE_WRITE;
            }
            continue;
        }

        if (st != ST_OK) {
            return st;
        }

        if (fprintf(out, "Expression #%zu:\n", number) < 0) {
            tree_free(root);
            return ST_FILE_WRITE;
        }

        st = render_tree(out, root);
        tree_free(root);
        if (st != ST_OK) {
            return st;
        }

        if (fputc('\n', out) == EOF) {
            return ST_FILE_WRITE;
        }
    }
}

Status process_file(const char *input_path, const char *output_path)
{
    if (input_path == NULL || output_path == NULL ||
        is_same_file(input_path, output_path)) {
        return ST_INVALID_ARGUMENT;
    }

    FILE *in = fopen(input_path, "r");
    if (in == NULL) {
        return ST_FILE_OPEN;
    }

    FILE *out = fopen(output_path, "w");
    if (out == NULL) {
        Status close_status = fclose(in) == 0 ? ST_OK : ST_FILE_READ;
        return close_status == ST_OK ? ST_FILE_OPEN : close_status;
    }

    Status st = process_stream(in, out);

    if (fclose(in) != 0 && st == ST_OK) {
        st = ST_FILE_READ;
    }
    if (fclose(out) != 0 && st == ST_OK) {
        st = ST_FILE_WRITE;
    }

    return st;
}