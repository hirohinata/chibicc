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
        error_at((*ppToken)->filename, (*ppToken)->user_input, (*ppToken)->str, "'%s'�ł͂���܂���", op);
    }
    *ppToken = (*ppToken)->next;
}

// ���̃g�[�N�������l�̏ꍇ�A�g�[�N����1�ǂݐi�߂Ă��̐��l��Ԃ��B
// ����ȊO�̏ꍇ�ɂ̓G���[��񍐂���B
int expect_number(Token** ppToken) {
    if ((*ppToken)->kind != TK_NUM) {
        error_at((*ppToken)->filename, (*ppToken)->user_input, (*ppToken)->str, "���ł͂���܂���");
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
static Token* new_token(TokenKind kind, Token* cur, const char* str, int len, const char* filename, const char* user_input) {
    Token* tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->filename = filename;
    tok->user_input = user_input;
    cur->next = tok;
    return tok;
}

// �w�肳�ꂽ�t�@�C���̓��e��Ԃ�
static const char* read_file(const char* path) {
    // �t�@�C�����J��
    FILE* fp = fopen(path, "r");
    if (!fp) {
        error("cannot open %s", path);
        return NULL;
    }

    // �t�@�C���̒����𒲂ׂ�
    if (fseek(fp, 0, SEEK_END) == -1) {
        error("%s cannot fseek", path);
        return NULL;
    }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1) {
        error("%s cannot fseek", path);
        return NULL;
    }

    // �t�@�C�����e��ǂݍ���
    char* buf = calloc(1, size + 1);
    fread(buf, size, 1, fp);

    fclose(fp);
    return buf;
}

// ���͕�����p���g�[�N�i�C�Y���Ă����Ԃ�
Token* tokenize(const char* filename, StringLiteral** ppStrLiterals) {
    Token head;
    head.next = NULL;
    Token* cur = &head;
    StringLiteral* pCurStrLiterals = NULL;
    int strLiteralCount = 0;

    const char* user_input = read_file(filename);
    const char* p = user_input;
    while (*p) {
        // �󔒕������X�L�b�v
        if (isspace(*p)) {
            p++;
            continue;
        }

        // �s�R�����g���X�L�b�v
        if (strncmp(p, "//", 2) == 0) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            continue;
        }

        // �u���b�N�R�����g���X�L�b�v
        if (strncmp(p, "/*", 2) == 0) {
            char* q = strstr(p + 2, "*/");
            if (!q) {
                error_at(filename, user_input, p, "�R�����g�������Ă��܂���");
            }
            p = q + 2;
            continue;
        }

        // ���ʎq
        if ('a' <= *p && *p <= 'z' || 'A' <= *p && *p <= 'Z') {
            const char* pEnd = p;
            do { pEnd++; } while ('a' <= *pEnd && *pEnd <= 'z' || 'A' <= *pEnd && *pEnd <= 'Z' || '0' <= *pEnd && *pEnd <= '9' || *pEnd == '_');

            if ((int)(pEnd - p) == 6 && strncmp(p, "return", 6) == 0) {
                cur = new_token(TK_RETURN, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 2 && strncmp(p, "if", 2) == 0) {
                cur = new_token(TK_IF, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "else", 4) == 0) {
                cur = new_token(TK_ELSE, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 5 && strncmp(p, "while", 5) == 0) {
                cur = new_token(TK_WHILE, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "for", 3) == 0) {
                cur = new_token(TK_FOR, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 4 && strncmp(p, "char", 4) == 0) {
                cur = new_token(TK_CHAR, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 3 && strncmp(p, "int", 3) == 0) {
                cur = new_token(TK_INT, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else if ((int)(pEnd - p) == 6 && strncmp(p, "sizeof", 6) == 0) {
                cur = new_token(TK_SIZEOF, cur, p, (int)(pEnd - p), filename, user_input);
            }
            else {
                cur = new_token(TK_IDENT, cur, p, (int)(pEnd - p), filename, user_input);
            }
            p = pEnd;
            continue;
        }

        // �ꕶ���L��
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '{' || *p == '}' || *p == '[' || *p == ']' || *p == ';' || *p == ',') {
            cur = new_token(TK_RESERVED, cur, p++, 1, filename, user_input);
            continue;
        }

        // �񕶎��ɂȂ蓾��L��
        if (*p == '=' || *p == '!' || *p == '<' || *p == '>' || *p == '&') {
            if (*(p + 1) == '=' ||
                (*(p + 1) == *p && (*p == '&'))) {
                cur = new_token(TK_RESERVED, cur, p, 2, filename, user_input);
                p += 2;
            }
            else {
                //TODO: �����_�ł͘_��NOT('!')�͍\����͂��Ή����Ă��Ȃ�
                cur = new_token(TK_RESERVED, cur, p++, 1, filename, user_input);
            }
            continue;
        }

        // ���l���e����
        if (isdigit(*p)) {
            const char* pEnd = p;
            int val = strtol(p, &pEnd, 10);

            cur = new_token(TK_NUM, cur, p, (int)(pEnd - p), filename, user_input);
            cur->val = val;
            p = pEnd;
            continue;
        }

        // �����񃊃e����
        if (*p == '"') {
            const char* pEnd = p;
            while (*(++pEnd) != '"') {
                if (*pEnd == '\0' || *pEnd == '\n') {
                    error_at(filename, user_input, p, "�����񃊃e�����������Ă��܂���");
                }
            }
            ++pEnd;

            if (pCurStrLiterals == NULL) {
                *ppStrLiterals = calloc(1, sizeof(StringLiteral));
                pCurStrLiterals = *ppStrLiterals;
            }
            else {
                pCurStrLiterals->pNext = calloc(1, sizeof(StringLiteral));
                pCurStrLiterals = pCurStrLiterals->pNext;
            }

            int len = (int)(pEnd - p);

            pCurStrLiterals->pszText = calloc(len + 1, sizeof(char));
            strncpy_s(pCurStrLiterals->pszText, len + 1, p, len);

            cur = new_token(TK_STRING, cur, p, (int)(pEnd - p), filename, user_input);
            cur->val = strLiteralCount++;
            p = pEnd;
            continue;

        }

        error_at(filename, user_input, p, "�g�[�N�i�C�Y�ł��܂���");
    }

    new_token(TK_EOF, cur, p, 0, filename, user_input);
    return head.next;
}
