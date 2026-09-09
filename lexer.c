/*
 * lexer.c — 词法分析模块实现
 * 功能：按 DFA 状态转换图识别全部单词（阶段2 完成）
 * 输入：源文件指针 fp
 * 输出：单词种类编码 + 全局 token_text
 *
 * 阶段0说明：本文件先提供可编译的空壳与全局缓冲区定义，
 *           gettoken() 的完整 DFA 实现在阶段2 填充。
 * 阶段2任务清单（对应任务书 3.3）：
 *   1. 标识符/关键字（查关键字表）        ← 已完成（本次）
 *   2. 整型常量 123 / 0123 / 0x123，后缀 L
 *   3. 浮点常量 1.5 / .5 / 1. / F 后缀
 *   4. 字符常量 'a' 及转义 '\n' '\0'
 *   5. 双字符运算符 == != <= >= && ||（需前看一位 + ungetc 退回）
 *   6. 单字符运算符与定界符 + - * / % = ( ) [ ] { } ; ,
 *   7. 注释：块注释与 // 行注释（跳过时换行照常计数）
 *   8. 预处理：# 开头整行跳过（行号 +1）
 *   9. 空白：空格、制表符跳过；回车跳过但行号 +1
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "error.h"

/* 全局单词值缓冲区（token.h 中声明） */
char token_text[256];

/* 行号计数器：每次遇到 '\n' 加 1（注释与预处理跳过时也照常计数） */
int line_no = 1;

/*
 * 关键字查找表：标识符拼完后到这里查，
 * 命中返回该关键字对应的种类编码，否则它是普通标识符返回 IDENT。
 * （指导书原文：将所有关键字做成一个查找表）
 */
static const struct {
    const char *name;
    int kind;
} keywords[] = {
    {"int",      INT},
    {"float",    FLOAT},
    {"char",     CHAR},
    {"if",       IF},
    {"else",     ELSE},
    {"while",    WHILE},
    {"for",      FOR},
    {"return",   RETURN},
    {"break",    BREAK},
    {"continue", CONTINUE}
};

int gettoken(FILE *fp)
{
    int c;          /* 刚读入的字符：用 int 不用 char，因为要装得下 EOF */
    int len = 0;    /* token_text 中已写入字符的个数 */

    /* ① 初始化：单词自身值清空，别让上一次的残留混进来 */
    token_text[0] = '\0';

    /* ② 过滤空白符：空格 / 制表符 / 回车 / 换行 一律跳过 */
    /*    注意：回车要让行号计数器 +1（报错定位全靠它） */
    while ((c = fgetc(fp)) == ' ' || c == '\t' || c == '\n' || c == '\r') {
        if (c == '\n')
            line_no++;
    }

    /* ③ 跳过空白后文件已读完 → 返回文件结束标记 */
    if (c == EOF)
        return EOF_TOKEN;

    /* ③b 注释与预处理行（任务书 3.3.1 第 4/5 条）
     *   行注释 //   → 吞到换行（不吞换行，留给空白跳过），整行文本写入 token_text
     *   块注释      → 吞到结束符（支持跨行，维护 line_no），整段文本写入 token_text
     *   预处理 #    → 吞到换行，整行文本写入 token_text
     * 三者都返回各自的种类编码（而不跳过），因为任务书 3.3.3(5) 要求
     * 生成的抽象语法树包含编译预处理和注释。
     * 必须放在标识符/数字/字符/运算符之前 —— 否则 / 会被识别为 SLASH */
    if (c == '/') {
        int next = fgetc(fp);
        if (next == '/') {
            /* 行注释：吞到 \n（不吞换行），文本含 // */
            len = 0;
            token_text[len++] = '/';
            token_text[len++] = '/';
            while ((c = fgetc(fp)) != EOF && c != '\n') {
                if (len < 254) token_text[len++] = (char)c;
            }
            token_text[len] = '\0';
            if (c == '\n') ungetc(c, fp);   /* 换行留给空白处理，顺便计行号 */
            return COMMENT;
        }
        if (next == '*') {
            /* 块注释：吞到结束符，维护行号，文本含起始与结束标记 */
            int done = 0;
            len = 0;
            token_text[len++] = '/';
            token_text[len++] = '*';
            while (!done && (c = fgetc(fp)) != EOF) {
                if (c == '*') {
                    int n2 = fgetc(fp);
                    if (n2 == '/') {                  /* 遇到结束符 */
                        done = 1;
                        if (len < 253) token_text[len++] = '*';
                        if (len < 254) token_text[len++] = '/';
                        break;
                    }
                    if (len < 254) token_text[len++] = '*';
                    if (n2 != EOF) ungetc(n2, fp);    /* 退回，下次循环重新处理 */
                    continue;
                }
                if (c == '\n') line_no++;             /* 跨行照常计数 */
                if (len < 254) token_text[len++] = (char)c;
            }
            token_text[len] = '\0';
            /* 截断保护：注释太长被截断时，结尾必须仍是合法的块注释结束符 */
            if (len > 252) {
                token_text[252] = '*';
                token_text[253] = '/';
                token_text[254] = '\0';
            }
            if (!done) { lex_error(line_no, "块注释未闭合"); return ERROR_TOKEN; }
            return COMMENT;
        }
        /* / 后面不是 / 也不是 * → 退回 / 让 A3 处理 */
        if (next != EOF) ungetc(next, fp);
    }
    if (c == '#') {
        /* 预处理行：吞到 \n（不吞 \n），整行文本写入 token_text */
        len = 0;
        token_text[len++] = '#';
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            if (len < 255) token_text[len++] = (char)c;
        }
        token_text[len] = '\0';
        if (c == '\n') ungetc(c, fp);
        return PREPROCESS;
    }

    /* ④ 以字母或下划线开头：标识符 或 关键字（指导书伪代码的 if 分支） */
    /*    注意：C 标识符允许下划线，所以"字母"要放宽成"字母或下划线" */
    if (isalpha(c) || c == '_') {
        /* 边读边拼：下一个字符是字母、数字或下划线就继续，拼进 token_text */
        do {
            token_text[len++] = (char)c;
            c = fgetc(fp);
        } while (c != EOF && (isalnum(c) || c == '_'));

        /* 多读了一个字符，把它退回文件缓冲区（可能是下一个单词的开头） */
        if (c != EOF)
            ungetc(c, fp);

        token_text[len] = '\0';     /* C 字符串必须以 '\0' 结尾 */

        /* ⑤ 查关键字表：命中返回关键字编码，否则是普通标识符 */
        for (int i = 0; i < (int)(sizeof(keywords) / sizeof(keywords[0])); i++) {
            if (strcmp(token_text, keywords[i].name) == 0)
                return keywords[i].kind;
        }
        return IDENT;
    }

    /* 数字开头：整型 / 长整型 / 浮点
     * 任务书 3.3.2：123/0123/0x123→INT_CONST；123L→LONG_CONST；1.5/.5/1./1.5F→FLOAT_CONST */
    if (isdigit(c)) {
        int is_float = 0;

        /* 吃整数部分 */
        do {
            token_text[len++] = (char)c;
            c = fgetc(fp);
        } while (c != EOF && isdigit(c));

        /* 0x 开头：十六进制（追加 0-9 / a-f / A-F） */
        if (len == 1 && token_text[0] == '0' && (c == 'x' || c == 'X')) {
            token_text[len++] = (char)c;
            c = fgetc(fp);
            while (c != EOF && isxdigit(c)) {
                token_text[len++] = (char)c;
                c = fgetc(fp);
            }
        }

        /* 小数点：浮点（1.5 或 1.） */
        if (c == '.') {
            is_float = 1;
            token_text[len++] = '.';
            c = fgetc(fp);
            while (c != EOF && isdigit(c)) {
                token_text[len++] = (char)c;
                c = fgetc(fp);
            }
        }

        /* 浮点后缀 F / f（1.5F） */
        if (c == 'F' || c == 'f') {
            is_float = 1;
            token_text[len++] = (char)c;
            c = fgetc(fp);
        }

        /* 整型后缀 L / l（123L；浮点不认 L） */
        if (!is_float && (c == 'L' || c == 'l')) {
            token_text[len++] = (char)c;
            c = fgetc(fp);
            token_text[len] = '\0';
            if (c != EOF) ungetc(c, fp);
            return LONG_CONST;
        }

        token_text[len] = '\0';
        if (c != EOF) ungetc(c, fp);
        return is_float ? FLOAT_CONST : INT_CONST;
    }

    /* 小数点开头：浮点（.5） */
    if (c == '.') {
        int next = fgetc(fp);
        if (isdigit(next)) {
            token_text[len++] = '.';
            c = next;
            while (c != EOF && isdigit(c)) {
                token_text[len++] = (char)c;
                c = fgetc(fp);
            }
            if (c == 'F' || c == 'f') {
                token_text[len++] = (char)c;
                c = fgetc(fp);
            }
            token_text[len] = '\0';
            if (c != EOF) ungetc(c, fp);
            return FLOAT_CONST;
        }
        /* 单独的 . 不是合法 token */
        if (next != EOF) ungetc(next, fp);
        token_text[0] = '.'; token_text[1] = '\0';
        lex_error(line_no, "非法字符");
        return ERROR_TOKEN;
    }

    /* 字符常量：'a' / '\n' / '\t' / '\r' / '\0'  → CHAR_CONST
     * 任务书 3.3.2：字符常量编码为 CHAR_CONST
     * 支持：单字符 / 4 个常见转义 \n \t \r \0 */
    if (c == '\'') {
        len = 0;
        token_text[len++] = '\'';
        c = fgetc(fp);
        if (c == EOF) {
            lex_error(line_no, "字符常量未闭合（文件提前结束）");
            return ERROR_TOKEN;
        }
        if (c == '\\') {
            /* 转义：吞 \ 看下一字符 */
            token_text[len++] = '\\';
            c = fgetc(fp);
            if (c == EOF || (c != 'n' && c != 't' && c != 'r' && c != '0')) {
                lex_error(line_no, "字符常量含非法转义序列");
                return ERROR_TOKEN;
            }
            token_text[len++] = (char)c;
        } else {
            /* 普通字符（不含 ' 和换行） */
            if (c == '\'' || c == '\n') {
                lex_error(line_no, "字符常量格式错误");
                return ERROR_TOKEN;
            }
            token_text[len++] = (char)c;
        }
        /* 必须以 ' 收尾 */
        c = fgetc(fp);
        if (c != '\'') {
            lex_error(line_no, "字符常量缺少右单引号");
            return ERROR_TOKEN;
        }
        token_text[len++] = '\'';
        token_text[len] = '\0';
        return CHAR_CONST;
    }

    /* 双字符运算符 =/!/<>/&/| 系列（指导书伪代码标准套路：前看一位）
     * 必须放在单字符 switch 之前 ——  否则 switch 里的 ASSIGN 会先匹配
     * == != <= >= && ||  → EQ/NE/LE/GE/AND/OR
     * = < > 单独           → ASSIGN/GT/LT（任务书虽只列双字符，但单字符也是合法关系运算）
     * ! & | 单独           → 非法字符（任务书未要求） */
    if (c == '=' || c == '!' || c == '<' || c == '>' || c == '&' || c == '|') {
        char first = (char)c;
        int next = fgetc(fp);
        if (first == '=' && next == '=') { token_text[0]='='; token_text[1]='='; token_text[2]='\0'; return EQ; }
        if (first == '!' && next == '=') { token_text[0]='!'; token_text[1]='='; token_text[2]='\0'; return NE; }
        if (first == '<' && next == '=') { token_text[0]='<'; token_text[1]='='; token_text[2]='\0'; return LE; }
        if (first == '>' && next == '=') { token_text[0]='>'; token_text[1]='='; token_text[2]='\0'; return GE; }
        if (first == '&' && next == '&') { token_text[0]='&'; token_text[1]='&'; token_text[2]='\0'; return AND; }
        if (first == '|' && next == '|') { token_text[0]='|'; token_text[1]='|'; token_text[2]='\0'; return OR; }
        /* 不匹配双字符 → 退回多读的字符 */
        if (next != EOF) ungetc(next, fp);
        if (first == '=') { token_text[0]='='; token_text[1]='\0'; return ASSIGN; }
        if (first == '<') { token_text[0]='<'; token_text[1]='\0'; return LT; }
        if (first == '>') { token_text[0]='>'; token_text[1]='\0'; return GT; }
        /* ! & | 单独出现 → 非法字符 */
        token_text[0] = first; token_text[1] = '\0';
        lex_error(line_no, "非法字符");
        return ERROR_TOKEN;
    }

    /* 单字符运算符与定界符
     * 算术 + - * / %；定界符 ( ) [ ] { } ; ,
     * 赋值 = 已在上方双字符块处理（= 单独 → ASSIGN） */
    switch (c) {
        case '+': token_text[0] = '+'; token_text[1] = '\0'; return PLUS;
        case '-': token_text[0] = '-'; token_text[1] = '\0'; return MINUS;
        case '*': token_text[0] = '*'; token_text[1] = '\0'; return STAR;
        case '/': token_text[0] = '/'; token_text[1] = '\0'; return SLASH;
        case '%': token_text[0] = '%'; token_text[1] = '\0'; return PERCENT;
        case '(': token_text[0] = '('; token_text[1] = '\0'; return LP;
        case ')': token_text[0] = ')'; token_text[1] = '\0'; return RP;
        case '[': token_text[0] = '['; token_text[1] = '\0'; return LB;
        case ']': token_text[0] = ']'; token_text[1] = '\0'; return RB;
        case '{': token_text[0] = '{'; token_text[1] = '\0'; return LBRA;
        case '}': token_text[0] = '}'; token_text[1] = '\0'; return RBRA;
        case ';': token_text[0] = ';'; token_text[1] = '\0'; return SEMI;
        case ',': token_text[0] = ','; token_text[1] = '\0'; return COMMA;
        default: break;
    }

    /* 非法字符：不在任何单词定义中的符号（任务书要求报错带位置） */
    token_text[0] = (char)c; token_text[1] = '\0';
    lex_error(line_no, "非法字符");
    return ERROR_TOKEN;
}

/* 供调试/检查用：把种类编码翻译成可读名字（阶段2完成后使用） */
const char *token_name(int kind)
{
    static const char *names[] = {
        "ERROR_TOKEN", "EOF_TOKEN",
        "IDENT", "INT_CONST", "LONG_CONST", "FLOAT_CONST", "CHAR_CONST",
        "INT", "FLOAT", "CHAR",
        "IF", "ELSE", "WHILE", "FOR", "RETURN", "BREAK", "CONTINUE",
        "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "ASSIGN",
        "EQ", "NE", "GT", "GE", "LT", "LE", "AND", "OR",
        "LP", "RP", "LB", "RB", "LBRA", "RBRA", "SEMI", "COMMA",
        "PREPROCESS", "COMMENT"
    };
    if (kind >= 0 && kind < (int)(sizeof(names) / sizeof(names[0])))
        return names[kind];
    return "?";
}
