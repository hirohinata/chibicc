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
static Node* postfix(Token** ppToken);
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
static Node* decl_var(Token** ppToken, Node* pTypeNode, const Token* pVarNameToken);
static Node* def_func(Token** ppToken, Node* pTypeNode, const Token* pFuncNameToken);
static Node* def_func_or_var(Token** ppToken);
static Node* type(Token** ppToken);
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

    // 次のトークンが識別子ならVARノードを生成
    const Token* pIdentToken = consume_ident(ppToken);
    if (pIdentToken) {
        Node* node = calloc(1, sizeof(Node));
        node->kind = ND_VAR;
        node->pToken = pIdentToken;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number(ppToken));
}

static Node* postfix(Token** ppToken) {
    Node* pNode = primary(ppToken);

    for (;;) {
        const Token* pCurToken = *ppToken;

        if (consume(ppToken, "(")) {
            //NOTE:現状、直接的な関数呼び出しにのみ対応している
            if (pNode->kind != ND_VAR) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "非対応の関数呼び出し形式です");
            }

            Node* pInvokeNode = new_node(pNode->pToken, ND_INVOKE, NULL, NULL);
            const int maxParam = sizeof(pInvokeNode->children) / sizeof(pInvokeNode->children[0]);
            int argCount = 0;
            while (!consume(ppToken, ")")) {
                if (maxParam <= argCount) {
                    error_at((*ppToken)->user_input, (*ppToken)->str, "引数の数が%d個以上ある関数呼び出しは非対応です", maxParam);
                }
                if (0 < argCount) {
                    expect(ppToken, ",");
                }
                pInvokeNode->children[argCount++] = expr(ppToken);
            }

            pNode = pInvokeNode;
        }
        else if (consume(ppToken, "[")) {
            pNode = new_node(pCurToken, ND_ADD, pNode, expr(ppToken));
            pNode = new_node(pCurToken, ND_DEREF, pNode, NULL);
            expect(ppToken, "]");
        }
        else {
            break;
        }
    }

    return pNode;
}

static Node* unary(Token** ppToken) {
    const Token* pCurToken = *ppToken;

    if (consume(ppToken, "+"))
        return unary(ppToken);
    if (consume(ppToken, "-"))
        return new_node(pCurToken, ND_SUB, new_node_num(0), unary(ppToken));
    if (consume(ppToken, "&"))
        return new_node(pCurToken, ND_ADDR, unary(ppToken), NULL);
    if (consume(ppToken, "*"))
        return new_node(pCurToken, ND_DEREF, unary(ppToken), NULL);
    if (consume_reserved_word(ppToken, TK_SIZEOF))
        return new_node(pCurToken, ND_SIZEOF, unary(ppToken), NULL);
    return postfix(ppToken);
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
        Node* pTypeNode = type(ppToken);
        if (pTypeNode == NULL) {
            node = new_node(NULL, ND_EXPR_STMT, expr(ppToken), NULL);
        }
        else {
            const Token* pVarNameToken = consume_ident(ppToken);
            if (pVarNameToken == NULL) {
                error_at((*ppToken)->user_input, (*ppToken)->str, "変数名が必要です");
            }
            node = decl_var(ppToken, pTypeNode, pVarNameToken);
        }

        expect(ppToken, ";");
    }

    return node;
}

static Node* decl_var(Token** ppToken, Node* pTypeNode, const Token* pVarNameToken) {
    if (consume(ppToken, "[")) {
        Node* pCurNode = pTypeNode;
        while (pCurNode->rhs) pCurNode = (Node*)pCurNode->rhs;

        //NOTE:普通とは逆方向の木構造。型に対する修飾も木構造であるべきなのか？
        do {
            const Token* pToken = *ppToken;
            const int size = expect_number(ppToken);
            if (size <= 0) {
                error_at(pToken->user_input, pToken->str, "'%d' は配列のサイズとして不正です", size);
            }
            pCurNode->rhs = new_node_num(size);
            pCurNode = (Node*)pCurNode->rhs;
        } while (consume(ppToken, ","));
        expect(ppToken, "]");
    }

    return new_node(pVarNameToken, ND_DECL_VAR, pTypeNode, NULL);
}

static Node* def_func(Token** ppToken, Node* pTypeNode, const Token* pFuncNameToken) {
    Node* pDefFuncNode = new_node(pFuncNameToken, ND_DEF_FUNC, pTypeNode, NULL);

    const int maxParam = sizeof(pDefFuncNode->children) / sizeof(pDefFuncNode->children[0]);
    int argCount = 0;

    while (!consume(ppToken, ")")) {
        if (maxParam <= argCount) {
            error_at((*ppToken)->user_input, (*ppToken)->str, "引数の数が%d個以上ある関数定義は非対応です", maxParam);
        }
        if (0 < argCount) {
            expect(ppToken, ",");
        }

        Node* pTypeNode = type(ppToken);
        if (pTypeNode == NULL) {
            error_at((*ppToken)->user_input, (*ppToken)->str, "引数の型名が必要です");
        }

        const Token* pParamNameToken = consume_ident(ppToken);
        if (pParamNameToken == NULL) {
            error_at((*ppToken)->user_input, (*ppToken)->str, "引数名が必要です");
        }

        pDefFuncNode->children[argCount++] = decl_var(ppToken, pTypeNode, pParamNameToken);
    }

    expect(ppToken, "{");
    pDefFuncNode->rhs = compound_stmt(ppToken);

    return pDefFuncNode;
}

static Node* def_func_or_var(Token** ppToken) {
    Node* pTypeNode = type(ppToken);
    if (pTypeNode == NULL) {
        error_at((*ppToken)->user_input, (*ppToken)->str, "型名が必要です");
    }

    const Token* pNameToken = consume_ident(ppToken);
    if (pNameToken == NULL) {
        error_at((*ppToken)->user_input, (*ppToken)->str, "識別子が必要です");
    }

    if (consume(ppToken, "(")) {
        return def_func(ppToken, pTypeNode, pNameToken);
    }
    else {
        Node* pNode = decl_var(ppToken, pTypeNode, pNameToken);
        expect(ppToken, ";");
        return pNode;
    }
}

static Node* type(Token** ppToken) {
    Node* pTypeNode;
    Node* pCurNode;
    const Token* pTypeNameToken = *ppToken;

    if (!consume_reserved_word(ppToken, TK_INT)) {
        return NULL;
    }

    pTypeNode = new_node(pTypeNameToken, ND_TYPE, NULL, NULL);
    pCurNode = pTypeNode;

    //NOTE:普通とは逆方向の木構造。型に対する修飾も木構造であるべきなのか？
    for (;;) {
        const Token* pToken = *ppToken;
        if (consume(ppToken, "*")) {
            pCurNode->rhs = new_node(pToken, ND_DEREF, NULL, NULL);
            pCurNode = (Node*)pCurNode->rhs;
        }
        else {
            break;
        }
    }

    return pTypeNode;
}

static Node* program(Token** ppToken) {
    Node* pRoot = NULL;
    Node* pCur = NULL;

    while (!at_eof(*ppToken)) {
        Node* pNode = new_node(NULL, ND_TOP_LEVEL, def_func_or_var(ppToken), NULL);

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
