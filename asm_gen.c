#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "asm_gen.h"
#include "error.h"

static void gen_node(Node* pNode) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    if (pNode->kind == ND_NUM) {
        printf("  push %d\n", pNode->val);
        return;
    }

    gen_node(pNode->lhs);
    gen_node(pNode->rhs);
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

void gen(Node* pNode) {
    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_node(pNode);

    // 最終的な評価結果をraxにpop
    printf("  pop rax\n");
    printf("  ret\n");
}
