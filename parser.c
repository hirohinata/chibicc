#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "error.h"

static Node* primary(Token** ppToken);
static Node* unary(Token** ppToken);
static Node* mul(Token** ppToken);
static Node* expr(Token** ppToken);

static Node* new_node(NodeKind kind, Node* lhs, Node* rhs) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node* new_node_num(int val) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

static Node* primary(Token** ppToken) {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume(ppToken, "(")) {
        Node* node = expr(ppToken);
        expect(ppToken, ")");
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(ppToken));
}

static Node* unary(Token** ppToken) {
    if (consume(ppToken, "+"))
        return unary(ppToken);
    if (consume(ppToken, "-"))
        return new_node(ND_SUB, new_node_num(0), unary(ppToken));
    return primary(ppToken);
}

static Node* mul(Token** ppToken) {
    Node* node = unary(ppToken);

    for (;;) {
        if (consume(ppToken, "*"))
            node = new_node(ND_MUL, node, unary(ppToken));
        else if (consume(ppToken, "/"))
            node = new_node(ND_DIV, node, unary(ppToken));
        else
            return node;
    }
}

static Node* expr(Token** ppToken) {
    Node* node = mul(ppToken);

    for (;;) {
        if (consume(ppToken, "+"))
            node = new_node(ND_ADD, node, mul(ppToken));
        else if (consume(ppToken, "-"))
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
