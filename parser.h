#pragma once

// Û\¶ØÌm[hÌíÞ
typedef enum {
    ND_STMT,    // ¶
    ND_ADD,     // +
    ND_SUB,     // -
    ND_MUL,     // *
    ND_DIV,     // /
    ND_EQ,      // ==
    ND_NE,      // !=
    ND_LT,      // <
    ND_LE,      // <=
    ND_ASSIGN,  // =
    ND_LVAR,    // [JÏ
    ND_NUM,     // ®
    ND_RETURN,  // return¶
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;

// Û\¶ØÌm[hÌ^
struct Node {
    NodeKind kind;          // m[hÌ^
    Node* lhs;              // ¶Ó
    Node* rhs;              // EÓ
    const Token* pToken;    // ³g[N
};

Node* parse(struct Token* pToken);
