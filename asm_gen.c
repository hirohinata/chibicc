#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "asm_gen.h"
#include "error.h"

#define MAX_FUNC_NAME (64)

typedef struct LVar LVar;

// ローカル変数の型
struct LVar {
    LVar* next;         // 次の変数かNULL
    const char* name;   // 変数の名前
    int len;            // 名前の長さ
    int offset;         // RBPからのオフセット
};

static void gen_lval(const Node* pNode, const LVar* pLVars);
static void gen_if_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_while_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_for_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_invoke_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_local_node(const Node* pNode, const LVar* pLVars, int* pLabelCount);
static void gen_def_func(const Node* pNode, int* pLabelCount);
static void gen_global_node(const Node* pNode, int* pLabelCount);

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static const LVar* find_lvar(const LVar* pLVarTop, const Node* pNode) {
    for (const LVar* pVar = pLVarTop; pVar; pVar = pVar->next) {
        if (pVar->len == pNode->pToken->len && !memcmp(pNode->pToken->str, pVar->name, pVar->len)) {
            return pVar;
        }
    }
    return NULL;
}

// 変数を登録し、総消費スタックサイズを返す。
static int resigter_lvars(LVar** ppLVarTop, const Node* pNode) {
    if (pNode->kind == ND_LVAR &&
        find_lvar(*ppLVarTop, pNode) == NULL)
    {
        LVar* pVar = calloc(1, sizeof(LVar));
        pVar->next = *ppLVarTop;
        pVar->name = pNode->pToken->str;
        pVar->len = pNode->pToken->len;
        pVar->offset = (*ppLVarTop ? (*ppLVarTop)->offset : 0) + 8;
        *ppLVarTop = pVar;
    }

    if (pNode->lhs) resigter_lvars(ppLVarTop, pNode->lhs);
    if (pNode->rhs) resigter_lvars(ppLVarTop, pNode->rhs);

    return (*ppLVarTop ? (*ppLVarTop)->offset : 0);
}

static void gen_lval(const Node* pNode, const LVar* pLVars) {
    if (pNode->kind != ND_LVAR) {
        error("代入の左辺値が変数ではありません");
    }

    const LVar* pVar = find_lvar(pLVars, pNode);
    if (pVar == NULL) {
        error("Internal Error. Local variable is not registered.");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", pVar->offset);
    printf("  push rax\n");
}

static void gen_if_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    const int endLabelId = (*pLabelCount)++;

    // 条件式を評価
    gen_local_node(pNode->children[0], pLVars, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    if (pNode->rhs) {
        const int elseLabelId = (*pLabelCount)++;

        // 条件式が偽(0)ならelseラベルへジャンプ
        printf("  je  .Lelse%04d\n", elseLabelId);

        // 条件式が真なら(elseラベルへジャンプしていないなら)if-branchを評価し、endラベルへジャンプ
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
        printf("  jmp .Lend%04d\n", endLabelId);

        // elseラベルではelse-branchを実行（endラベルへは自然と落ちるためジャンプ不要）
        printf(".Lelse%04d:\n", elseLabelId);
        gen_local_node(pNode->rhs, pLVars, pLabelCount);
    }
    else {
        // 条件式が偽(0)ならendラベルへジャンプ
        printf("  je  .Lend%04d\n", endLabelId);

        // 条件式が真なら(elseラベルへジャンプしていないなら)if-branchを実行
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
    }

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_while_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    const int beginLabelId = (*pLabelCount)++;
    const int endLabelId = (*pLabelCount)++;

    printf(".Lbegin%04d:\n", beginLabelId);

    // 条件式を評価
    gen_local_node(pNode->lhs, pLVars, pLabelCount);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    // 条件式が偽(0)ならendラベルへジャンプ
    printf("  je  .Lend%04d\n", endLabelId);

    // ループ対象の文を実行
    gen_local_node(pNode->rhs, pLVars, pLabelCount);

    // ループするためにbeginラベルへ無条件ジャンプ
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_for_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    /*
  Aをコンパイルしたコード
.LbeginXXX:
  Bをコンパイルしたコード
  pop rax
  cmp rax, 0
  je  .LendXXX
  Dをコンパイルしたコード
  Cをコンパイルしたコード
  jmp .LbeginXXX
.LendXXX:
    */
    const int beginLabelId = (*pLabelCount)++;
    const int endLabelId = (*pLabelCount)++;

    // 初期化式を評価
    if (pNode->children[0]) {
        gen_local_node(pNode->children[0], pLVars, pLabelCount);
        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
    }

    printf(".Lbegin%04d:\n", beginLabelId);

    // 条件式を評価
    if (pNode->children[1]) {
        gen_local_node(pNode->children[1], pLVars, pLabelCount);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");

        // 条件式が偽(0)ならendラベルへジャンプ
        printf("  je  .Lend%04d\n", endLabelId);
    }

    // ループ対象の文を実行
    gen_local_node(pNode->rhs, pLVars, pLabelCount);

    // ループごとに評価する式を評価
    if (pNode->children[2]) {
        gen_local_node(pNode->children[2], pLVars, pLabelCount);
        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
    }

    // ループするためにbeginラベルへ無条件ジャンプ
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_invoke_stmt(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    int i;
    const char regNames[][4] = { "rcx", "rdx", "r8", "r9" };
    char funcName[MAX_FUNC_NAME] = { 0 };

    _STATIC_ASSERT(sizeof(regNames) / sizeof(regNames[0]) == sizeof(pNode->children) / sizeof(pNode->children[0]));

    if (MAX_FUNC_NAME <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "関数名が%d文字以上あります", MAX_FUNC_NAME);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    // 引数を順に評価して、対応するレジスタに格納
    for (i = 0; i < sizeof(pNode->children) / sizeof(pNode->children[0]); ++i) {
        if (pNode->children[i] == NULL) break;

        gen_local_node(pNode->children[i], pLVars, pLabelCount);
        printf("  pop %s\n", regNames[i]);
    }

    // 呼び出し先でrax全体を利用するとは限らないのでゼロクリアを挟む
    printf("  mov rax, 0\n");

    // rspを16の倍数にそろえる（x86-64のABIによる制約）
    //     rspはr15に退避しておく
    //     下位バイトを0埋めしたら16の倍数になる
    // TODO: rspを16の倍数にそろえているはずだが、それだと高頻度で異常値が返るため下位バイトすべてを0埋めしている。
    //       この状態でも異常値が返ることがあるが、頻度は下がっている。
    //       おそらくは他に守るべき制約があるものと思われる。
    printf("  mov  r15, rsp\n");
    printf("  mov  spl, 0\n");

    printf("  call %s\n", funcName);

    printf("  mov  rsp, r15\n");

    // 戻り値はraxに格納されているのでそれをpushする
    printf("  push rax\n");
}

static void gen_local_node(const Node* pNode, const LVar* pLVars, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NOP:
        // 何もしない
        return;
    case ND_NUM:
        // 数値リテラル
        printf("  push %d\n", pNode->pToken->val);
        return;
    case ND_LVAR:
        // 変数
        gen_lval(pNode, pLVars);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_INVOKE:
        // 関数呼び出し
        gen_invoke_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_ASSIGN:
        // 代入演算
        gen_lval(pNode->lhs, pLVars);
        gen_local_node(pNode->rhs, pLVars, pLabelCount);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_BLOCK:
        // ブロック
        gen_local_node(pNode->lhs, pLVars, pLabelCount);

        // 継続文があるならそれを評価
        if (pNode->rhs) gen_local_node(pNode->rhs, pLVars, pLabelCount);
        return;
    case ND_EXPR_STMT:
        // 式文
        gen_local_node(pNode->lhs, pLVars, pLabelCount);

        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
        return;
    case ND_RETURN:
        // return文
        gen_local_node(pNode->lhs, pLVars, pLabelCount);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    case ND_IF:
        // if文
        gen_if_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_WHILE:
        // while文
        gen_while_stmt(pNode, pLVars, pLabelCount);
        return;
    case ND_FOR:
        // for文
        gen_for_stmt(pNode, pLVars, pLabelCount);
        return;
    }

    // 二項演算
    gen_local_node(pNode->lhs, pLVars, pLabelCount);
    gen_local_node(pNode->rhs, pLVars, pLabelCount);
    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (pNode->kind) {
    case ND_ADD: // +
        printf("  add rax, rdi\n");
        break;
    case ND_SUB: // -
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL: // *
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV: // /
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_EQ:  // ==
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:  // !=
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:  // <
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:  // <=
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }

    printf("  push rax\n");
}

static void gen_def_func(const Node* pNode, int* pLabelCount) {
    char funcName[MAX_FUNC_NAME] = { 0 };

    if (MAX_FUNC_NAME <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "関数名が%d文字以上あります", MAX_FUNC_NAME);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    //TODO:pNode->childrenから引数を取得する

    LVar* pLVars = NULL;
    int stack_size = resigter_lvars(&pLVars, pNode);

    printf("%s:\n", funcName);

    // プロローグ
    // ローカル変数が必要とする分の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_local_node(pNode->lhs, pLVars, pLabelCount);

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_global_node(const Node* pNode, int* pLabelCount) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
        return;
    }

    switch (pNode->kind) {
    case ND_NOP:
        // 何もしない
        return;
    case ND_TOP_LEVEL:
        // 関数定義外のトップレベル層
        gen_global_node(pNode->lhs, pLabelCount);

        // 継続ノードがあるならそれを評価
        if (pNode->rhs) gen_global_node(pNode->rhs, pLabelCount);
        return;
    case ND_DEF_FUNC:
        // 関数定義
        gen_def_func(pNode, pLabelCount);
        return;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }
}

void gen(const Node* pNode) {
    int labelCount = 0;

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_global_node(pNode, &labelCount);
}
