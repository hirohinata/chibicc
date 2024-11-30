#pragma once

// �g�[�N���̎��
typedef enum {
    TK_RESERVED, // �L��
    TK_RETURN,   // return
    TK_IF,       // if
    TK_ELSE,     // else
    TK_WHILE,    // while
    TK_FOR,      // for
    TK_IDENT,    // ���ʎq
    TK_NUM,      // �����g�[�N��
    TK_EOF,      // ���͂̏I����\���g�[�N��
} TokenKind;

typedef struct Token Token;

// �g�[�N���^
struct Token {
    TokenKind kind;             // �g�[�N���̌^
    Token* next;                // ���̓��̓g�[�N��
    int val;                    // kind��TK_NUM�̏ꍇ�A���̐��l
    const char* str;            // �g�[�N��������
    int len;                    // �g�[�N���̒���
    const char* user_input;     // ���������[�U�[���͕�����
};

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// �^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool consume(Token** ppToken, const char* op);

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// �^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool consume_reserved_word(Token** ppToken, TokenKind token_kind);

// ���̃g�[�N�������ʎq�̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// ���ʎq�g�[�N����Ԃ��B����ȊO�̏ꍇ�ɂ�NULL��Ԃ��B
const Token* consume_ident(Token** ppToken);

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�B
// ����ȊO�̏ꍇ�ɂ̓G���[��񍐂���B
void expect(Token** ppToken, const char* op);

// ���̃g�[�N�������l�̏ꍇ�A�g�[�N����1�ǂݐi�߂Ă��̐��l��Ԃ��B
// ����ȊO�̏ꍇ�ɂ̓G���[��񍐂���B
int expect_number(Token** ppToken);

// ���̃g�[�N����EOF�Ȃ�^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool at_eof(Token* pToken);

// ���͕�����p���g�[�N�i�C�Y���Ă����Ԃ�
Token* tokenize(const char* user_input);
