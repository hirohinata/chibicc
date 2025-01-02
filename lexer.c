#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "error.h"

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(Token** ppToken, const char* op) {
    if ((*ppToken)->kind != TK_RESERVED ||
        strlen(op) != (*ppToken)->len ||
        memcmp((*ppToken)->str, op, (*ppToken)->len))
    {
        return false;
    }
    *ppToken = (*ppToken)->next;
    return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume_reserved_word(Token** ppToken, TokenKind token_kind) {
    if ((*ppToken)->kind != token_kind) {
        return false;
    }
    *ppToken = (*ppToken)->next;
    return true;
}

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// 識別子トークンを返す。それ以外の場合にはNULLを返す。
const Token* consume_ident(Token** ppToken) {
    if ((*ppToken)->kind != TK_IDENT) {
        return NULL;
    }

    const Token* pIdentToken = *ppToken;
    *ppToken = (*ppToken)->next;
    return pIdentToken;

}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(Token** ppToken, const char* op) {
    if ((*ppToken)->kind != TK_RESERVED ||
        strlen(op) != (*ppToken)->len ||
        memcmp((*ppToken)->str, op, (*ppToken)->len))
    {
        error_at((*ppToken)->user_input, (*ppToken)->str, "'%s'ではありません", op);
    }
    *ppToken = (*ppToken)->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token** ppToken) {
    if ((*ppToken)->kind != TK_NUM) {
        error_at((*ppToken)->user_input, (*ppToken)->str, "数ではありません");
    }
    int val = (*ppToken)->val;
    *ppToken = (*ppToken)->next;
    return val;
}

// 次のトークンがEOFなら真を返す。それ以外の場合には偽を返す。
bool at_eof(Token* pToken) {
    return pToken->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
static Token* new_token(TokenKind kind, Token* cur, const char* str, int len, const char* user_input) {
    Token* tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->user_input = user_input;
    cur->next = tok;
    return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token* tokenize(const char* user_input) {
    Token head;
    head.next = NULL;
    Token* cur = &head;

    const char* p = user_input;
    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        // 識別子
        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z') {
            const char* pEnd = p;
            do { pEnd++; } while ('a' <= *pEnd && *pEnd <= 'z' || 'A' <= *pEnd && *pEnd <= 'Z' || '0' <= *pEnd && *pEnd <= '9' || *pEnd == '_');

            if ((int)(pEnd - p) == 6 && strncmp(p, "return", 6) == 0) {
                cur = new_token(TK_RETURN, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 2 && strncmp(p, "if", 2) == 0) {
                cur = new_token(TK_IF, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "else", 4) == 0) {
                cur = new_token(TK_ELSE, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 5 && strncmp(p, "while", 5) == 0) {
                cur = new_token(TK_WHILE, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "for", 3) == 0) {
                cur = new_token(TK_FOR, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "int", 3) == 0) {
                cur = new_token(TK_INT, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 6 && strncmp(p, "sizeof", 6) == 0) {
                cur = new_token(TK_SIZEOF, cur, p, (int)(pEnd - p), user_input);
            }
            else {
                cur = new_token(TK_IDENT, cur, p, (int)(pEnd - p), user_input);
            }
            p = pEnd;
            continue;
        }

        // 一文字記号
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '{' || *p == '}' || *p == ';' || *p == ',') {
            cur = new_token(TK_RESERVED, cur, p++, 1, user_input);
            continue;
        }

        // 二文字になり得る記号
        if (*p == '=' || *p == '!' || *p == '<' || *p == '>' || *p == '&') {
            if (*(p + 1) == '=' ||
                (*(p + 1) == *p && (*p == '&'))) {
                cur = new_token(TK_RESERVED, cur, p, 2, user_input);
                p += 2;
            }
            else {
                //TODO: 現時点では論理NOT('!')は構文解析が対応していない
                cur = new_token(TK_RESERVED, cur, p++, 1, user_input);
            }
            continue;
        }

        // 数値リテラル
        if (isdigit(*p)) {
            const char* pEnd = p;
            int val = strtol(p, &pEnd, 10);

            cur = new_token(TK_NUM, cur, p, (int)(pEnd - p), user_input);
            cur->val = val;
            p = pEnd;
            continue;
        }

        error_at(user_input, p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0, user_input);
    return head.next;
}
