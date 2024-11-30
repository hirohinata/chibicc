#pragma once

// 抽象構文木のノードの種類
typedef enum {
    ND_STMT,        // 文
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_ASSIGN,      // =
    ND_LVAR,        // ローカル変数
    ND_NUM,         // 整数
    ND_EXPR_STMT,   // 式文
    ND_RETURN,      // return文
    ND_IF,          // if文
    ND_WHILE,       // while文
    ND_FOR,         // for文
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;          // ノードの型
    Node* lhs;              // 左辺
    Node* rhs;              // 右辺
    Node* children[4];      // その他の子ノード
    const Token* pToken;    // 元トークン
};

Node* parse(struct Token* pToken);
