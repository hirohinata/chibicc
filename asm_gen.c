#include <windows.h>

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

#define MAX_FUNC_NAME_LEN (64)

typedef struct Type Type;
typedef struct LVar LVar;
typedef struct FuncContext FuncContext;

struct Type {
    enum { TY_VOID, TY_INT, TY_PTR, TY_ARRAY } ty;
    const Type* ptr_to;
    size_t array_size;
};
static const Type VOID_TYPE = { TY_VOID };
static const Type INT_TYPE = { TY_INT };

// ���[�J���ϐ��̌^
struct LVar {
    LVar* next;             // ���̕ϐ���NULL
    Type* pType;            // �ϐ��̌^
    const char* name;       // �ϐ��̖��O
    int len;                // ���O�̒���
    int offset;             // RBP����̃I�t�Z�b�g
};

// �֐���`���̊�
struct FuncContext {
    LVar* pLVars;           // ���[�J���ϐ��e�[�u���i�������W�J����j
};

#define PARAM_REG_INDEX_64BIT  (3)
#define PARAM_REG_INDEX_32BIT  (2)
#define PARAM_REG_INDEX_16BIT  (1)
#define PARAM_REG_INDEX_8BIT   (0)
static const char PARAM_REG_NAME[][4][4] = {
    {  "cl",  "dl", "r8b", "r9b" },
    {  "cx",  "dx", "r8w", "r9w" },
    { "ecx", "edx", "r8d", "r9d" },
    { "rcx", "rdx", "r8" , "r9"  },
};
_STATIC_ASSERT(sizeof(PARAM_REG_NAME[0]) / sizeof(PARAM_REG_NAME[0][0]) == sizeof(((Node*)0)->children) / sizeof(((Node*)0)->children[0]));

static const Type* gen_left_expr(const Node* pNode, const FuncContext* pContext);
static void gen_if_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount);
static void gen_while_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount);
static void gen_for_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount);
static const Type* gen_invoke_expr(const Node* pNode, const FuncContext* pContext, int* pLabelCount);
static const Type* gen_add_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_sub_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_mul_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_div_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_local_node(const Node* pNode, const FuncContext* pContext, int* pLabelCount);
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

static size_t get_type_size(const Type* pType) {
    switch (pType->ty) {
    case TY_INT:
        return 4;
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return pType->array_size * get_type_size(pType->ptr_to);
    default:
        error("Internal Error. Invalid Type '%d'.", pType->ty);
        return 0;
    }
}

static Type* parse_type(FuncContext* pContext, const Node* pNode) {
    Type* pType = calloc(1, sizeof(Type));

    if (pNode->kind != ND_TYPE) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�^�����K�v�ł�");
    }

    switch (pNode->pToken->kind) {
    case TK_INT:
        pType->ty = TY_INT;
        break;
    default:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "����`�̌^���ł�");
    }

    const Node* pCurNode = pNode;
    while (pCurNode->rhs != NULL) {
        Type* pNewType = calloc(1, sizeof(Type));
        pNewType->ptr_to = pType;

        switch (pCurNode->rhs->kind) {
        case ND_DEREF:
            pNewType->ty = TY_PTR;
            break;
        case ND_NUM:
            pNewType->ty = TY_ARRAY;
            pNewType->array_size = pCurNode->rhs->pToken->val;
            break;
        default:
            error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        }

        pType = pNewType;
        pCurNode = pCurNode->rhs;
    }

    return pType;
}

// �ϐ���o�^���A������X�^�b�N�T�C�Y��Ԃ��B
static int resigter_lvars(FuncContext* pContext, const Node* pNode) {
    if (pNode->kind == ND_DECL_VAR) {
        if (find_lvar(pContext->pLVars, pNode) != NULL) {
            error_at(pNode->pToken->user_input, pNode->pToken->str, "���[�J���ϐ������d�����Ă��܂�");
        }

        LVar* pVar = calloc(1, sizeof(LVar));
        pVar->next = pContext->pLVars;
        pVar->pType = parse_type(pContext, pNode->lhs);
        pVar->name = pNode->pToken->str;
        pVar->len = pNode->pToken->len;
        pVar->offset = (pContext->pLVars ? pContext->pLVars->offset : 0) + (int)get_type_size(pVar->pType);
        pContext->pLVars = pVar;
    }
    else {
        if (pNode->lhs) resigter_lvars(pContext, pNode->lhs);
        if (pNode->rhs) resigter_lvars(pContext, pNode->rhs);
    }

    return (pContext->pLVars ? pContext->pLVars->offset : 0);
}

// ������o�^���A�����̐���Ԃ��B
static int resigter_params(FuncContext* pContext, const Node* pNode) {
    int paramNum;

    for (paramNum = 0; paramNum < sizeof(pNode->children) / sizeof(pNode->children[0]); ++paramNum) {
        if (pNode->children[paramNum] == NULL) break;

        if (find_lvar(pContext->pLVars, pNode->children[paramNum]) != NULL) {
            error_at(pNode->children[paramNum]->pToken->user_input, pNode->children[paramNum]->pToken->str, "���������d�����Ă��܂�");
        }

        resigter_lvars(pContext, pNode->children[paramNum]);
    }

    return paramNum;
}

static const Type* gen_left_expr(const Node* pNode, const FuncContext* pContext) {
    if (pNode->kind != ND_LVAR) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�ϐ��ł������ł�����܂���");
    }

    const LVar* pVar = find_lvar(pContext->pLVars, pNode);
    if (pVar == NULL) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "����`�̕ϐ��ł�");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", pVar->offset);
    printf("  push rax\n");

    return pVar->pType;
}

static void gen_if_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount) {
    const int endLabelId = (*pLabelCount)++;

    // ��������]��
    gen_local_node(pNode->children[0], pContext, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    if (pNode->rhs) {
        const int elseLabelId = (*pLabelCount)++;

        // ���������U(0)�Ȃ�else���x���փW�����v
        printf("  je  .Lelse%04d\n", elseLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch��]�����Aend���x���փW�����v
        gen_local_node(pNode->lhs, pContext, pLabelCount);
        printf("  jmp .Lend%04d\n", endLabelId);

        // else���x���ł�else-branch�����s�iend���x���ւ͎��R�Ɨ����邽�߃W�����v�s�v�j
        printf(".Lelse%04d:\n", elseLabelId);
        gen_local_node(pNode->rhs, pContext, pLabelCount);
    }
    else {
        // ���������U(0)�Ȃ�end���x���փW�����v
        printf("  je  .Lend%04d\n", endLabelId);

        // ���������^�Ȃ�(else���x���փW�����v���Ă��Ȃ��Ȃ�)if-branch�����s
        gen_local_node(pNode->lhs, pContext, pLabelCount);
    }

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_while_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount) {
    const int beginLabelId = (*pLabelCount)++;
    const int endLabelId = (*pLabelCount)++;

    printf(".Lbegin%04d:\n", beginLabelId);

    // ��������]��
    gen_local_node(pNode->lhs, pContext, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    // ���������U(0)�Ȃ�end���x���փW�����v
    printf("  je  .Lend%04d\n", endLabelId);

    // ���[�v�Ώۂ̕������s
    gen_local_node(pNode->rhs, pContext, pLabelCount);

    // ���[�v���邽�߂�begin���x���֖������W�����v
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_for_stmt(const Node* pNode, const FuncContext* pContext, int* pLabelCount) {
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
        gen_local_node(pNode->children[0], pContext, pLabelCount);
        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
    }

    printf(".Lbegin%04d:\n", beginLabelId);

    // ��������]��
    if (pNode->children[1]) {
        gen_local_node(pNode->children[1], pContext, pLabelCount);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");

        // ���������U(0)�Ȃ�end���x���փW�����v
        printf("  je  .Lend%04d\n", endLabelId);
    }

    // ���[�v�Ώۂ̕������s
    gen_local_node(pNode->rhs, pContext, pLabelCount);

    // ���[�v���Ƃɕ]�����鎮��]��
    if (pNode->children[2]) {
        gen_local_node(pNode->children[2], pContext, pLabelCount);
        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
    }

    // ���[�v���邽�߂�begin���x���֖������W�����v
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static const Type* gen_invoke_expr(const Node* pNode, const FuncContext* pContext, int* pLabelCount) {
    int i;
    char funcName[MAX_FUNC_NAME_LEN + 1] = { 0 };

    if (MAX_FUNC_NAME_LEN <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�֐�����%d�����ȏ゠��܂�", MAX_FUNC_NAME_LEN);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    // ���������ɕ]�����āA�Ή����郌�W�X�^�Ɋi�[
    for (i = 0; i < sizeof(pNode->children) / sizeof(pNode->children[0]); ++i) {
        if (pNode->children[i] == NULL) break;

        gen_local_node(pNode->children[i], pContext, pLabelCount);
        printf("  pop %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_64BIT][i]);
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

    // TODO: �֐��e�[�u���������̂ŁAint�^�߂�l�̊֐��Ăяo���Ɖ��肵�Ă���
    return &INT_TYPE;
}

static const Type* gen_add_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    const Type* pResultType = NULL;

    switch (pLhsType->ty) {
    case TY_INT:
        switch (pRhsType->ty) {
        case TY_INT:
            pResultType = &INT_TYPE;
            break;
        case TY_PTR:
            //���Ӓl�̐����l���|�C���^���w����̌^�T�C�Y�{����
            printf("  imul rax, %zd\n", get_type_size(pRhsType->ptr_to));
            pResultType = pRhsType;
            break;
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    case TY_PTR:
        switch (pRhsType->ty) {
        case TY_INT:
            //�E�Ӓl�̐����l���|�C���^���w����̌^�T�C�Y�{����
            printf("  imul rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            pResultType = pLhsType;
            break;
        case TY_PTR:
            error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^���m�̉��Z�͂ł��܂���");
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    printf("  add rax, rdi\n");

    return pResultType;
}

static const Type* gen_sub_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    const Type* pResultType = NULL;

    switch (pLhsType->ty) {
    case TY_INT:
        switch (pRhsType->ty) {
        case TY_INT:
            pResultType = &INT_TYPE;
            break;
        case TY_PTR:
            error_at(pNode->pToken->user_input, pNode->pToken->str, "�����l����|�C���^�̌��Z�͂ł��܂���");
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    case TY_PTR:
        switch (pRhsType->ty) {
        case TY_INT:
            //�E�Ӓl�̐����l���|�C���^���w����̌^�T�C�Y�{����
            printf("  imul rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            pResultType = pLhsType;
            break;
        case TY_PTR:
            //�|�C���^���m�̌��Z��ptrdiff_t�^�ɂȂ�
            if (pLhsType->ptr_to->ty != pRhsType->ptr_to->ty) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "���Z����|�C���^�̌^����v���Ă��܂���");
            }
            printf("  sub rax, rdi\n");

            //�|�C���^���w����̌^�T�C�Y�ŏ��Z���邱�ƂŁA��̔z��v�f�̓Y���̍��ɂȂ�
            printf("  mov rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            printf("  cqo\n");
            printf("  idiv rdi\n");

            return &INT_TYPE;   // ���Z���Ă���㏈���Œl��������̂ŁA�|�C���^���m�̌��Z�͋��ʏ����ɂ��Ȃ�
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    printf("  sub rax, rdi\n");

    return pResultType;
}

static const Type* gen_mul_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    switch (pLhsType->ty) {
    case TY_INT:
        break;
    case TY_PTR:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^�̏�Z�͂ł��܂���");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    switch (pRhsType->ty) {
    case TY_INT:
        break;
    case TY_PTR:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^�̏�Z�͂ł��܂���");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
    }

    printf("  imul rax, rdi\n");
    return &INT_TYPE;
}

static const Type* gen_div_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    switch (pLhsType->ty) {
    case TY_INT:
        break;
    case TY_PTR:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^�̏��Z�͂ł��܂���");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    switch (pRhsType->ty) {
    case TY_INT:
        break;
    case TY_PTR:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^�̏��Z�͂ł��܂���");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
    }

    printf("  cqo\n");
    printf("  idiv rdi\n");
    return &INT_TYPE;
}

static const Type* gen_local_node(const Node* pNode, const FuncContext* pContext, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
    }

    switch (pNode->kind) {
    case ND_NOP:
        // �������Ȃ�
        return &VOID_TYPE;
    case ND_TYPE:
        // �^���i�R�[�h�̏o�͂͂��Ȃ��j
        return &VOID_TYPE;
    case ND_DECL_VAR:
        // �ϐ��錾�i���O�ɓo�^�ς݁F�������q�Ή�����Ȃ炱���j
        return &VOID_TYPE;
    case ND_NUM:
        // ���l���e����
        printf("  push %d\n", pNode->pToken->val);
        return &INT_TYPE;
    case ND_LVAR:
        // �ϐ�
        {
            const Type* pResultType = gen_left_expr(pNode, pContext);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            switch (pResultType->ty) {
            case TY_INT:
                printf("  cdqe\n");
                break;
            case TY_PTR:
            case TY_ARRAY:
                break;
            default:
                error("Internal Error. Invalid Type '%d'.", pResultType->ty);
            }
            printf("  push rax\n");
            return pResultType;
        }
    case ND_ADDR:
        // �P��&
        {
            Type* pResultType = calloc(1, sizeof(Type));
            pResultType->ty = TY_PTR;
            pResultType->ptr_to = gen_left_expr(pNode->lhs, pContext);
            return pResultType;
        }
    case ND_DEREF:
        // �P��*
        {
            const Type* pResultType = gen_local_node(pNode->lhs, pContext, pLabelCount);
            if (pResultType->ty != TY_PTR) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "�|�C���^�^�ł͂Ȃ��l�̓f���t�@�����X�ł��܂���");
            }
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            return pResultType->ptr_to;
        }
    case ND_SIZEOF:
        // sizeof
        {
            // �ꎞ�I�ɕW���o�͂�؂邱�ƂŔ퉉�Z�q�̕]���𖳌��ɂ���
            FILE* backup_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
            FILE* hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            SetStdHandle(STD_OUTPUT_HANDLE, hNull);

            const size_t size = get_type_size(gen_local_node(pNode->lhs, pContext, pLabelCount));

            // �W���o�͂����ɖ߂�
            SetStdHandle(STD_OUTPUT_HANDLE, backup_stdout);
            CloseHandle(hNull);

            printf("  push %zd\n", size);
            return &INT_TYPE;
        }
    case ND_SUBSCRIPT:
        // �z��Y����
        {
            const Type* pLhsType = gen_local_node(pNode->lhs, pContext, pLabelCount);
            const Type* pRhsType = gen_local_node(pNode->rhs, pContext, pLabelCount);
            if (pLhsType->ty != TY_ARRAY) {
                error_at(pNode->lhs->pToken->user_input, pNode->lhs->pToken->str, "�z��^�ł͂Ȃ��l�͓Y�����w��ł��܂���");
            }
            if (pLhsType->ty != TY_INT) {
                error_at(pNode->rhs->pToken->user_input, pNode->rhs->pToken->str, "�z��Y�����͐����^�ł���K�v������܂�");
            }
            printf("  pop rdi\n");
            printf("  imul rdi, %zd\n", get_type_size(pLhsType->ptr_to));   //�Y�������^�T�C�Y�{���ĉ��Z������Ώۗv�f���w���|�C���^�ɂȂ�
            printf("  pop rax\n");
            printf("  add rax, rdi\n");
            printf("  push rax\n");
            return pLhsType->ptr_to;
        }
    case ND_INVOKE:
        // �֐��Ăяo��
        return gen_invoke_expr(pNode, pContext, pLabelCount);
    case ND_ASSIGN:
        // ������Z
        {
            const Type* pLhsType = gen_left_expr(pNode->lhs, pContext);
            gen_local_node(pNode->rhs, pContext, pLabelCount);

            switch (pLhsType->ty) {
            case TY_INT:
                printf("  pop rax\n");
                printf("  cdqe\n");
                printf("  mov rdi, rax\n");
                printf("  pop rax\n");
                printf("  mov [rax], edi\n");
                break;
            case TY_PTR:
            case TY_ARRAY:
                printf("  pop rdi\n");
                printf("  pop rax\n");
                printf("  mov [rax], rdi\n");
                break;
            default:
                error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
            }
            printf("  push rdi\n");
            return pLhsType;
        }
    case ND_BLOCK:
        // �u���b�N
        gen_local_node(pNode->lhs, pContext, pLabelCount);

        // �p����������Ȃ炻���]��
        if (pNode->rhs) gen_local_node(pNode->rhs, pContext, pLabelCount);
        return &VOID_TYPE;
    case ND_EXPR_STMT:
        // ����
        gen_local_node(pNode->lhs, pContext, pLabelCount);

        // ���̕]�����ʂƂ��ăX�^�b�N�Ɉ�̒l���c���Ă���
        // �͂��Ȃ̂ŁA�X�^�b�N�����Ȃ��悤�Ƀ|�b�v���Ă���
        printf("  pop rax\n");
        return &VOID_TYPE;
    case ND_RETURN:
        // return��
        gen_local_node(pNode->lhs, pContext, pLabelCount);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return &VOID_TYPE;
    case ND_IF:
        // if��
        gen_if_stmt(pNode, pContext, pLabelCount);
        return &VOID_TYPE;
    case ND_WHILE:
        // while��
        gen_while_stmt(pNode, pContext, pLabelCount);
        return &VOID_TYPE;
    case ND_FOR:
        // for��
        gen_for_stmt(pNode, pContext, pLabelCount);
        return &VOID_TYPE;
    }

    // �񍀉��Z
    const Type* pLhsType = gen_local_node(pNode->lhs, pContext, pLabelCount);
    const Type* pRhsType = gen_local_node(pNode->rhs, pContext, pLabelCount);
    const Type* pResultType = NULL;
    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (pNode->kind) {
    case ND_ADD: // +
        pResultType = gen_add_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_SUB: // -
        pResultType = gen_sub_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_MUL: // *
        pResultType = gen_mul_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_DIV: // /
        pResultType = gen_div_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_EQ:  // ==
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_NE:  // !=
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_LT:  // <
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_LE:  // <=
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
    }

    printf("  push rax\n");
    return pResultType;
}

static void gen_def_func(const Node* pNode, int* pLabelCount) {
    int i;
    FuncContext context = { 0 };
    char funcName[MAX_FUNC_NAME_LEN + 1] = { 0 };

    if (MAX_FUNC_NAME_LEN <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "�֐�����%d�����ȏ゠��܂�", MAX_FUNC_NAME_LEN);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    int paramNum = resigter_params(&context, pNode);
    const LVar* pParamTop = context.pLVars;

    const int stack_size = resigter_lvars(&context, pNode);

    printf("%s:\n", funcName);

    // �v�����[�O
    // ���[�J���ϐ����K�v�Ƃ��镪�̗̈���m�ۂ���
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // ������Ή����郍�[�J���ϐ��ɓW�J����
    for (i = 0; i < paramNum; ++i) {
        if (pParamTop == NULL) {
            error("Internal Error. Param node is NULL.");
        }
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", pParamTop->offset);
        switch (pParamTop->pType->ty) {
        case TY_INT:
            printf("  mov [rax], %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_32BIT][paramNum - i - 1]);
            break;
        case TY_PTR:
        case TY_ARRAY:
            printf("  mov [rax], %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_64BIT][paramNum - i - 1]);
            break;
        default:
            error("Internal Error. Invalid Type '%d'.", pParamTop->pType->ty);
        }
        pParamTop = pParamTop->next;
    }

    // �e�m�[�h�̉�͂��s���A�Z���u���������o�͂���
    gen_local_node(pNode->rhs, &context, pLabelCount);

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
