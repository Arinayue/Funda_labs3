#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func.h"

#define READ_BUFFER_SIZE 4096

Status count_frequencies(const char *path, unsigned long long frequencies[BYTE_VALUES],
                         unsigned long long *file_size)
{
    FILE *file;
    unsigned char buffer[READ_BUFFER_SIZE];
    size_t read_count;
    size_t i;
    unsigned long long total = 0;

    if (path == NULL || frequencies == NULL || file_size == NULL) {
        return ST_NULL_POINTER;
    }
    if (path[0] == '\0') {
        return ST_INVALID_ARGUMENT;
    }
    memset(frequencies, 0, BYTE_VALUES * sizeof(*frequencies));
    *file_size = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return ST_FILE_OPEN;
    }
    while ((read_count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (read_count > ULLONG_MAX - total) {
            if (fclose(file) != 0) {
                return ST_FILE_CLOSE;
            }
            return ST_OVERFLOW;
        }
        for (i = 0; i < read_count; i++) {
            frequencies[buffer[i]]++;
        }
        total += (unsigned long long)read_count;
    }
    if (ferror(file)) {
        if (fclose(file) != 0) {
            return ST_FILE_CLOSE;
        }
        return ST_FILE_READ;
    }
    if (fclose(file) != 0) {
        return ST_FILE_CLOSE;
    }
    *file_size = total;
    return ST_OK;
}

static Status merge_two_lightest(MinHeap *heap)
{
    HuffmanNode *first = NULL;
    HuffmanNode *second = NULL;
    HuffmanNode *parent = NULL;
    Status st;

    st = heap_pop(heap, &first);
    if (st != ST_OK) {
        return st;
    }
    st = heap_pop(heap, &second);
    if (st != ST_OK) {
        tree_free(first);
        return st;
    }
    st = node_create(first->weight + second->weight, -1, first, second, &parent);
    if (st != ST_OK) {
        tree_free(first);
        tree_free(second);
        return st;
    }
    st = heap_push(heap, parent);
    if (st != ST_OK) {
        tree_free(parent);
    }
    return st;
}

Status build_huffman_tree(const unsigned long long frequencies[BYTE_VALUES],
                          HuffmanNode **root)
{
    MinHeap *heap = NULL;
    HuffmanNode *leaf = NULL;
    size_t distinct = 0;
    unsigned long long total_frequency = 0;
    int symbol;
    Status st = ST_OK;

    if (frequencies == NULL || root == NULL) {
        return ST_NULL_POINTER;
    }
    *root = NULL;
    for (symbol = 0; symbol < BYTE_VALUES; symbol++) {
        if (frequencies[symbol] > 0) {
            if (frequencies[symbol] > ULLONG_MAX - total_frequency) {
                return ST_OVERFLOW;
            }
            total_frequency += frequencies[symbol];
            distinct++;
        }
    }
    if (distinct == 0) {
        return ST_OK;
    }
    st = heap_create(distinct, &heap);
    if (st != ST_OK) {
        return st;
    }
    for (symbol = 0; symbol < BYTE_VALUES && st == ST_OK; symbol++) {
        if (frequencies[symbol] == 0) {
            continue;
        }
        st = node_create(frequencies[symbol], symbol, NULL, NULL, &leaf);
        if (st == ST_OK) {
            st = heap_push(heap, leaf);
            if (st != ST_OK) {
                tree_free(leaf);
            }
        }
    }
    while (st == ST_OK && heap_size(heap) > 1) {
        st = merge_two_lightest(heap);
    }
    if (st == ST_OK) {
        st = heap_pop(heap, root);
    }
    heap_release_nodes(heap);
    heap_destroy(heap);
    return st;
}

static Status assign_codes(const HuffmanNode *node, char *path, size_t depth,
                           char *codes[BYTE_VALUES])
{
    Status st;
    char *copy;

    if (node_is_leaf(node)) {
        if (node->symbol < 0 || node->symbol >= BYTE_VALUES) {
            return ST_INVALID_ARGUMENT;
        }
        copy = malloc(depth + 1);
        if (copy == NULL) {
            return ST_NO_MEMORY;
        }
        memcpy(copy, path, depth);
        copy[depth] = '\0';
        codes[node->symbol] = copy;
        return ST_OK;
    }
    if (depth >= MAX_CODE_LENGTH) {
        return ST_CODE_TOO_LONG;
    }
    st = ST_OK;
    if (node->left != NULL) {
        path[depth] = '0';
        st = assign_codes(node->left, path, depth + 1, codes);
    }
    if (st == ST_OK && node->right != NULL) {
        path[depth] = '1';
        st = assign_codes(node->right, path, depth + 1, codes);
    }
    return st;
}

Status build_codes(const HuffmanNode *root, char *codes[BYTE_VALUES])
{
    char path[MAX_CODE_LENGTH + 1];
    char *copy;
    int i;

    if (codes == NULL) {
        return ST_NULL_POINTER;
    }
    for (i = 0; i < BYTE_VALUES; i++) {
        codes[i] = NULL;
    }
    if (root == NULL) {
        return ST_OK;
    }
    if (node_is_leaf(root)) {
        if (root->symbol < 0 || root->symbol >= BYTE_VALUES) {
            return ST_INVALID_ARGUMENT;
        }
        copy = malloc(2);
        if (copy == NULL) {
            return ST_NO_MEMORY;
        }
        copy[0] = '0';
        copy[1] = '\0';
        codes[root->symbol] = copy;
        return ST_OK;
    }
    return assign_codes(root, path, 0, codes);
}

Status compute_stats(const unsigned long long frequencies[BYTE_VALUES],
                     char *const codes[BYTE_VALUES], unsigned long long file_size,
                     CompressionStats *stats)
{
    unsigned long long bits = 0;
    unsigned long long length;
    int i;

    if (frequencies == NULL || codes == NULL || stats == NULL) {
        return ST_NULL_POINTER;
    }
    for (i = 0; i < BYTE_VALUES; i++) {
        if (frequencies[i] == 0) {
            continue;
        }
        if (codes[i] == NULL) {
            return ST_INVALID_ARGUMENT;
        }
        length = (unsigned long long)strlen(codes[i]);
        if (length == 0 || frequencies[i] > (ULLONG_MAX - bits) / length) {
            return ST_OVERFLOW;
        }
        bits += frequencies[i] * length;
    }
    stats->original_bytes = file_size;
    stats->encoded_bits = bits;
    stats->encoded_bytes = bits / 8 + (bits % 8 != 0 ? 1 : 0);
    if (file_size == 0) {
        stats->savings_percent = 0.0;
    } else {
        stats->savings_percent =
            100.0 * (1.0 - (double)stats->encoded_bytes / (double)file_size);
    }
    return ST_OK;
}

void free_codes(char *codes[BYTE_VALUES])
{
    int i;

    if (codes == NULL) {
        return;
    }
    for (i = 0; i < BYTE_VALUES; i++) {
        free(codes[i]);
        codes[i] = NULL;
    }
}