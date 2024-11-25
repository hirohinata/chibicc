#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "error.h"

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// �^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool consume(Token** ppToken, const char* op) {
    if ((*ppToken)->kind != TK_RESERVED ||
        strlen(op) != (*ppToken)->len ||
        memcmp((*ppToken)->str, op, (*ppToken)->len))
    {
        return false;
    }
    *ppToken = (*ppToken)->next;
    return true;
}

// ���̃g�[�N�������ʎq�̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// ���ʎq�g�[�N����Ԃ��B����ȊO�̏ꍇ�ɂ�NULL��Ԃ��B
const Token* consume_ident(Token** ppToken) {
    if ((*ppToken)->kind != TK_IDENT) {
        return NULL;
    }

    const Token* pIdentToken = *ppToken;
    *ppToken = (*ppToken)->next;
    return pIdentToken;

}

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�B
// ����ȊO�̏ꍇ�ɂ̓G���[��񍐂���B
void expect(Token** ppToken, const char* op) {
    if ((*ppToken)->kind != TK_RESERVED ||
        strlen(op) != (*ppToken)->len ||
        memcmp((*ppToken)->str, op, (*ppToken)->len))
    {
        error_at((*ppToken)->user_input, (*ppToken)->str, "'%s'�ł͂���܂���", op);
    }
    *ppToken = (*ppToken)->next;
}

// ���̃g�[�N�������l�̏ꍇ�A�g�[�N����1�ǂݐi�߂Ă��̐��l��Ԃ��B
// ����ȊO�̏ꍇ�ɂ̓G���[��񍐂���B
int expect_number(Token** ppToken) {
    if ((*ppToken)->kind != TK_NUM) {
        error_at((*ppToken)->user_input, (*ppToken)->str, "���ł͂���܂���");
    }
    int val = (*ppToken)->val;
    *ppToken = (*ppToken)->next;
    return val;
}

// ���̃g�[�N����EOF�Ȃ�^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool at_eof(Token* pToken) {
    return pToken->kind == TK_EOF;
}

// �V�����g�[�N�����쐬����cur�Ɍq����
static Token* new_token(TokenKind kind, Token* cur, const char* str, int len, const char* user_input) {
    Token* tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->user_input = user_input;
    cur->next = tok;
    return tok;
}

// ���͕�����p���g�[�N�i�C�Y���Ă����Ԃ�
Token* tokenize(const char* user_input) {
    Token head;
    head.next = NULL;
    Token* cur = &head;

    const char* p = user_input;
    while (*p) {
        // �󔒕������X�L�b�v
        if (isspace(*p)) {
            p++;
            continue;
        }

        // �ꕶ���ϐ�
        if ('a' <= *p && *p <= 'z') {
            cur = new_token(TK_IDENT, cur, p++, 1, user_input);
            cur->len = 1;
            continue;
        }

        // �ꕶ���L��
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == ';') {
            cur = new_token(TK_RESERVED, cur, p++, 1, user_input);
            continue;
        }

        // �񕶎��ɂȂ蓾��L��
        if (*p == '=' || *p == '!' || *p == '<' || *p == '>') {
            if (*(p + 1) == '=') {
                cur = new_token(TK_RESERVED, cur, p, 2, user_input);
                p += 2;
            }
            else {
                //TODO: �����_�ł͘_��NOT('!')�͍\����͂��Ή����Ă��Ȃ�
                cur = new_token(TK_RESERVED, cur, p++, 1, user_input);
            }
            continue;
        }

        // ���l���e����
        if (isdigit(*p)) {
            const char* pEnd = p;
            int val = strtol(p, &pEnd, 10);

            cur = new_token(TK_NUM, cur, p, (int)(pEnd - p), user_input);
            cur->val = val;
            p = pEnd;
            continue;
        }

        error_at(user_input, p, "�g�[�N�i�C�Y�ł��܂���");
    }

    new_token(TK_EOF, cur, p, 0, user_input);
    return head.next;
}