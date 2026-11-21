#ifndef FUNC_H
#define FUNC_H

#include "min_heap.h"
#include "status.h"

#define BYTE_VALUES 256
#define MAX_CODE_LENGTH 256

typedef struct {
    unsigned long long original_bytes;
    unsigned long long encoded_bits;
    unsigned long long encoded_bytes;
    double savings_percent;
} CompressionStats;

Status count_frequencies(const char *path, unsigned long long frequencies[BYTE_VALUES],
                         unsigned long long *file_size);
Status build_huffman_tree(const unsigned long long frequencies[BYTE_VALUES],
                          HuffmanNode **root);
Status build_codes(const HuffmanNode *root, char *codes[BYTE_VALUES]);
Status compute_stats(const unsigned long long frequencies[BYTE_VALUES],
                     char *const codes[BYTE_VALUES], unsigned long long file_size,
                     CompressionStats *stats);
void free_codes(char *codes[BYTE_VALUES]);

#endif