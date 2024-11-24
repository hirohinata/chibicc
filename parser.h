#pragma once

// Û\¶ØÌm[hÌíÞ
typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_EQ,  // ==
    ND_NE,  // !=
    ND_LT,  // <
    ND_LE,  // <=
    ND_NUM, // ®
} NodeKind;

typedef struct Node Node;

// Û\¶ØÌm[hÌ^
struct Node {
    NodeKind kind; // m[hÌ^
    Node* lhs;     // ¶Ó
    Node* rhs;     // EÓ
    int val;       // kindªND_NUMÌêÌÝg¤
};

Node* parse(struct Token* pToken);
