#include <windows.h>

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

#define MAX_FUNC_NAME_LEN (64)

typedef struct Type Type;
typedef struct GVar GVar;
typedef struct LVar LVar;
typedef struct GlobalContext GlobalContext;
typedef struct FuncContext FuncContext;

struct Type {
    enum { TY_VOID, TY_CHAR, TY_INT, TY_PTR, TY_ARRAY } ty;
    const Type* ptr_to;
    size_t array_size;
    bool is_lvalue;
};
static const Type VOID_TYPE = { TY_VOID };
static const Type CHAR_TYPE = { TY_CHAR };
static const Type CHAR_PTR_TYPE = { TY_PTR, &CHAR_TYPE };
static const Type INT_TYPE = { TY_INT };

// グローバル変数の型
struct GVar {
    GVar* next;             // 次の変数かNULL
    Type* pType;            // 変数の型
    const char* name;       // 変数の名前
    int len;                // 名前の長さ
};

// ローカル変数の型
struct LVar {
    LVar* next;             // 次の変数かNULL
    Type* pType;            // 変数の型
    const char* name;       // 変数の名前
    int len;                // 名前の長さ
    int offset;             // RBPからのオフセット
};

// グローバルの環境
struct GlobalContext {
    int labelCount;
    GVar* pGVars;           // グローバル変数テーブル
    //TODO:関数テーブルもここ
};

// 関数定義内の環境
struct FuncContext {
    LVar* pLVars;           // ローカル変数テーブル（引数も展開する）
};

#define PARAM_REG_INDEX_64BIT  (3)
#define PARAM_REG_INDEX_32BIT  (2)
#define PARAM_REG_INDEX_16BIT  (1)
#define PARAM_REG_INDEX_8BIT   (0)
static const char PARAM_REG_NAME[][4][4] = {
    {  "cl",  "dl", "r8b", "r9b" },
    {  "cx",  "dx", "r8w", "r9w" },
    { "ecx", "edx", "r8d", "r9d" },
    { "rcx", "rdx", "r8" , "r9"  },
};
_STATIC_ASSERT(sizeof(PARAM_REG_NAME[0]) / sizeof(PARAM_REG_NAME[0][0]) == sizeof(((Node*)0)->children) / sizeof(((Node*)0)->children[0]));

static const Type* gen_left_expr(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static void gen_if_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static void gen_while_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static void gen_for_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static const Type* gen_invoke_expr(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static const Type* gen_add_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_sub_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_mul_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_div_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType);
static const Type* gen_local_node(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext);
static void gen_def_func(const Node* pNode, GlobalContext* pGlobalContext);
static void gen_global_node(const Node* pNode, GlobalContext* pGlobalContext);

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static const LVar* find_lvar(const LVar* pLVarTop, const Node* pNode) {
    for (const LVar* pVar = pLVarTop; pVar; pVar = pVar->next) {
        if (pVar->len == pNode->pToken->len && !memcmp(pNode->pToken->str, pVar->name, pVar->len)) {
            return pVar;
        }
    }
    return NULL;
}

static const GVar* find_gvar(const GVar* pGVarTop, const Node* pNode) {
    for (const GVar* pVar = pGVarTop; pVar; pVar = pVar->next) {
        if (pVar->len == pNode->pToken->len && !memcmp(pNode->pToken->str, pVar->name, pVar->len)) {
            return pVar;
        }
    }
    return NULL;
}

static size_t get_type_size(const Type* pType) {
    switch (pType->ty) {
    case TY_CHAR:
        return 1;
    case TY_INT:
        return 4;
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return pType->array_size * get_type_size(pType->ptr_to);
    default:
        error("Internal Error. Invalid Type '%d'.", pType->ty);
        return 0;
    }
}

static void eval_var(const Type* pType, const char* pszRegName) {
    switch (pType->ty) {
    case TY_CHAR:
        printf("  movsx %s, BYTE PTR [%s]\n", pszRegName, pszRegName);
        break;
    case TY_INT:
        printf("  movsx %s, DWORD PTR [%s]\n", pszRegName, pszRegName);
        break;
    case TY_PTR:
        printf("  mov %s, [%s]\n", pszRegName, pszRegName);
        break;
    case TY_ARRAY:
        //ポインタ型はその指し示す先にある値を取り出すことで評価となるが、配列型はその指し示す先にある値は[0]の要素そのもの
        //raxに格納されている値は配列型を指し示すポインタであるため、何もしないことで配列型変数の評価となる
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pType->ty);
    }
}

static Type* parse_type(const Node* pNode) {
    Type* pType = calloc(1, sizeof(Type));

    if (pNode->kind != ND_TYPE) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "型名が必要です");
    }

    switch (pNode->pToken->kind) {
    case TK_CHAR:
        pType->ty = TY_CHAR;
        break;
    case TK_INT:
        pType->ty = TY_INT;
        break;
    default:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "未定義の型名です");
    }

    const Node* pCurNode = pNode;
    while (pCurNode->rhs != NULL) {
        Type* pNewType = calloc(1, sizeof(Type));
        pNewType->ptr_to = pType;

        switch (pCurNode->rhs->kind) {
        case ND_DEREF:
            pNewType->ty = TY_PTR;
            break;
        case ND_NUM:
            pNewType->ty = TY_ARRAY;
            pNewType->array_size = pCurNode->rhs->pToken->val;
            break;
        default:
            error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        }

        pType = pNewType;
        pCurNode = pCurNode->rhs;
    }

    return pType;
}

// 変数を登録し、総消費スタックサイズを返す。
static int resigter_lvars(FuncContext* pContext, const Node* pNode) {
    if (pNode->kind == ND_DECL_VAR) {
        if (find_lvar(pContext->pLVars, pNode) != NULL) {
            error_at(pNode->pToken->user_input, pNode->pToken->str, "ローカル変数名が重複しています");
        }

        LVar* pVar = calloc(1, sizeof(LVar));
        pVar->next = pContext->pLVars;
        pVar->pType = parse_type(pNode->lhs);
        pVar->pType->is_lvalue = true;
        pVar->name = pNode->pToken->str;
        pVar->len = pNode->pToken->len;
        pVar->offset = (pContext->pLVars ? pContext->pLVars->offset : 0) + (int)get_type_size(pVar->pType);
        pContext->pLVars = pVar;
    }
    else {
        if (pNode->lhs) resigter_lvars(pContext, pNode->lhs);
        if (pNode->rhs) resigter_lvars(pContext, pNode->rhs);
    }

    return (pContext->pLVars ? pContext->pLVars->offset : 0);
}

// 引数を登録し、引数の数を返す。
static int resigter_params(FuncContext* pContext, const Node* pNode) {
    int paramNum;

    for (paramNum = 0; paramNum < sizeof(pNode->children) / sizeof(pNode->children[0]); ++paramNum) {
        if (pNode->children[paramNum] == NULL) break;

        if (find_lvar(pContext->pLVars, pNode->children[paramNum]) != NULL) {
            error_at(pNode->children[paramNum]->pToken->user_input, pNode->children[paramNum]->pToken->str, "引数名が重複しています");
        }

        resigter_lvars(pContext, pNode->children[paramNum]);
    }

    return paramNum;
}

static const Type* gen_left_expr(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
    if (pNode->kind == ND_VAR) {
        const LVar* pLVar = find_lvar(pContext->pLVars, pNode);
        if (pLVar != NULL) {
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", pLVar->offset);
            printf("  push rax\n");
            return pLVar->pType;
        }

        const GVar* pGVar = find_gvar(pGlobalContext->pGVars, pNode);
        if (pGVar != NULL) {
            char pszFormat[64] = { 0 };
            snprintf(pszFormat, sizeof(pszFormat), "  lea rax, %%.%ds[rip]\n", pGVar->len);
            printf(pszFormat, pGVar->name);
            printf("  push rax\n");
            return pGVar->pType;
        }

        error_at(pNode->pToken->user_input, pNode->pToken->str, "未定義の変数です");
        return NULL;
    }
    else if (pNode->kind == ND_DEREF) {
        // 単項*
        const Type* pType = gen_local_node(pNode->lhs, pGlobalContext, pContext);
        if (pType->ty != TY_PTR && pType->ty != TY_ARRAY) {
            error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタ型ではない値はデリファレンスできません");
        }

        if (pType->ptr_to->is_lvalue) {
            return pType->ptr_to;
        }
        else {
            Type* pResultType = calloc(1, sizeof(Type));
            memcpy(pResultType, pType->ptr_to, sizeof(Type));
            pResultType->is_lvalue = true;
            return pResultType;
        }
    }
    else {
        const Token* pCurToken = pNode->pToken;
        const Type* pType = gen_local_node(pNode, pGlobalContext, pContext);
        if (!pType->is_lvalue) {
            error_at(pCurToken->user_input, pCurToken->str, "無効な左辺値です");
        }
        return pType;
    }
}

static void gen_if_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
    const int endLabelId = pGlobalContext->labelCount++;

    // 条件式を評価
    gen_local_node(pNode->children[0], pGlobalContext, pContext);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    if (pNode->rhs) {
        const int elseLabelId = pGlobalContext->labelCount++;

        // 条件式が偽(0)ならelseラベルへジャンプ
        printf("  je  .Lelse%04d\n", elseLabelId);

        // 条件式が真なら(elseラベルへジャンプしていないなら)if-branchを評価し、endラベルへジャンプ
        gen_local_node(pNode->lhs, pGlobalContext, pContext);
        printf("  jmp .Lend%04d\n", endLabelId);

        // elseラベルではelse-branchを実行（endラベルへは自然と落ちるためジャンプ不要）
        printf(".Lelse%04d:\n", elseLabelId);
        gen_local_node(pNode->rhs, pGlobalContext, pContext);
    }
    else {
        // 条件式が偽(0)ならendラベルへジャンプ
        printf("  je  .Lend%04d\n", endLabelId);

        // 条件式が真なら(elseラベルへジャンプしていないなら)if-branchを実行
        gen_local_node(pNode->lhs, pGlobalContext, pContext);
    }

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_while_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
    const int beginLabelId = pGlobalContext->labelCount++;
    const int endLabelId = pGlobalContext->labelCount++;

    printf(".Lbegin%04d:\n", beginLabelId);

    // 条件式を評価
    gen_local_node(pNode->lhs, pGlobalContext, pContext);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");

    // 条件式が偽(0)ならendラベルへジャンプ
    printf("  je  .Lend%04d\n", endLabelId);

    // ループ対象の文を実行
    gen_local_node(pNode->rhs, pGlobalContext, pContext);

    // ループするためにbeginラベルへ無条件ジャンプ
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static void gen_for_stmt(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
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
    const int beginLabelId = pGlobalContext->labelCount++;
    const int endLabelId = pGlobalContext->labelCount++;

    // 初期化式を評価
    if (pNode->children[0]) {
        gen_local_node(pNode->children[0], pGlobalContext, pContext);
        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
    }

    printf(".Lbegin%04d:\n", beginLabelId);

    // 条件式を評価
    if (pNode->children[1]) {
        gen_local_node(pNode->children[1], pGlobalContext, pContext);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");

        // 条件式が偽(0)ならendラベルへジャンプ
        printf("  je  .Lend%04d\n", endLabelId);
    }

    // ループ対象の文を実行
    gen_local_node(pNode->rhs, pGlobalContext, pContext);

    // ループごとに評価する式を評価
    if (pNode->children[2]) {
        gen_local_node(pNode->children[2], pGlobalContext, pContext);
        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
    }

    // ループするためにbeginラベルへ無条件ジャンプ
    printf("  jmp .Lbegin%04d\n", beginLabelId);

    printf(".Lend%04d:\n", endLabelId);
}

static const Type* gen_invoke_expr(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
    int i;
    char funcName[MAX_FUNC_NAME_LEN + 1] = { 0 };

    if (MAX_FUNC_NAME_LEN <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "関数名が%d文字以上あります", MAX_FUNC_NAME_LEN);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    // 引数を順に評価して、対応するレジスタに格納
    for (i = 0; i < sizeof(pNode->children) / sizeof(pNode->children[0]); ++i) {
        if (pNode->children[i] == NULL) break;

        gen_local_node(pNode->children[i], pGlobalContext, pContext);
        printf("  pop %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_64BIT][i]);
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

    // TODO: 関数テーブルが無いので、int型戻り値の関数呼び出しと仮定している
    return &INT_TYPE;
}

static const Type* gen_add_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    const Type* pResultType = NULL;

    switch (pLhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        switch (pRhsType->ty) {
        case TY_CHAR:
        case TY_INT:
            pResultType = &INT_TYPE;
            break;
        case TY_PTR:
        case TY_ARRAY:
            //左辺値の整数値をポインタが指す先の型サイズ倍する
            printf("  imul rax, %zd\n", get_type_size(pRhsType->ptr_to));
            pResultType = pRhsType;
            break;
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    case TY_PTR:
    case TY_ARRAY:
        switch (pRhsType->ty) {
        case TY_CHAR:
        case TY_INT:
            //右辺値の整数値をポインタが指す先の型サイズ倍する
            printf("  imul rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            pResultType = pLhsType;
            break;
        case TY_PTR:
        case TY_ARRAY:
            error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタ同士の加算はできません");
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    printf("  add rax, rdi\n");

    return pResultType;
}

static const Type* gen_sub_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    const Type* pResultType = NULL;

    switch (pLhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        switch (pRhsType->ty) {
        case TY_CHAR:
        case TY_INT:
            pResultType = &INT_TYPE;
            break;
        case TY_PTR:
        case TY_ARRAY:
            error_at(pNode->pToken->user_input, pNode->pToken->str, "整数値からポインタの減算はできません");
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    case TY_PTR:
    case TY_ARRAY:
        switch (pRhsType->ty) {
        case TY_CHAR:
        case TY_INT:
            //右辺値の整数値をポインタが指す先の型サイズ倍する
            printf("  imul rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            pResultType = pLhsType;
            break;
        case TY_PTR:
        case TY_ARRAY:
            //ポインタ同士の減算はptrdiff_t型になる
            if (pLhsType->ptr_to->ty != pRhsType->ptr_to->ty) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "減算するポインタの型が一致していません");
            }
            printf("  sub rax, rdi\n");

            //ポインタが指す先の型サイズで除算することで、二つの配列要素の添字の差になる
            printf("  mov rdi, %zd\n", get_type_size(pLhsType->ptr_to));
            printf("  cqo\n");
            printf("  idiv rdi\n");

            return &INT_TYPE;   // 減算してから後処理で値調整入るので、ポインタ同士の減算は共通処理にしない
        default:
            error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
        }
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    printf("  sub rax, rdi\n");

    return pResultType;
}

static const Type* gen_mul_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    switch (pLhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        break;
    case TY_PTR:
    case TY_ARRAY:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタの乗算はできません");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    switch (pRhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        break;
    case TY_PTR:
    case TY_ARRAY:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタの乗算はできません");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
    }

    printf("  imul rax, rdi\n");
    return &INT_TYPE;
}

static const Type* gen_div_expr(const Node* pNode, const Type* pLhsType, const Type* pRhsType) {
    switch (pLhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        break;
    case TY_PTR:
    case TY_ARRAY:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタの除算はできません");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
    }

    switch (pRhsType->ty) {
    case TY_CHAR:
    case TY_INT:
        break;
    case TY_PTR:
    case TY_ARRAY:
        error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタの除算はできません");
        break;
    default:
        error("Internal Error. Invalid Type '%d'.", pRhsType->ty);
    }

    printf("  cqo\n");
    printf("  idiv rdi\n");
    return &INT_TYPE;
}

static const Type* gen_local_node(const Node* pNode, GlobalContext* pGlobalContext, const FuncContext* pContext) {
    if (!pNode) {
        error("Internal Error. Node is NULL.");
    }

    switch (pNode->kind) {
    case ND_NOP:
        // 何もしない
        return &VOID_TYPE;
    case ND_TYPE:
        // 型名（コードの出力はしない）
        return &VOID_TYPE;
    case ND_DECL_VAR:
        // 変数宣言（事前に登録済み：初期化子対応するならここ）
        return &VOID_TYPE;
    case ND_NUM:
        // 数値リテラル
        printf("  push %d\n", pNode->pToken->val);
        return &INT_TYPE;
    case ND_STRING:
        // 文字列リテラル
        printf("  lea rax, .LC%04d[rip]\n", pNode->pToken->val);
        printf("  push rax\n");
        return &CHAR_PTR_TYPE;
    case ND_VAR:
        // 変数
        {
            const Type* pType = gen_left_expr(pNode, pGlobalContext, pContext);
            printf("  pop rax\n");
            eval_var(pType, "rax");
            printf("  push rax\n");

            Type* pResultType = calloc(1, sizeof(Type));
            memcpy(pResultType, pType, sizeof(Type));
            pResultType->is_lvalue = false;
            return pResultType;
        }
    case ND_ADDR:
        // 単項&
        {
            Type* pResultType = calloc(1, sizeof(Type));
            pResultType->ty = TY_PTR;
            pResultType->ptr_to = gen_left_expr(pNode->lhs, pGlobalContext, pContext);
            return pResultType;
        }
    case ND_DEREF:
        // 単項*
        {
            const Type* pResultType = gen_local_node(pNode->lhs, pGlobalContext, pContext);
            if (pResultType->ty != TY_PTR && pResultType->ty != TY_ARRAY) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "ポインタ型ではない値はデリファレンスできません");
            }
            printf("  pop rax\n");
            eval_var(pResultType->ptr_to, "rax");
            printf("  push rax\n");
            return pResultType->ptr_to;
        }
    case ND_SIZEOF:
        // sizeof
        {
            // 一時的に標準出力を切ることで被演算子の評価を無効にする
            FILE* backup_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
            FILE* hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            SetStdHandle(STD_OUTPUT_HANDLE, hNull);

            const size_t size = get_type_size(gen_local_node(pNode->lhs, pGlobalContext, pContext));

            // 標準出力を元に戻す
            SetStdHandle(STD_OUTPUT_HANDLE, backup_stdout);
            CloseHandle(hNull);

            printf("  push %zd\n", size);
            return &INT_TYPE;
        }
    case ND_INVOKE:
        // 関数呼び出し
        return gen_invoke_expr(pNode, pGlobalContext, pContext);
    case ND_ASSIGN:
        // 代入演算
        {
            const Type* pLhsType = gen_left_expr(pNode->lhs, pGlobalContext, pContext);
            const Type* pRhsType = gen_local_node(pNode->rhs, pGlobalContext, pContext);

            switch (pLhsType->ty) {
            case TY_CHAR:
                printf("  pop rax\n");
                printf("  movsx rdi, al\n");
                printf("  pop rax\n");
                printf("  mov [rax], dil\n");
                break;
            case TY_INT:
                printf("  pop rax\n");
                printf("  movsx rdi, eax\n");
                printf("  pop rax\n");
                printf("  mov [rax], edi\n");
                break;
            case TY_PTR:
            case TY_ARRAY:
                printf("  pop rdi\n");
                printf("  pop rax\n");
                printf("  mov [rax], rdi\n");
                break;
            default:
                error("Internal Error. Invalid Type '%d'.", pLhsType->ty);
            }
            printf("  push rdi\n");
            return pRhsType;
        }
    case ND_BLOCK:
        // ブロック
        gen_local_node(pNode->lhs, pGlobalContext, pContext);

        // 継続文があるならそれを評価
        if (pNode->rhs) gen_local_node(pNode->rhs, pGlobalContext, pContext);
        return &VOID_TYPE;
    case ND_EXPR_STMT:
        // 式文
        gen_local_node(pNode->lhs, pGlobalContext, pContext);

        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
        return &VOID_TYPE;
    case ND_RETURN:
        // return文
        gen_local_node(pNode->lhs, pGlobalContext, pContext);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return &VOID_TYPE;
    case ND_IF:
        // if文
        gen_if_stmt(pNode, pGlobalContext, pContext);
        return &VOID_TYPE;
    case ND_WHILE:
        // while文
        gen_while_stmt(pNode, pGlobalContext, pContext);
        return &VOID_TYPE;
    case ND_FOR:
        // for文
        gen_for_stmt(pNode, pGlobalContext, pContext);
        return &VOID_TYPE;
    }

    // 二項演算
    const Type* pLhsType = gen_local_node(pNode->lhs, pGlobalContext, pContext);
    const Type* pRhsType = gen_local_node(pNode->rhs, pGlobalContext, pContext);
    const Type* pResultType = NULL;
    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (pNode->kind) {
    case ND_ADD: // +
        pResultType = gen_add_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_SUB: // -
        pResultType = gen_sub_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_MUL: // *
        pResultType = gen_mul_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_DIV: // /
        pResultType = gen_div_expr(pNode, pLhsType, pRhsType);
        break;
    case ND_EQ:  // ==
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_NE:  // !=
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_LT:  // <
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    case ND_LE:  // <=
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        pResultType = &INT_TYPE;
        break;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
    }

    printf("  push rax\n");
    return pResultType;
}

static void gen_def_func(const Node* pNode, GlobalContext* pGlobalContext) {
    int i;
    FuncContext context = { 0 };
    char funcName[MAX_FUNC_NAME_LEN + 1] = { 0 };

    if (MAX_FUNC_NAME_LEN <= pNode->pToken->len) {
        error_at(pNode->pToken->user_input, pNode->pToken->str, "関数名が%d文字以上あります", MAX_FUNC_NAME_LEN);
    }
    memcpy(funcName, pNode->pToken->str, pNode->pToken->len);

    int paramNum = resigter_params(&context, pNode);
    const LVar* pParamTop = context.pLVars;

    const int stack_size = resigter_lvars(&context, pNode);

    printf("%s:\n", funcName);

    // プロローグ
    // ローカル変数が必要とする分の領域を確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", stack_size);

    // 引数を対応するローカル変数に展開する
    for (i = 0; i < paramNum; ++i) {
        if (pParamTop == NULL) {
            error("Internal Error. Param node is NULL.");
        }
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", pParamTop->offset);
        switch (pParamTop->pType->ty) {
        case TY_CHAR:
            printf("  mov [rax], %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_8BIT][paramNum - i - 1]);
            break;
        case TY_INT:
            printf("  mov [rax], %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_32BIT][paramNum - i - 1]);
            break;
        case TY_PTR:
        case TY_ARRAY:
            printf("  mov [rax], %s\n", PARAM_REG_NAME[PARAM_REG_INDEX_64BIT][paramNum - i - 1]);
            break;
        default:
            error("Internal Error. Invalid Type '%d'.", pParamTop->pType->ty);
        }
        pParamTop = pParamTop->next;
    }

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_local_node(pNode->rhs, pGlobalContext, &context);

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_global_node(const Node* pNode, GlobalContext* pGlobalContext) {
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
        gen_global_node(pNode->lhs, pGlobalContext);

        // 継続ノードがあるならそれを評価
        if (pNode->rhs) gen_global_node(pNode->rhs, pGlobalContext);
        return;
    case ND_DEF_FUNC:
        // 関数定義
        gen_def_func(pNode, pGlobalContext);
        return;
    case ND_DECL_VAR:
        // グローバル変数宣言（事前に登録済み）
        return;
    default:
        error("Internal Error. Invalid NodeKind '%d'.", pNode->kind);
        return;
    }
}

// グローバル変数を登録する
static void resigter_gvars(GlobalContext* pGlobalContext, const Node* pNode) {
    if (!pNode) return;

    switch (pNode->kind) {
    case ND_TOP_LEVEL:
        resigter_gvars(pGlobalContext, pNode->lhs);
        resigter_gvars(pGlobalContext, pNode->rhs);
        break;
    case ND_DECL_VAR:
        {
            if (find_gvar(pGlobalContext->pGVars, pNode) != NULL) {
                error_at(pNode->pToken->user_input, pNode->pToken->str, "グローバル変数名が重複しています");
            }

            GVar* pVar = calloc(1, sizeof(GVar));
            pVar->next = pGlobalContext->pGVars;
            pVar->pType = parse_type(pNode->lhs);
            pVar->pType->is_lvalue = true;
            pVar->name = pNode->pToken->str;
            pVar->len = pNode->pToken->len;
            pGlobalContext->pGVars = pVar;

            char pszFormat[64] = { 0 };
            snprintf(pszFormat, sizeof(pszFormat), "%%.%ds:\n", pVar->len);
            printf(pszFormat, pVar->name);
            printf("  .zero %zd\n", get_type_size(pVar->pType));
        }
        break;
    }
}

void resigter_str_literals(const StringLiteral* pStrLiterals) {
    int i = 0;
    while (pStrLiterals) {
        printf(".LC%04d:\n", i++);
        printf("  .string %s\n", pStrLiterals->pszText);
        pStrLiterals = pStrLiterals->pNext;
    }
}

void gen(const Node* pNode, const StringLiteral* pStrLiterals) {
    GlobalContext globalContext = { 0 };

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");

    // 文字列リテラルの登録
    printf(".data\n");
    resigter_str_literals(pStrLiterals);

    // グローバル変数の登録
    printf(".bss\n");
    resigter_gvars(&globalContext, pNode);

    printf(".text\n");
    printf(".globl main\n");

    // 各ノードの解析を行いアセンブリを順次出力する
    gen_global_node(pNode, &globalContext);
}
