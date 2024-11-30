#pragma once

// トークンの種類
typedef enum {
    TK_RESERVED, // 記号
    TK_RETURN,   // return
    TK_IF,       // if
    TK_ELSE,     // else
    TK_WHILE,    // while
    TK_FOR,      // for
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数トークン
    TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
    TokenKind kind;             // トークンの型
    Token* next;                // 次の入力トークン
    int val;                    // kindがTK_NUMの場合、その数値
    const char* str;            // トークン文字列
    int len;                    // トークンの長さ
    const char* user_input;     // 分解元ユーザー入力文字列
};

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(Token** ppToken, const char* op);

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume_reserved_word(Token** ppToken, TokenKind token_kind);

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// 識別子トークンを返す。それ以外の場合にはNULLを返す。
const Token* consume_ident(Token** ppToken);

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(Token** ppToken, const char* op);

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token** ppToken);

// 次のトークンがEOFなら真を返す。それ以外の場合には偽を返す。
bool at_eof(Token* pToken);

// 入力文字列pをトークナイズしてそれを返す
Token* tokenize(const char* user_input);
