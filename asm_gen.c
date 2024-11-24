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
    case ND_NUM: // ����
        printf("  push %d\n", pNode->val);
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }

    printf("  push rax\n");
}

void gen(Node* pNode) {
    // �A�Z���u���̑O���������o��
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_node(pNode);

    // �ŏI�I�ȕ]�����ʂ�rax��pop
    printf("  pop rax\n");
    printf("  ret\n");
}
