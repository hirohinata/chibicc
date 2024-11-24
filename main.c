#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの種類
typedef enum {
    TK_RESERVED, // 記号
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
    const char* user_input;     // 分解元ユーザー入力文字列
};

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告する
void error_at(const char* user_input, const char* loc, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = (int)(loc - user_input);
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(Token** ppToken, char op) {
    if ((*ppToken)->kind != TK_RESERVED || (*ppToken)->str[0] != op)
        return false;
    *ppToken = (*ppToken)->next;
    return true;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(Token** ppToken, char op) {
    if ((*ppToken)->kind != TK_RESERVED || (*ppToken)->str[0] != op)
        error_at((*ppToken)->user_input, (*ppToken)->str, "'%c'ではありません", op);
    *ppToken = (*ppToken)->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(Token** ppToken) {
    if ((*ppToken)->kind != TK_NUM)
        error_at((*ppToken)->user_input, (*ppToken)->str, "数ではありません");
    int val = (*ppToken)->val;
    *ppToken = (*ppToken)->next;
    return val;
}

bool at_eof(Token* pToken) {
    return pToken->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token* new_token(TokenKind kind, Token* cur, const char* str, const char* user_input) {
    Token* tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
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

        if (*p == '+' || *p == '-') {
            cur = new_token(TK_RESERVED, cur, p++, user_input);
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, user_input);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error_at(user_input, p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, user_input);
    return head.next;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズする
    Token* pToken = tokenize(argv[1]);

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 式の最初は数でなければならないので、それをチェックして
    // 最初のmov命令を出力
    printf("  mov rax, %d\n", expect_number(&pToken));

    // `+ <数>`あるいは`- <数>`というトークンの並びを消費しつつ
    // アセンブリを出力
    while (!at_eof(pToken)) {
        if (consume(&pToken, '+')) {
            printf("  add rax, %d\n", expect_number(&pToken));
            continue;
        }

        expect(&pToken, '-');
        printf("  sub rax, %d\n", expect_number(&pToken));
    }

    printf("  ret\n");
    return 0;
}