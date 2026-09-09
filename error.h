/*
 * error.h — 报错模块
 * 功能：统一所有错误输出出口，维护全局错误计数
 * 输入：行号 + 错误描述
 * 输出：向 stderr 打印带行号的错误信息
 */
#ifndef ERROR_H
#define ERROR_H

/* 全局错误计数：任何模块出错时 +1，供上层判断是否继续 */
extern int errors;

/* 词法错误：打印 "词法错误 第N行: ..." */
void lex_error(int line, const char *msg);

/* 语法错误：打印 "语法错误 第N行: ..." */
void syntax_error(int line, const char *msg);

#endif /* ERROR_H */
