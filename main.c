/*
 * main.c �? 主控模块
 * 功能：解析命令行参数，调度词�?/语法/格式化各模块
 * 输入：命令行 argv[1] = 源程序文件路径；argv[2] = 可选参�? -t
 * 输出：阶�?0 逐字符打印源文件（带行号），验证文件读取通路�?
 *       后续阶段依次输出单词表、AST、格式化文件
 *
 * 用法�?./formatter test.c       完整处理
 *       ./formatter test.c -t    仅词法分析（阶段2 实现�?
 */
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "token.h"
#include "printer.h"
#ifdef _WIN32
int __stdcall SetConsoleOutputCP(unsigned int wCodePageID);
static void enable_utf8_console(void)
{
    SetConsoleOutputCP(65001);      /* 65001 = CP_UTF8，让中文正常显示 */
}
#endif

/* 打印用法说明 */
static void print_usage(const char *prog)
{
    printf("用法: %s <源程序文�?.c> [-t]\n", prog);
    printf("  -t  仅进行词法分析，显示单词表（阶段2 实现）\n");
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    enable_utf8_console();      /* 解决 Windows 下中文输出乱�? */
#endif

   
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    const char *file = argv[1];
    int lex_only = 0;
    int parse_only = 0;
    if (argc >= 3 && strcmp(argv[2], "-t") == 0)
        lex_only = 1;
    if (argc >= 3 && strcmp(argv[2], "-p") == 0)
        parse_only = 1;

    /* ---- 打开源程序文�? ---- */
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        fprintf(stderr, "错误: 无法打开文件 %s\n", file);
        return 1;
    }

    /* ---- 阶段0：验证文件读取通路 ---- */
    /* 按行读入并打印行号——行号正是词法报错位置的计数基础�?
       注：Windows 控制台默�? GBK，显�? UTF-8 中文注释会乱码，
       执行 chcp 65001 可切�? UTF-8；词法层会跳过注释，不影响功�? */
    printf("===== 阶段0: 读取文件（带行号�?=====\n");
    char buf[1024];
    int line = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        line++;
        buf[strcspn(buf, "\n")] = '\0';     /* 去掉行尾换行便于显示 */
        printf("%3d | %s\n", line, buf);
    }
    printf("===== 文件�? %d �? =====\n", line);

    /* ---- 阶段2：词法分析模式（-t�?---- */
    if (lex_only) {
        /* 阶段0 已把文件读到末尾，必须回到开头，gettoken 才能从头认字 */
        rewind(fp);
        printf("\n===== 阶段2: 单词表（按出现顺序）=====\n");
        int kind;
        /* 指导书：每调用一�? gettoken 显示一个单词，直到返回 EOF 为止 */
        while ((kind = gettoken(fp)) != EOF_TOKEN) {
            printf("%-12s : %s\n", token_name(kind), token_text);
        }
        printf("===== 单词识别结束 =====\n");

        /* ---- 加分项：按种类编码排序显示（任务书要求） ---- */
        rewind(fp);
        printf("\n===== 加分�?: 单词表（按种类编码排序）=====\n");
        enum { MAX_TOK = 10000 };
        static int  kinds[MAX_TOK];
        static char texts[MAX_TOK][256];
        int n = 0;
        while ((kind = gettoken(fp)) != EOF_TOKEN && n < MAX_TOK) {
            kinds[n] = kind;
            strncpy(texts[n], token_text, sizeof(texts[n]) - 1);
            texts[n][sizeof(texts[n]) - 1] = '\0';
            n++;
        }
        /* 冒泡排序：按种类编码从小到大（token 数有限，够快�? */
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (kinds[j] < kinds[i]) {
                    int tk = kinds[i]; kinds[i] = kinds[j]; kinds[j] = tk;
                    char tt[256];
                    strcpy(tt, texts[i]);
                    strcpy(texts[i], texts[j]);
                    strcpy(texts[j], tt);
                }
            }
        }
        /* 按种类分组显示，同一种类内值去�? */
        int i = 0;
        while (i < n) {
            int k = kinds[i];
            printf("%-12s :", token_name(k));
            while (i < n && kinds[i] == k) {
                /* 检�? texts[i] 是否在本组已出现过（去重�? */
                int dup = 0;
                for (int j = i - 1; j >= 0 && kinds[j] == k; j--) {
                    if (strcmp(texts[j], texts[i]) == 0) { dup = 1; break; }
                }
                if (!dup) printf(" %s", texts[i]);
                i++;
            }
            printf("\n");
        }
        printf("===== 排序显示结束（共 %d 个单词）=====\n", n);
        fclose(fp);
        return 0;
    }

    /* ---- 阶段3：表达式子程序测试（-p，仅 B1 阶段用） ---- */
    if (parse_only) {
        rewind(fp);
        parser_init(fp);
        printf("\n===== 阶段3-B1: 表达式子程序测试 =====\n");
        ASTNode *root = parse_expr(SEMI);
        if (root) {
            printf("表达式解析成功\n");
            print_tree(root, 0);
            free_tree(root);
        } else {
            printf("表达式解析失败\n");
        }
        fclose(fp);
        return 0;
    }

    /* ---- 阶段3：语法分�? + 阶段5：格式化输出（完整流程） ---- */
    rewind(fp);
    printf("\n===== 阶段3: 语法分析（AST 先根遍历�?=====\n");
    ASTNode *root = program(fp);
    if (root != NULL) {
        print_tree(root, 0);
        printf("===== 语法分析结束 =====\n");

        /* 阶段5：对 AST 先根遍历，生成格式化源程序文�? */
        char out_name[512];
        strcpy(out_name, file);
        char *dot = strrchr(out_name, '.');
        if (dot != NULL) *dot = '\0';
        strcat(out_name, "_fmt.c");

        FILE *outf = fopen(out_name, "w");
        if (outf == NULL) {
            fprintf(stderr, "错误: 无法创建输出文件 %s\n", out_name);
        } else {
            format_source(outf, root);
            fclose(outf);
            printf("\n===== 阶段5: 格式化输出已写入 %s =====\n", out_name);
        }
        free_tree(root);
    } else {
        printf("===== 语法分析失败 =====\n");
    }
    fclose(fp);
    return 0;
}
