#ifndef HPACK_HUFFMAN_H_
#define HPACK_HUFFMAN_H_

#define NULL_NODE (Node*)0
#include <stdint.h>
#include <string>

typedef struct {
    uint32_t code;
    uint8_t bitLen;
} huffman_code;

struct Node {
    Node(Node* lp, Node* rp, uint32_t c) : left(lp), right(rp), code(c) {};
    ~Node() {};
    Node *left, *right;
    int16_t code;
};

class HuffmanTree {
    Node* root;
    void delete_node(Node* node);
public:
    HuffmanTree();
    ~HuffmanTree(); // delete alocated tree
    int64_t encode(uint8_t* buf, std::string content);
    std::string decode(const uint8_t* buf, uint32_t len);
};

#endif // HPACK_HUFFMAN_H_
