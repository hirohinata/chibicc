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
static Node* if_stmt(Token** ppToken);
static Node* while_stmt(Token** ppToken);
static Node* for_stmt(Token** ppToken);
static Node* compound_stmt(Token** ppToken);
static Node* stmt(Token** ppToken);
static Node* program(Token** ppToken);

static Node* new_node(const Token* pToken, NodeKind kind, Node* lhs, Node* rhs) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->pToken = pToken;
    return node;
}

static Node* new_node_num(int val) {
    Token* tok = calloc(1, sizeof(Token));
    tok->val = val;

    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->pToken = tok;
    return node;
}

static Node* primary(Token** ppToken) {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume(ppToken, "(")) {
        Node* node = expr(ppToken);
        expect(ppToken, ")");
        return node;
    }

    // 次のトークンが識別子ならLVARノードを生成
    const Token* pIdentToken = consume_ident(ppToken);
    if (pIdentToken) {
        Node* node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;
        node->pToken = pIdentToken;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(ppToken));
}

static Node* unary(Token** ppToken) {
    const Token* pCurToken = *ppToken;

    if (consume(ppToken, "+"))
        return unary(ppToken);
    if (consume(ppToken, "-"))
        return new_node(pCurToken, ND_SUB, new_node_num(0), unary(ppToken));
    return primary(ppToken);
}

static Node* mul(Token** ppToken) {
    Node* node = unary(ppToken);

    for (;;) {
        const Token* pCurToken = *ppToken;

        if (consume(ppToken, "*"))
            node = new_node(pCurToken, ND_MUL, node, unary(ppToken));
        else if (consume(ppToken, "/"))
            node = new_node(pCurToken, ND_DIV, node, unary(ppToken));
        else
            return node;
    }
}

static Node* add(Token** ppToken) {
    Node* node = mul(ppToken);

    for (;;) {
        const Token* pCurToken = *ppToken;

        if (consume(ppToken, "+"))
            node = new_node(pCurToken, ND_ADD, node, mul(ppToken));
        else if (consume(ppToken, "-"))
            node = new_node(pCurToken, ND_SUB, node, mul(ppToken));
        else
            return node;
    }
}

static Node* relational(Token** ppToken) {
    Node* node = add(ppToken);

    for (;;) {
        const Token* pCurToken = *ppToken;

        if (consume(ppToken, "<"))
            node = new_node(pCurToken, ND_LT, node, add(ppToken));
        else if (consume(ppToken, "<="))
            node = new_node(pCurToken, ND_LE, node, add(ppToken));
        else if (consume(ppToken, ">"))
            node = new_node(pCurToken, ND_LT, add(ppToken), node);
        else if (consume(ppToken, ">="))
            node = new_node(pCurToken, ND_LE, add(ppToken), node);
        else
            return node;
    }
}

static Node* equality(Token** ppToken) {
    Node* node = relational(ppToken);

    for (;;) {
        const Token* pCurToken = *ppToken;

        if (consume(ppToken, "=="))
            node = new_node(pCurToken, ND_EQ, node, relational(ppToken));
        else if (consume(ppToken, "!="))
            node = new_node(pCurToken, ND_NE, node, relational(ppToken));
        else
            return node;
    }
}

static Node* assign(Token** ppToken) {
    Node* node = equality(ppToken);

    const Token* pCurToken = *ppToken;
    if (consume(ppToken, "="))
        return new_node(pCurToken, ND_ASSIGN, node, assign(ppToken));
    else
        return node;
}

static Node* expr(Token** ppToken) {
    return assign(ppToken);
}

static Node* if_stmt(Token** ppToken)
{
    Node* pIfNode = NULL;
    Node* pConditionExpr = NULL;
    Node* pIfBranchStmt = NULL;
    Node* pElseBranchStmt = NULL;
    const Token* pIfToken = *(ppToken - 1);

    expect(ppToken, "(");
    pConditionExpr = expr(ppToken);
    expect(ppToken, ")");

    pIfBranchStmt = stmt(ppToken);

    if (consume_reserved_word(ppToken, TK_ELSE)) {
        pElseBranchStmt = stmt(ppToken);
    }

    pIfNode = new_node(pIfToken, ND_IF, pIfBranchStmt, pElseBranchStmt);
    pIfNode->children[0] = pConditionExpr;
    return pIfNode;
}

static Node* while_stmt(Token** ppToken)
{
    Node* pConditionExpr = NULL;
    const Token* pWhileToken = *(ppToken - 1);

    expect(ppToken, "(");
    pConditionExpr = expr(ppToken);
    expect(ppToken, ")");

    return new_node(pWhileToken, ND_WHILE, pConditionExpr, stmt(ppToken));
}

static Node* for_stmt(Token** ppToken)
{
    Node* pForExpr = NULL;
    Node* pInitExpr = NULL;
    Node* pCondExpr = NULL;
    Node* pLoopExpr = NULL;
    const Token* pForToken = *(ppToken - 1);

    expect(ppToken, "(");
    if (!consume(ppToken, ";")) {
        pInitExpr = expr(ppToken);
        expect(ppToken, ";");
    }
    if (!consume(ppToken, ";")) {
        pCondExpr = expr(ppToken);
        expect(ppToken, ";");
    }
    if (!consume(ppToken, ")")) {
        pLoopExpr = expr(ppToken);
        expect(ppToken, ")");
    }

    pForExpr = new_node(pForToken, ND_FOR, NULL, stmt(ppToken));
    pForExpr->children[0] = pInitExpr;
    pForExpr->children[1] = pCondExpr;
    pForExpr->children[2] = pLoopExpr;
    return pForExpr;
}

static Node* compound_stmt(Token** ppToken) {
    Node* pRoot = NULL;
    Node* pCur = NULL;

    while (!consume(ppToken, "}")) {
        Node* pNode = new_node(NULL, ND_BLOCK, stmt(ppToken), NULL);

        if (pRoot == NULL) {
            pRoot = pNode;
        }
        else {
            pCur->rhs = pNode;
        }
        pCur = pNode;
    }

    if (pRoot == NULL) {
        pRoot = new_node(NULL, ND_NOP, NULL, NULL);
    }

    return pRoot;
}

static Node* stmt(Token** ppToken) {
    Node* node = NULL;

    if (consume_reserved_word(ppToken, TK_RETURN)) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_RETURN;
        node->lhs = expr(ppToken);

        expect(ppToken, ";");
    }
    else if (consume_reserved_word(ppToken, TK_IF)) {
        node = if_stmt(ppToken);
    }
    else if (consume_reserved_word(ppToken, TK_WHILE)) {
        node = while_stmt(ppToken);
    }
    else if (consume_reserved_word(ppToken, TK_FOR)) {
        node = for_stmt(ppToken);
    }
    else if (consume(ppToken, "{")) {
        node = compound_stmt(ppToken);
    }
    else {
        node = new_node(NULL, ND_EXPR_STMT, expr(ppToken), NULL);
        expect(ppToken, ";");
    }

    return node;
}
static Node* program(Token** ppToken) {
    Node* pRoot = NULL;
    Node* pCur = NULL;

    while (!at_eof(*ppToken)) {
        Node* pNode = new_node(NULL, ND_BLOCK, stmt(ppToken), NULL);

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
