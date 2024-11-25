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
static Node* add(Token** ppToken);
static Node* relational(Token** ppToken);
static Node* equality(Token** ppToken);
static Node* assign(Token** ppToken);
static Node* expr(Token** ppToken);
static Node* stmt(Token** ppToken);
static Node* program(Token** ppToken);

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
    // ���̃g�[�N����"("�Ȃ�A"(" expr ")"�̂͂�
    if (consume(ppToken, "(")) {
        Node* node = expr(ppToken);
        expect(ppToken, ")");
        return node;
    }

    // ���̃g�[�N�������ʎq�Ȃ�LVAR�m�[�h�𐶐�
    Token* pIdentToken = consume_ident(ppToken);
    if (pIdentToken) {
        Node* node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;
        node->offset = (pIdentToken->str[0] - 'a' + 1) * 8;
        return node;
    }

    // �����łȂ���ΐ��l�̂͂�
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

static Node* add(Token** ppToken) {
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

static Node* relational(Token** ppToken) {
    Node* node = add(ppToken);

    for (;;) {
        if (consume(ppToken, "<"))
            node = new_node(ND_LT, node, add(ppToken));
        else if (consume(ppToken, "<="))
            node = new_node(ND_LE, node, add(ppToken));
        else if (consume(ppToken, ">"))
            node = new_node(ND_LT, add(ppToken), node);
        else if (consume(ppToken, ">="))
            node = new_node(ND_LE, add(ppToken), node);
        else
            return node;
    }
}

static Node* equality(Token** ppToken) {
    Node* node = relational(ppToken);

    for (;;) {
        if (consume(ppToken, "=="))
            node = new_node(ND_EQ, node, relational(ppToken));
        else if (consume(ppToken, "!="))
            node = new_node(ND_NE, node, relational(ppToken));
        else
            return node;
    }
}

static Node* assign(Token** ppToken) {
    Node* node = equality(ppToken);
    if (consume(ppToken, "="))
        node = new_node(ND_ASSIGN, node, assign(ppToken));
    else
        return node;
}

static Node* expr(Token** ppToken) {
    return assign(ppToken);
}

static Node* stmt(Token** ppToken) {
    Node* node = expr(ppToken);
    expect(ppToken, ";");
    return node;
}
static Node* program(Token** ppToken) {
    Node* pRoot = NULL;
    Node* pCur = NULL;

    while (!at_eof(*ppToken)) {
        Node* pNode = new_node(ND_STMT, stmt(ppToken), NULL);

        if (pRoot == NULL) {
            pRoot = pNode;
        }
        else {
            pCur->rhs = pNode;
        }
        pCur = pNode;
    }

    return pRoot;
}

Node* parse(Token* pToken) {
    return program(&pToken);
}
