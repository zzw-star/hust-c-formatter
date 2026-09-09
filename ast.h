/*
 * ast.h — 抽象语法树（AST）定义
 * 功能：定义 AST 结点类型（tag + union 属性 + 孩子链），
 *       以及建树、销毁、显示接口
 * 输入：无
 * 输出：ASTNode 类型与相关函数声明
 *
 * 说明：结点逻辑上是多叉树，物理上用"第一孩子 + 第二孩子"链
 *       表示（类似广义表），满足任务书"孩子表示法/孩子兄弟法"要求；
 *       用 tag 标记结点种类、union 存放各结点专属信息。
 */
#ifndef AST_H
#define AST_H

/* 结点种类标记：每个值对应一种语法单位 */
typedef enum {
    NODE_PROGRAM,           /* 程序 */
    NODE_EXT_DEF_LIST,      /* 外部定义序列 */
    NODE_EXT_DEF,           /* 外部定义 */
    NODE_EXT_VAR_DEF,       /* 外部变量定义 */
    NODE_VAR_LIST,          /* 变量序列 */
    NODE_FUNC_DEF,          /* 函数定义 */
    NODE_FUNC_DECL,         /* 函数声明（原型） */
    NODE_PARAM_LIST,        /* 形式参数序列 */
    NODE_COMPOUND,          /* 复合语句 { ... } */
    NODE_STMT_LIST,         /* 语句序列 */
    NODE_IF,                /* if 语句（2棵子树：条件、IF子句） */
    NODE_IF_ELSE,           /* if-else 语句（3棵子树：条件、IF子句、ELSE子句） */
    NODE_WHILE,             /* while 语句 */
    NODE_FOR,               /* for 语句 */
    NODE_RETURN,            /* return 语句 */
    NODE_BREAK,             /* break 语句 */
    NODE_CONTINUE,          /* continue 语句 */
    NODE_EXPR_STMT,         /* 表达式语句 */
    NODE_ASSIGN,            /* 赋值表达式 */
    NODE_BINOP,             /* 双目运算（算术/关系/逻辑） */
    NODE_FUNC_CALL,         /* 函数调用 */
    NODE_ARG_LIST,          /* 实参序列 */
    NODE_ARRAY_INDEX,       /* 数组下标访问 a[i] */
    NODE_IDENT,             /* 标识符 */
    NODE_INT_CONST,         /* 整型常量 */
    NODE_LONG_CONST,        /* 长整型常量 */
    NODE_FLOAT_CONST,       /* 浮点常量 */
    NODE_CHAR_CONST,        /* 字符常量 */
    NODE_TYPE,               /* 类型结点：int / float / char */
    NODE_PREPROCESS,         /* 编译预处理行：#include/#define（任务书 3.3.3(5)） */
    NODE_COMMENT             /* 注释：行注释 // 与块注释（任务书 3.3.3(5)） */
} NodeTag;

/* AST 结点 */
typedef struct ASTNode {
    NodeTag tag;            /* 结点种类 */
    union {                 /* 不同种类结点各自的专属信息 */
        struct { char name[64]; } ident;        /* NODE_IDENT：标识符名 */
        struct { int value; char text[64]; } int_const;    /* NODE_INT_CONST：值 + 字面量 */
        struct { long value; char text[64]; } long_const;  /* NODE_LONG_CONST */
        struct { float value; char text[64]; } float_const;/* NODE_FLOAT_CONST */
        struct { char value; char text[64]; } char_const;  /* NODE_CHAR_CONST */
        struct { char name[64]; } type;         /* NODE_TYPE：类型名 int/float/char */
        struct { char op; } binop;              /* NODE_BINOP：运算符 + - * / % == 等 */
        struct { char text[256]; } raw;         /* NODE_PREPROCESS/NODE_COMMENT：原文整段保留 */
    } attr;
    struct ASTNode *child1; /* 第一棵子树（孩子链） */
    struct ASTNode *child2; /* 第二棵子树（兄弟链/下一子树） */
} ASTNode;

/* 新建一个指定种类的结点（孩子指针初始为 NULL） */
ASTNode *new_node(NodeTag tag);

/* 释放以 root 为根的整棵树（递归后序遍历） */
void free_tree(ASTNode *root);

/* 先根遍历显示 AST（缩进文本形式，阶段5 完成） */
void print_tree(ASTNode *root, int depth);

#endif /* AST_H */
