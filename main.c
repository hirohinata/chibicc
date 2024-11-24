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

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NUM, // 整数
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind; // ノードの型
    Node* lhs;     // 左辺
    Node* rhs;     // 右辺
    int val;       // kindがND_NUMの場合のみ使う
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

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
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

Node* new_node(NodeKind kind, Node* lhs, Node* rhs) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node* new_node_num(int val) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

Node* expr(Token** ppToken);

Node* primary(Token** ppToken) {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume(ppToken, '(')) {
        Node* node = expr(ppToken);
        expect(ppToken, ')');
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(ppToken));
}

Node* unary(Token** ppToken) {
    if (consume(ppToken, '+'))
        return unary(ppToken);
    if (consume(ppToken, '-'))
        return new_node(ND_SUB, new_node_num(0), unary(ppToken));
    return primary(ppToken);
}

Node* mul(Token** ppToken) {
    Node* node = unary(ppToken);

    for (;;) {
        if (consume(ppToken, '*'))
            node = new_node(ND_MUL, node, unary(ppToken));
        else if (consume(ppToken, '/'))
            node = new_node(ND_DIV, node, unary(ppToken));
        else
            return node;
    }
}

Node* expr(Token** ppToken) {
    Node* node = mul(ppToken);

    for (;;) {
        if (consume(ppToken, '+'))
            node = new_node(ND_ADD, node, mul(ppToken));
        else if (consume(ppToken, '-'))
            node = new_node(ND_SUB, node, mul(ppToken));
        else
            return node;
    }
}

Node* parse(Token* pToken) {
    Node* pNode = expr(&pToken);

    if (!at_eof(pToken)) {
        error_at(pToken->user_input, pToken->str, "構文解釈できない字句 '%s' が残りました", pToken->str);
    }

    return pNode;
}

void gen(Node* pNode) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    if (pNode->kind == ND_NUM) {
        printf("  push %d\n", pNode->val);
        return;
    }

    gen(pNode->lhs);
    gen(pNode->rhs);
    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (pNode->kind) {
    case ND_ADD: // +
        printf("  add rax, rdi\n");
        break;
    case ND_SUB: // -
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL: // *
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV: // /
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_NUM: // 整数
        printf("  push %d\n", pNode->val);
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }

    printf("  push rax\n");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズする
    Token* pToken = tokenize(argv[1]);

    // 構文木を作成する
    Node* pNode = parse(pToken);

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 構文木からアセンブリを出力
    gen(pNode);

    // 最終的な評価結果をraxにpop
    printf("  pop rax\n");
    printf("  ret\n");
    return 0;
}