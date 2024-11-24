#pragma once

// ���ۍ\���؂̃m�[�h�̎��
typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_EQ,  // ==
    ND_NE,  // !=
    ND_LT,  // <
    ND_LE,  // <=
    ND_NUM, // ����
} NodeKind;

typedef struct Node Node;

// ���ۍ\���؂̃m�[�h�̌^
struct Node {
    NodeKind kind; // �m�[�h�̌^
    Node* lhs;     // ����
    Node* rhs;     // �E��
    int val;       // kind��ND_NUM�̏ꍇ�̂ݎg��
};

Node* parse(struct Token* pToken);
