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
        error_at((*ppToken)->filename, (*ppToken)->user_input, (*ppToken)->str, "'%s'ではありません", op);
    }
    *ppToken = (*ppToken)->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token** ppToken) {
    if ((*ppToken)->kind != TK_NUM) {
        error_at((*ppToken)->filename, (*ppToken)->user_input, (*ppToken)->str, "数ではありません");
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
static Token* new_token(TokenKind kind, Token* cur, const char* str, int len, const char* filename, const char* user_input) {
    Token* tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->filename = filename;
    tok->user_input = user_input;
    cur->next = tok;
    return tok;
}

// 指定されたファイルの内容を返す
static const char* read_file(const char* path) {
    // ファイルを開く
    FILE* fp = fopen(path, "r");
    if (!fp) {
        error("cannot open %s", path);
        return NULL;
    }

    // ファイルの長さを調べる
    if (fseek(fp, 0, SEEK_END) == -1) {
        error("%s cannot fseek", path);
        return NULL;
    }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1) {
        error("%s cannot fseek", path);
        return NULL;
    }

    // ファイル内容を読み込む
    char* buf = calloc(1, size + 1);
    fread(buf, size, 1, fp);

    fclose(fp);
    return buf;
}

// 入力文字列pをトークナイズしてそれを返す
Token* tokenize(const char* filename, StringLiteral** ppStrLiterals) {
    Token head;
    head.next = NULL;
    Token* cur = &head;
    StringLiteral* pCurStrLiterals = NULL;
    int strLiteralCount = 0;

    const char* user_input = read_file(filename);
    const char* p = user_input;
    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        // 行コメントをスキップ
        if (strncmp(p, "//", 2) == 0) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            continue;
        }

        // ブロックコメントをスキップ
        if (strncmp(p, "/*", 2) == 0) {
            char* q = strstr(p + 2, "*/");
            if (!q) {
                error_at(filename, user_input, p, "コメントが閉じられていません");
            }
            p = q + 2;
            continue;
        }

        // 識別子
        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z') {
            const char* pEnd = p;
            do { pEnd++; } while ('a' <= *pEnd && *pEnd <= 'z' || 'A' <= *pEnd && *pEnd <= 'Z' || '0' <= *pEnd && *pEnd <= '9' || *pEnd == '_');

            if ((int)(pEnd - p) == 6 && strncmp(p, "return", 6) == 0) {
                cur = new_token(TK_RETURN, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 2 && strncmp(p, "if", 2) == 0) {
                cur = new_token(TK_IF, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "else", 4) == 0) {
                cur = new_token(TK_ELSE, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 5 && strncmp(p, "while", 5) == 0) {
                cur = new_token(TK_WHILE, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "for", 3) == 0) {
                cur = new_token(TK_FOR, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "char", 4) == 0) {
                cur = new_token(TK_CHAR, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "int", 3) == 0) {
                cur = new_token(TK_INT, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 6 && strncmp(p, "sizeof", 6) == 0) {
                cur = new_token(TK_SIZEOF, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else {
                cur = new_token(TK_IDENT, cur, p, (int)(pEnd - p), filename, user_input);
            }
            p = pEnd;
            continue;
        }

        // 一文字記号
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '{' || *p == '}' || *p == '[' || *p == ']' || *p == ';' || *p == ',') {
            cur = new_token(TK_RESERVED, cur, p++, 1, filename, user_input);
            continue;
        }

        // 二文字になり得る記号
        if (*p == '=' || *p == '!' || *p == '<' || *p == '>' || *p == '&') {
            if (*(p + 1) == '=' ||
                (*(p + 1) == *p && (*p == '&'))) {
                cur = new_token(TK_RESERVED, cur, p, 2, filename, user_input);
                p += 2;
            }
            else {
                //TODO: 現時点では論理NOT('!')は構文解析が対応していない
                cur = new_token(TK_RESERVED, cur, p++, 1, filename, user_input);
            }
            continue;
        }

        // 数値リテラル
        if (isdigit(*p)) {
            const char* pEnd = p;
            int val = strtol(p, &pEnd, 10);

            cur = new_token(TK_NUM, cur, p, (int)(pEnd - p), filename, user_input);
            cur->val = val;
            p = pEnd;
            continue;
        }

        // 文字列リテラル
        if (*p == '"') {
            const char* pEnd = p;
            while (*(++pEnd) != '"') {
                if (*pEnd == '\0' || *pEnd == '\n') {
                    error_at(filename, user_input, p, "文字列リテラルが閉じられていません");
                }
            }
            ++pEnd;

            if (pCurStrLiterals == NULL) {
                *ppStrLiterals = calloc(1, sizeof(StringLiteral));
                pCurStrLiterals = *ppStrLiterals;
            }
            else {
                pCurStrLiterals->pNext = calloc(1, sizeof(StringLiteral));
                pCurStrLiterals = pCurStrLiterals->pNext;
            }

            int len = (int)(pEnd - p);

            pCurStrLiterals->pszText = calloc(len + 1, sizeof(char));
            strncpy_s(pCurStrLiterals->pszText, len + 1, p, len);

            cur = new_token(TK_STRING, cur, p, (int)(pEnd - p), filename, user_input);
            cur->val = strLiteralCount++;
            p = pEnd;
            continue;

        }

        error_at(filename, user_input, p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0, filename, user_input);
    return head.next;
}
