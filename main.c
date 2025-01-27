﻿#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "asm_gen.h"
#include "error.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    StringLiteral* pStrLiterals = NULL;

    // トークナイズする
    Token* pToken = tokenize(argv[1], &pStrLiterals);

    // 構文木を作成する
    Node* pNode = parse(pToken, pStrLiterals);

    // 構文木からアセンブリを出力
    gen(pNode, pStrLiterals);

    return 0;
}
