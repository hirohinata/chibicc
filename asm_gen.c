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

#define MAX_FUNC_NAME (64)

typedef struct LVar LVar;

// ���[�J���ϐ��̌^
struct LVar {
    LVar* next;         // ���̕ϐ���NULL
    const char* name;   // �ϐ��̖��O
    int len;            // ���O�̒���
    int offset;         // RBP����̃I�t�Z�b�g
};

static void gen_lval(const Node* pNode, const LVar* pLVars);
static void gen_if_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_while_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_for_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_invoke_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_local_node(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_def_func(const Node* pNode, int* pLabelCount);
static void gen_global_node(const Node* pNode, int* pLabelCount);

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
static int resigter_lvars(LVar** ppLVarTop, const Node* pNode) {
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

static void gen_if_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    const int endLabelId = (*pLabelCount)++;

    // ��������]��
    gen_local_node(pNode->children[0], pLVars, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    if (pNode->rhs) {
        const int elseLabelId = (*pLabelCount)++;

        // ���������U(0)�Ȃ�else���x���փW�����v
        printf("  je  .Lelse%04d\n", elseLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch��]�����Aend���x���փW�����v
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
        printf("  jmp .Lend%04d\n", endLabelId);

        // else���x���ł�else-branch�����s�iend���x���ւ͎��R�Ɨ����邽�߃W�����v�s�v�j
        printf(".Lelse%04d:\n", elseLabelId);
        gen_local_node(pNode->rhs, pLVars, pLabelCount);
    }
    else {
        // ���������U(0)�Ȃ�end���x���փW�����v
        printf("  je  .Lend%04d\n", endLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch�����s
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
    }

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_while_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    const int beginLabelId = (*pLabelCount)++;
    const int endLabelId = (*pLabelCount)++;

    printf(".Lbegin%04d:\n", beginLabelId);

    // ��������]��
    gen_local_node(pNode->lhs, pLVars, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    // ���������U(0)�Ȃ�end���x���փW�����v
    printf("  je  .Lend%04d\n", endLabelId);

    // ���[�v�Ώۂ̕������s
    gen_local_node(pNode->rhs, pLVars, pLabelCount);

    // ���[�v���邽�߂�begin���x���֖������W�����v
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_for_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    /*
  A���R���p�C�������R�[�h
.LbeginXXX:
  B���R���p�C�������R�[�h
  pop rax
  cmp rax, 0
  je  .LendXXX
  D���R���p�C�������R�[�h
  C���R���p�C�������R�[�h
  jmp .LbeginXXX
.LendXXX:
    */
    const int beginLabelId = (*pLabelCount)++;
    const int endLabelId = (*pLabelCount)++;

    // ����������]��
    if (pNode->children[0]) {
        gen_local_node(pNode->children[0], pLVars, pLabelCount);
        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
    }

    printf(".Lbegin%04d:\n", beginLabelId);

    // ��������]��
    if (pNode->children[1]) {
        gen_local_node(pNode->children[1], pLVars, pLabelCount);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");

        // ���������U(0)�Ȃ�end���x���փW�����v
        printf("  je  .Lend%04d\n", endLabelId);
    }

    // ���[�v�Ώۂ̕������s
    gen_local_node(pNode->rhs, pLVars, pLabelCount);

    // ���[�v���Ƃɕ]�����鎮��]��
    if (pNode->children[2]) {
        gen_local_node(pNode->children[2], pLVars, pLabelCount);
        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
    }

    // ���[�v���邽�߂�begin���x���֖������W�����v
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_invoke_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    int i;
    const char regNames[][4] = { "rcx", "rdx", "r8", "r9" };
    char funcName[MAX_FUNC_NAME] = { 0 };

    _STATIC_ASSERT(sizeof(regNames) / sizeof(regNames[0]) == sizeof(pNode->children) / sizeof(pNode->children[0]));

    if (MAX_FUNC_NAME <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�֐�����%d�����ȏ゠��܂�", MAX_FUNC_NAME);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    // ���������ɕ]�����āA�Ή����郌�W�X�^�Ɋi�[
    for (i = 0; i < sizeof(pNode->children) / sizeof(pNode->children[0]); ++i) {
        if (pNode->children[i] == NULL) break;

        gen_local_node(pNode->children[i], pLVars, pLabelCount);
        printf("  pop %s\n", regNames[i]);
    }

    // �Ăяo�����rax�S�̂𗘗p����Ƃ͌���Ȃ��̂Ń[���N���A������
    printf("  mov rax, 0\n");

    // rsp��16�̔{���ɂ��낦��ix86-64��ABI�ɂ�鐧��j
    //     rsp��r15�ɑޔ����Ă���
    //     ���ʃo�C�g��0���߂�����16�̔{���ɂȂ�
    // TODO: rsp��16�̔{���ɂ��낦�Ă���͂������A���ꂾ�ƍ��p�x�ňُ�l���Ԃ邽�߉��ʃo�C�g���ׂĂ�0���߂��Ă���B
    //       ���̏�Ԃł��ُ�l���Ԃ邱�Ƃ����邪�A�p�x�͉������Ă���B
    //       �����炭�͑��Ɏ��ׂ����񂪂�����̂Ǝv����B
    printf("  mov  r15, rsp\n");
    printf("  mov  spl, 0\n");

    printf("  call %s\n", funcName);

    printf("  mov  rsp, r15\n");

    // �߂�l��rax�Ɋi�[����Ă���̂ł����push����
    printf("  push rax\n");
}

static void gen_local_node(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NOP:
        // �������Ȃ�
        return;
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
    case ND_INVOKE:
        // �֐��Ăяo��
        gen_invoke_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_ASSIGN:
        // ������Z
        gen_lval(pNode->lhs, pLVars);
        gen_local_node(pNode->rhs, pLVars, pLabelCount);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_BLOCK:
        // �u���b�N
        gen_local_node(pNode->lhs, pLVars, pLabelCount);

        // �p����������Ȃ炻���]��
        if (pNode->rhs) gen_local_node(pNode->rhs, pLVars, pLabelCount);
        return;
    case ND_EXPR_STMT:
        // ����
        gen_local_node(pNode->lhs, pLVars, pLabelCount);

        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
        return;
    case ND_RETURN:
        // return��
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    case ND_IF:
        // if��
        gen_if_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_WHILE:
        // while��
        gen_while_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_FOR:
        // for��
        gen_for_stmt(pNode, pLVars, pLabelCount);
        return;
    }

    // �񍀉��Z
    gen_local_node(pNode->lhs, pLVars, pLabelCount);
    gen_local_node(pNode->rhs, pLVars, pLabelCount);
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

static void gen_def_func(const Node* pNode, int* pLabelCount) {
    char funcName[MAX_FUNC_NAME] = { 0 };

    if (MAX_FUNC_NAME <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�֐�����%d�����ȏ゠��܂�", MAX_FUNC_NAME);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    //TODO:pNode->children����������擾����

    LVar* pLVars = NULL;
    int stack_size = resigter_lvars(&pLVars, pNode);

    printf("%s:\n", funcName);

    // �v�����[�O
    // ���[�J���ϐ����K�v�Ƃ��镪�̗̈���m�ۂ���
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_local_node(pNode->lhs, pLVars, pLabelCount);

    // �G�s���[�O
    // �Ō�̎��̌��ʂ�RAX�Ɏc���Ă���̂ł��ꂪ�Ԃ�l�ɂȂ�
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_global_node(const Node* pNode, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NOP:
        // �������Ȃ�
        return;
    case ND_TOP_LEVEL:
        // �֐���`�O�̃g�b�v���x���w
        gen_global_node(pNode->lhs, pLabelCount);

        // �p���m�[�h������Ȃ炻���]��
        if (pNode->rhs) gen_global_node(pNode->rhs, pLabelCount);
        return;
    case ND_DEF_FUNC:
        // �֐���`
        gen_def_func(pNode, pLabelCount);
        return;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }
}

void gen(const Node* pNode) {
    int labelCount = 0;

    // �A�Z���u���̑O���������o��
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_global_node(pNode, &labelCount);
}
