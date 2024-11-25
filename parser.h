#pragma once

// ���ۍ\���؂̃m�[�h�̎��
typedef enum {
    ND_STMT,    // ��
    ND_ADD,     // +
    ND_SUB,     // -
    ND_MUL,     // *
    ND_DIV,     // /
    ND_EQ,      // ==
    ND_NE,      // !=
    ND_LT,      // <
    ND_LE,      // <=
    ND_ASSIGN,  // =
    ND_LVAR,    // ���[�J���ϐ�
    ND_NUM,     // ����
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;

// ���ۍ\���؂̃m�[�h�̌^
struct Node {
    NodeKind kind;          // �m�[�h�̌^
    Node* lhs;              // ����
    Node* rhs;              // �E��
    const Token* pToken;    // ���g�[�N��
};

Node* parse(struct Token* pToken);
