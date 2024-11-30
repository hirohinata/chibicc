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

// ���[�J���ϐ��̌^
struct LVar {
    LVar* next;         // ���̕ϐ���NULL
    const char* name;   // �ϐ��̖��O
    int len;            // ���O�̒���
    int offset;         // RBP����̃I�t�Z�b�g
};

static void gen_lval(const Node* pNode, const LVar* pLVars);
static void gen_if_stmt(Node * pNode, const LVar * pLVars, int* pLabelCount);
static void gen_node(Node* pNode, const LVar* pLVars, int* pLabelCount);

// �ϐ��𖼑O�Ō�������B������Ȃ������ꍇ��NULL��Ԃ��B
static const LVar* find_lvar(const LVar* pLVarTop, const Node* pNode) {
    for (const LVar* pVar = pLVarTop; pVar; pVar = pVar->next) {
        if (pVar->len == pNode->pToken->len && !memcmp(pNode->pToken->str, pVar->name, pVar->len)) {
            return pVar;
        }
    }
    return NULL;
}

// �ϐ���o�^���A������X�^�b�N�T�C�Y��Ԃ��B
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
        error("����̍��Ӓl���ϐ��ł͂���܂���");
    }

    const LVar* pVar = find_lvar(pLVars, pNode);
    if (pVar == NULL) {
        error("Internal Error. Local variable is not registered.");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", pVar->offset);
    printf("  push rax\n");
}

static void gen_if_stmt(Node* pNode, const LVar* pLVars, int* pLabelCount) {
    const int endLabelId = (*pLabelCount)++;

    // ��������]��
    gen_node(pNode->children[0], pLVars, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    if (pNode->rhs) {
        const int elseLabelId = (*pLabelCount)++;

        // ���������U(0)�Ȃ�else���x���փW�����v
        printf("  je  .Lelse%04d\n", elseLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch��]�����Aend���x���փW�����v
        gen_node(pNode->lhs, pLVars, pLabelCount);
        printf("  jmp .Lend%04d\n", endLabelId);

        // else���x���ł�else-branch��]���iend���x���ւ͎��R�Ɨ����邽�߃W�����v�s�v�j
        printf(".Lelse%04d:\n", elseLabelId);
        gen_node(pNode->rhs, pLVars, pLabelCount);
    }
    else {
        // ���������U(0)�Ȃ�end���x���փW�����v
        printf("  je  .Lend%04d\n", endLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch��]��
        gen_node(pNode->lhs, pLVars, pLabelCount);
    }

    printf(".Lend%04d:\n", endLabelId);
    return;
}

static void gen_node(Node* pNode, const LVar* pLVars, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NUM:
        // ���l���e����
        printf("  push %d\n", pNode->pToken->val);
        return;
    case ND_LVAR:
        // �ϐ�
        gen_lval(pNode, pLVars);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        // ������Z
        gen_lval(pNode->lhs, pLVars);
        gen_node(pNode->rhs, pLVars, pLabelCount);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_STMT:
        // ��
        gen_node(pNode->lhs, pLVars, pLabelCount);

        // �p����������Ȃ炻���]��
        if (pNode->rhs) gen_node(pNode->rhs, pLVars, pLabelCount);
        return;
    case ND_EXPR_STMT:
        // ����
        gen_node(pNode->lhs, pLVars, pLabelCount);

        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
        return;
    case ND_RETURN:
        // return��
        gen_node(pNode->lhs, pLVars, pLabelCount);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    case ND_IF:
        // if��
        gen_if_stmt(pNode, pLVars, pLabelCount);
        return;
    }

    // �񍀉��Z
    gen_node(pNode->lhs, pLVars, pLabelCount);
    gen_node(pNode->rhs, pLVars, pLabelCount);
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
    int labelCount = 0;

    // �ϐ��o�^���s��
    LVar* pLVars = NULL;
    int stack_size = resigter_lvars(&pLVars, pNode);

    // �A�Z���u���̑O���������o��
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // �v�����[�O
    // ���[�J���ϐ����K�v�Ƃ��镪�̗̈���m�ۂ���
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_node(pNode, pLVars, &labelCount);

    // �G�s���[�O
    // �Ō�̎��̌��ʂ�RAX�Ɏc���Ă���̂ł��ꂪ�Ԃ�l�ɂȂ�
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
