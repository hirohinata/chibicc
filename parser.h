#pragma once

// ���ۍ\���؂̃m�[�h�̎��
typedef enum {
    ND_NOP,         // �������Ȃ���v�f
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
    ND_LVAR,        // ���[�J���ϐ�
    ND_NUM,         // ����
    ND_EXPR_STMT,   // ����
    ND_RETURN,      // return��
    ND_IF,          // if��
    ND_WHILE,       // while��
    ND_FOR,         // for��
} NodeKind;

typedef struct Token Token;
typedef struct Node Node;

// ���ۍ\���؂̃m�[�h�̌^
struct Node {
    NodeKind kind;          // �m�[�h�̌^
    Node* lhs;              // ����
    Node* rhs;              // �E��
    Node* children[4];      // ���̑��̎q�m�[�h
    const Token* pToken;    // ���g�[�N��
};

Node* parse(struct Token* pToken);
