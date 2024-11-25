#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "asm_gen.h"
#include "error.h"

static void gen_lval(const Node* pNode) {
    if (pNode->kind != ND_LVAR) {
        error("����̍��Ӓl���ϐ��ł͂���܂���");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", pNode->offset);
    printf("  push rax\n");
}

static void gen_node(Node* pNode) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NUM:
        // ���l���e����
        printf("  push %d\n", pNode->val);
        return;
    case ND_LVAR:
        // �ϐ�
        gen_lval(pNode);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        // ������Z
        gen_lval(pNode->lhs);
        gen_node(pNode->rhs);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_STMT:
        // ��
        gen_node(pNode->lhs);

        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");

        // �p����������Ȃ炻���]��
        if (pNode->rhs) gen_node(pNode->rhs);
        return;
    }

    // �񍀉��Z
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
    // �A�Z���u���̑O���������o��
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // �v�����[�O
    // �ϐ�26���̗̈���m�ۂ���
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, 208\n");

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_node(pNode);

    // �G�s���[�O
    // �Ō�̎��̌��ʂ�RAX�Ɏc���Ă���̂ł��ꂪ�Ԃ�l�ɂȂ�
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
