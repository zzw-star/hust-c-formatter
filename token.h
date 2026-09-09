/*
 * token.h — 单词种类定义
 * 功能：定义语言全部单词的种类编码（枚举）与 Token 结构体
 * 输入：无
 * 输出：种类编码常量、Token 结构体、全局单词值缓冲区
 *
 * 说明：种类编码是词法分析的核心输出，一个编码唯一对应一类单词。
 *       任务书要求：123 / 0123 / 0x123 同为 int（编码相同），
 *       而 123L 为 long（编码不同）——见 LONG_CONST。
 */
#ifndef TOKEN_H
#define TOKEN_H

/* 单词种类编码 */
enum token_kind {
    ERROR_TOKEN,        /* 非法字符 */
    EOF_TOKEN,          /* 文件结束 */

    IDENT,              /* 标识符，如 abc、i */
    INT_CONST,          /* 整型常量：123 / 0123 / 0x123（同为 int） */
    LONG_CONST,         /* 长整型常量：123L */
    FLOAT_CONST,        /* 浮点常量：1.5 / .5 / 1. / 1.5F */
    CHAR_CONST,         /* 字符常量：'a'、'\n'、'\0' */

    /* 类型关键字 */
    INT, FLOAT, CHAR,
    /* 语句关键字 */
    IF, ELSE, WHILE, FOR, RETURN, BREAK, CONTINUE,

    /* 算术运算符 */
    PLUS, MINUS, STAR, SLASH, PERCENT,      /* + - * / % */
    ASSIGN,                                 /* = */
    /* 关系运算符 */
    EQ, NE, GT, GE, LT, LE,                 /* == != > >= < <= */
    /* 逻辑运算符 */
    AND, OR,                                /* && || */

    /* 定界符 */
    LP, RP, LB, RB, LBRA, RBRA,             /* ( ) [ ] { } */
    SEMI, COMMA,                            /* ; , */

    PREPROCESS,                             /* # 预处理行（#define/#include） */
    COMMENT                                 /* 注释（块注释与行注释两种形式） */
};

/* 单词结构体：种类编码 + 自身值 + 行号 */
typedef struct {
    int kind;           /* 种类编码（取 enum token_kind 的值） */
    char text[256];     /* 单词自身值，如 "abc" "123" "int" "==" */
    int line;           /* 单词所在行号（报错用，词法层维护） */
} Token;

/* 全局单词值缓冲区（指导书要求：token_text 为全局变量） */
extern char token_text[256];

#endif /* TOKEN_H */
