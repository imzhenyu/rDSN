#ifndef HPACK_H_
#define HPACK_H_

#include <stdint.h>
#include <vector>
#include "hpack_table.h"

int64_t encode_int(uint8_t* dst, uint32_t I, uint8_t N);
int64_t hpack_encode(uint8_t* buf, const std::vector<header>& headers, bool fromsTable, bool from_dTable, bool is_huffman, Table* table, int dynamic_table_size);
int64_t decode_int(uint32_t &dst, const uint8_t* buf, uint8_t N);
int64_t hpack_decode(std::vector<header>& headers, uint8_t* buf, Table* table, uint32_t length);

#endif // HPACK_H_
