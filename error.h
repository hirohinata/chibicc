#pragma once

// �G���[��񍐂��邽�߂̊֐�
// printf�Ɠ������������
void error(char* fmt, ...);

// �G���[�ӏ���񍐂���
void error_at(const char* filename, const char* user_input, const char* loc, char* fmt, ...);
