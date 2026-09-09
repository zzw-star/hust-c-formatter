/*
 * error.c — 报错模块实现
 * 功能：错误计数与统一的错误打印出口
 * 输入：行号、错误描述
 * 输出：stderr 输出；errors 计数 +1
 */
#include <stdio.h>
#include "error.h"

int errors = 0;

void lex_error(int line, const char *msg)
{
    errors++;
    fprintf(stderr, "词法错误 第%d行: %s\n", line, msg);
}

void syntax_error(int line, const char *msg)
{
    errors++;
    fprintf(stderr, "语法错误 第%d行: %s\n", line, msg);
}
