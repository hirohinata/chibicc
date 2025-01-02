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

// ���̃g�[�N�������҂��Ă���L���̂Ƃ��ɂ́A�g�[�N����1�ǂݐi�߂�
// �^��Ԃ��B����ȊO�̏ꍇ�ɂ͋U��Ԃ��B
bool consume_reserved_word(Token** ppToken, TokenKind token_kind) {
    if ((*ppToken)->kind != token_kind) {
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

        // ���ʎq
        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z') {
            const char* pEnd = p;
            do { pEnd++; } while ('a' <= *pEnd && *pEnd <= 'z' || 'A' <= *pEnd && *pEnd <= 'Z' || '0' <= *pEnd && *pEnd <= '9' || *pEnd == '_');

            if ((int)(pEnd - p) == 6 && strncmp(p, "return", 6) == 0) {
                cur = new_token(TK_RETURN, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 2 && strncmp(p, "if", 2) == 0) {
                cur = new_token(TK_IF, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "else", 4) == 0) {
                cur = new_token(TK_ELSE, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 5 && strncmp(p, "while", 5) == 0) {
                cur = new_token(TK_WHILE, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "for", 3) == 0) {
                cur = new_token(TK_FOR, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "int", 3) == 0) {
                cur = new_token(TK_INT, cur, p, (int)(pEnd - p), user_input);
            }
            else if ((int)(pEnd - p) == 6 && strncmp(p, "sizeof", 6) == 0) {
                cur = new_token(TK_SIZEOF, cur, p, (int)(pEnd - p), user_input);
            }
            else {
                cur = new_token(TK_IDENT, cur, p, (int)(pEnd - p), user_input);
            }
            p = pEnd;
            continue;
        }

        // �ꕶ���L��
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '{' || *p == '}' || *p == ';' || *p == ',') {
            cur = new_token(TK_RESERVED, cur, p++, 1, user_input);
            continue;
        }

        // �񕶎��ɂȂ蓾��L��
        if (*p == '=' || *p == '!' || *p == '<' || *p == '>' || *p == '&') {
            if (*(p + 1) == '=' ||
                (*(p + 1) == *p && (*p == '&'))) {
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
