#pragma once

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char* fmt, ...);

// エラー箇所を報告する
void error_at(const char* user_input, const char* loc, char* fmt, ...);
