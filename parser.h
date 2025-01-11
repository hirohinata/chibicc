#pragma once

// ���ۍ\���؂̃m�[�h�̎��
typedef enum {
    ND_NOP,         // �������Ȃ���v�f
    ND_TOP_LEVEL,   // �֐���`�O�̃g�b�v���x���w
    ND_DEF_FUNC,    // �֐���`
    ND_DECL_VAR,    // �ϐ��錾
    ND_TYPE,        // �^
    ND_BLOCK,       // �u���b�N
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_ASSIGN,      // =
    ND_ADDR,        // &�i�A�h���X�j
    ND_DEREF,       // *�i�֐ߎQ�Ɓj
    ND_SIZEOF,      // sizeof
    ND_INVOKE,      // �֐��Ăяo��
    ND_VAR,         // �ϐ�
    ND_NUM,         // ����
    ND_STRING,      // ������
    ND_EXPR_STMT,   // ����
    ND_RETURN,      // return��
    ND_IF,          // if��
    ND_WHILE,       // while��
    ND_FOR,         // for��
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;
typedef struct StringLiteral StringLiteral;

// ���ۍ\���؂̃m�[�h�̌^
struct Node {
    NodeKind kind;          // �m�[�h�̌^
    const Node* lhs;        // ����
    const Node* rhs;        // �E��
    const Node* children[4];// ���̑��̎q�m�[�h
    const Token* pToken;    // ���g�[�N��
};

Node* parse(Token* pToken, const StringLiteral* pStrLiterals);
