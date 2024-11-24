#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "asm_gen.h"
#include "error.h"

typedef struct LVar LVar;

// ローカル変数の型
struct LVar {
    LVar* next;         // 次の変数かNULL
    const char* name;   // 変数の名前
    int len;            // 名前の長さ
    int offset;         // RBPからのオフセット
};

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static const LVar* find_lvar(const LVar* pLVarTop, const Node* pNode) {
    for (const LVar* pVar = pLVarTop; pVar; pVar = pVar->next) {
        if (pVar->len == pNode->pToken->len && !memcmp(pNode->pToken->str, pVar->name, pVar->len)) {
            return pVar;
        }
    }
    return NULL;
}

// 変数を登録し、総消費スタックサイズを返す。
static int resigter_lvars(LVar** ppLVarTop, Node* pNode) {
    if (pNode->kind == ND_LVAR &&
        find_lvar(*ppLVarTop, pNode) == NULL)
    {
        LVar* pVar = calloc(1, sizeof(LVar));
        pVar->next = *ppLVarTop;
        pVar->name = pNode->pToken->str;
        pVar->len = pNode->pToken->len;
        pVar->offset = (*ppLVarTop ? (*ppLVarTop)->offset : 0) + 8;
        *ppLVarTop = pVar;
    }

    if (pNode->lhs) resigter_lvars(ppLVarTop, pNode->lhs);
    if (pNode->rhs) resigter_lvars(ppLVarTop, pNode->rhs);

    return (*ppLVarTop ? (*ppLVarTop)->offset : 0);
}

static void gen_lval(const Node* pNode, const LVar* pLVars) {
    if (pNode->kind != ND_LVAR) {
        error("代入の左辺値が変数ではありません");
    }

    const LVar* pVar = find_lvar(pLVars, pNode);
    if (pVar == NULL) {
        error("Internal Error. Local variable is not registered.");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", pVar->offset);
    printf("  push rax\n");
}

static void gen_node(Node* pNode, const LVar* pLVars) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NUM:
        // 数値リテラル
        printf("  push %d\n", pNode->pToken->val);
        return;
    case ND_LVAR:
        // 変数
        gen_lval(pNode, pLVars);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        // 代入演算
        gen_lval(pNode->lhs, pLVars);
        gen_node(pNode->rhs, pLVars);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_STMT:
        // 文
        gen_node(pNode->lhs, pLVars);

        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");

        // 継続文があるならそれを評価
        if (pNode->rhs) gen_node(pNode->rhs, pLVars);
        return;
    }

    // 二項演算
    gen_node(pNode->lhs, pLVars);
    gen_node(pNode->rhs, pLVars);
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
    case ND_EQ:  // ==
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:  // !=
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:  // <
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:  // <=
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }

    printf("  push rax\n");
}

void gen(Node* pNode) {
    // 変数登録を行う
    LVar* pLVars = NULL;
    int stack_size = resigter_lvars(&pLVars, pNode);

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // プロローグ
    // ローカル変数が必要とする分の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_node(pNode, pLVars);

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
