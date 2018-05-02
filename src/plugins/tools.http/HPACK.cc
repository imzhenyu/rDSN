#include <stdint.h>
#include <string>
#include "HPACK.h"
#include <string.h>

int64_t
encode_int(uint8_t* dst, uint32_t I, uint8_t N) {
    if (I < (1 << N)-1) {
        *dst = I;
        return 1;
    }

    I -= (1 << N)-1;
    *dst =  (1 << N) - 1;
    uint64_t i = 1;
    for (; I >= 128; i++) {
        *(dst+i) = (I & 0x7f) | 0x80;
        I = I >> 7;
    }
    *(dst+(i++)) = I;

    return i;
}

int64_t
hpack_encode(uint8_t* buf, const std::vector<header>& headers, bool from_sTable, bool from_dTable, bool is_huffman, Table* table, int dynamic_table_size) {
    int64_t len;
    int64_t cursor = 0;
    uint8_t intRep[10];
    if (dynamic_table_size != -1 && table->dynamic_table_size != dynamic_table_size) {
        uint8_t d_table_size[10];
        len = encode_int(d_table_size, dynamic_table_size, 5);
        *d_table_size |= 0x20;
        memcpy(buf, d_table_size, len);
        cursor += len;
    }
    for (header h : headers) {
        int index = 0;
        bool match = table->find_header(index, h);
        if (from_sTable && match) {
            if (from_dTable) {
                len = encode_int(intRep, index, 7);
                *intRep |= 0x80;
                memcpy(buf+cursor, intRep, len);
            } else {
                len = encode_int(intRep, index, 4);
                memcpy(buf+cursor, intRep, len);
                cursor += len;
                memcpy(buf+cursor, intRep, len);
                len = table->pack_string(buf+cursor, h.second, is_huffman);
            }
        } else if (from_sTable && !match && index > 0) {
            if (from_dTable) {
                len = encode_int(intRep, index, 6);
                *intRep |= 0x40;
                memcpy(buf+cursor, intRep, len);
                cursor += len;
                table->add_header(h);
            } else {
                len = encode_int(intRep, index, 4);
                memcpy(buf+cursor, intRep, len);
                cursor += len;
            }
            len = table->pack_string(buf+cursor, h.second, is_huffman);
        } else {
            uint8_t prefix = 0x00; // if buf is initialized by ZERO, no need.
            if (from_dTable) {
                prefix = 0x40;
                table->add_header(h);
            }
            *(buf+(cursor++)) = prefix;
            len = table->pack_string(buf+cursor, h.first, is_huffman);
            cursor += len;
            len = table->pack_string(buf+cursor, h.second, is_huffman);
        }
        cursor += len;
    }
    return cursor;
}


int64_t
decode_int(uint32_t &dst, const uint8_t* buf, uint8_t N) {
    int64_t len = 1;
    dst = *buf & ((1 << N) - 1);
    if (dst == (1 << N) -1) {
        int M = 0;
        do {
            dst += (*(buf+len) & 0x7f) << M;
            M += 7;
        }
        while (*(buf+(len++)) & 0x80);
    }

    return len;
}

int64_t
hpack_decode(std::vector<header>& headers, uint8_t* buf, Table* table, uint32_t length) {
    int64_t cursor = 0;
    while (cursor < length) {
        bool isIndexed = false;
        uint32_t index;
        int64_t len = 0;
        if ((*(buf+cursor) & 0xe0) == 0x20) {
            // 7/3 Header table Size Update
            uint32_t dst;
            len = decode_int(dst, (buf+cursor), 5);
            cursor += len;
            table->set_dynamic_table_size(dst);
        }

        uint8_t nLen = 0;
        if ((*(buf+cursor) & 0x80) > 0)  {
            // 7.1 Indexwd Header Field
            if ((*(buf+cursor) & 0x7f) == 0) {
                // error
            }
            nLen = 7;
            isIndexed = true;
        } else {
            if ((*(buf+cursor) & 0xc0) == 0x40) {
                // 7.2.1 Literal Header Field with Incremental Indexing
                nLen = 6;
            } else {
                // when buf[cursor]&0xf0 == 0xf0
                // 7.2.2 Literal Header Field without Indexing
                // else
                // 7.2.3 Literal Header Field never Indexed
                nLen = 4;
            }
        }
        len = decode_int(index, buf+cursor, nLen);
        cursor += len;
        header h;
        len = table->parse_header(h, index, buf+cursor, isIndexed);
        cursor += len;
        if (nLen == 6) {
            table->add_header(h);
        }
        headers.push_back(h);
    }
    return cursor;
}
