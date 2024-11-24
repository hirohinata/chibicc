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

// �G���[�ӏ���񍐂���
void error_at(const char* user_input, const char* loc, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = (int)(loc - user_input);
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos�̋󔒂��o��
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}
