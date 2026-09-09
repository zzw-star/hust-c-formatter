/*
 * parser.h — 语法分析模块
 * 功能：递归下降法分析源程序的语法结构，生成抽象语法树 AST
 * 输入：源文件指针 fp（已打开）
 * 输出：程序根结点指针；语法错误时返回 NULL
 */
#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "ast.h"

/*
 * program — 语法分析入口（阶段3 实现）
 * 功能：分析整个程序，生成 AST
 * 输入：fp 源文件指针
 * 输出：成功返回根结点指针，失败返回 NULL
 */
ASTNode *program(FILE *fp);

/* parser_init：在 program 之前调用一次（初始化运算符优先关系表 + 装第一个 token） */
void parser_init(FILE *fp);

/* exp(endsym)：分析表达式，到 endsym 结束；返回表达式 AST 根节点
 * endsym 是表达式结束符：SEMI/COMMA/RP/RB 等
 * 这是 B1 阶段的核心子程序（双栈算法 + 优先关系表） */
ASTNode *parse_expr(int endsym);

#endif /* PARSER_H */
