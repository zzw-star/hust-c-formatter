/*
 * lexer.h — 词法分析模块
 * 功能：gettoken() 从源文件读取一个单词，返回其种类编码，
 *       单词自身值存入全局 token_text
 * 输入：源文件指针 fp
 * 输出：单词种类编码；token_text 保存单词自身值
 */
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "token.h"

/*
 * gettoken — 词法分析（阶段2 完整实现）
 * 功能：从源文件中读取一个单词
 * 输入：fp 源文件指针（已打开）
 * 输出：返回单词种类编码（enum token_kind）；
 *       单词自身值写入全局 token_text；
 *       内部维护行号计数（每次回车 +1）
 */
int gettoken(FILE *fp);

/*
 * token_name — 把种类编码翻译成可读名字（调试/检查用）
 * 输入：kind 种类编码（enum token_kind 的值）
 * 输出：返回静态字符串名字，如 kind==IDENT 返回 "IDENT"
 */
const char *token_name(int kind);

/* 行号计数器（全局）：词法层维护，语法层报错时引用 */
extern int line_no;

#endif /* LEXER_H */
