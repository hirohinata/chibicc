#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errno.h"

// �G���[��񍐂��邽�߂̊֐�
// printf�Ɠ������������
void error(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// �G���[�̋N�����ꏊ��񍐂��邽�߂̊֐�
// ���̂悤�ȃt�H�[�}�b�g�ŃG���[���b�Z�[�W��\������
//
// foo.c:10: x = y + + 5;
//                   ^ ���ł͂���܂���
void error_at(const char* filename, const char* user_input, const char* loc, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    // loc���܂܂�Ă���s�̊J�n�n�_�ƏI���n�_���擾
    char* line = loc;
    while (user_input < line && line[-1] != '\n')
        line--;

    char* end = loc;
    while (*end != '\n')
        end++;

    // ���������s���S�̂̉��s�ڂȂ̂��𒲂ׂ�
    int line_num = 1;
    for (char* p = user_input; p < line; p++)
        if (*p == '\n')
            line_num++;

    // ���������s���A�t�@�C�����ƍs�ԍ��ƈꏏ�ɕ\��
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // �G���[�ӏ���"^"�Ŏw�������āA�G���[���b�Z�[�W��\��
    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, ""); // pos�̋󔒂��o��
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}