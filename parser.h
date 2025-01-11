#pragma once

// 抽象構文木のノードの種類
typedef enum {
    ND_NOP,         // 何もしない空要素
    ND_TOP_LEVEL,   // 関数定義外のトップレベル層
    ND_DEF_FUNC,    // 関数定義
    ND_DECL_VAR,    // 変数宣言
    ND_TYPE,        // 型
    ND_BLOCK,       // ブロック
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_ASSIGN,      // =
    ND_ADDR,        // &（アドレス）
    ND_DEREF,       // *（関節参照）
    ND_SIZEOF,      // sizeof
    ND_INVOKE,      // 関数呼び出し
    ND_VAR,         // 変数
    ND_NUM,         // 整数
    ND_STRING,      // 文字列
    ND_EXPR_STMT,   // 式文
    ND_RETURN,      // return文
    ND_IF,          // if文
    ND_WHILE,       // while文
    ND_FOR,         // for文
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;
typedef struct StringLiteral StringLiteral;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;          // ノードの型
    const Node* lhs;        // 左辺
    const Node* rhs;        // 右辺
    const Node* children[4];// その他の子ノード
    const Token* pToken;    // 元トークン
};

Node* parse(Token* pToken, const StringLiteral* pStrLiterals);
