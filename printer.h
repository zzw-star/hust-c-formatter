/*
 * printer.h — 格式化输出模块
 * 功能：对 AST 先根遍历，按选定的缩进规范生成格式化源程序
 * 输入：输出文件指针 out、AST 根结点指针 root
 * 输出：向 out 写入缩进编排后的源程序
 */
#ifndef PRINTER_H
#define PRINTER_H

#include <stdio.h>
#include "ast.h"

/*
 * format_source — 格式化输出（阶段5 实现）
 * 功能：遍历 AST，按缩进规范写源程序到 out
 * 输入：out 输出文件指针（已打开，如目标 .c 文件）；root AST 根结点
 * 输出：向 out 写入格式化代码
 */
void format_source(FILE *out, ASTNode *root);

#endif /* PRINTER_H */
