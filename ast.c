/*
 * ast.c — AST 结点的建立与销毁
 * 功能：new_node 分配并初始化结点；free_tree 递归释放整棵树
 * 输入：结点种类标记 / 树根指针
 * 输出：结点指针 / 无（释放内存）
 *
 * 说明：出错时调用 free_tree 释放整棵已建成的树，
 *       这是任务书"程序规范"的一部分，也是报错处理的收尾动作。
 */
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "token.h"

ASTNode *new_node(NodeTag tag)
{
    ASTNode *p = (ASTNode *)malloc(sizeof(ASTNode));
    if (p == NULL) {
        fprintf(stderr, "内存不足\n");
        exit(1);
    }
    p->tag = tag;
    p->child1 = NULL;
    p->child2 = NULL;
    return p;
}

void free_tree(ASTNode *root)
{
    if (root == NULL)
        return;
    free_tree(root->child1);    /* 先释放第一棵子树 */
    free_tree(root->child2);    /* 再释放第二棵子树 */
    free(root);                 /* 最后释放自身 */
}

/* 二元运算符编码 → 字符串（print_tree 用） */
static const char *op_name(int k)
{
    switch (k) {
        case PLUS:    return "+";
        case MINUS:   return "-";
        case STAR:    return "*";
        case SLASH:   return "/";
        case PERCENT: return "%";
        case EQ:      return "==";
        case NE:      return "!=";
        case GT:      return ">";
        case GE:      return ">=";
        case LT:      return "<";
        case LE:      return "<=";
        case AND:     return "&&";
        case OR:      return "||";
        default:      return "?";
    }
}

void print_tree(ASTNode *root, int depth)
{
    /* 先根遍历 + 缩进打印，展示源程序语法单位与 AST 的对应关系 */
    if (root == NULL) return;
    for (int i = 0; i < depth; i++) printf("  ");

    switch (root->tag) {
        case NODE_PROGRAM:     printf("Program\n"); break;
        case NODE_EXT_DEF_LIST:printf("ExtDefList\n"); break;
        case NODE_EXT_DEF:     printf("ExtDef\n"); break;
        case NODE_EXT_VAR_DEF: printf("VarDef\n"); break;
        case NODE_VAR_LIST:    printf("VarList\n"); break;
        case NODE_FUNC_DEF:    printf("FuncDef\n"); break;
        case NODE_FUNC_DECL:   printf("FuncDecl\n"); break;
        case NODE_PARAM_LIST:  printf("ParamList\n"); break;
        case NODE_COMPOUND:    printf("Compound\n"); break;
        case NODE_STMT_LIST:   printf("StmtList\n"); break;
        case NODE_IF:          printf("If\n"); break;
        case NODE_IF_ELSE:     printf("IfElse\n"); break;
        case NODE_WHILE:       printf("While\n"); break;
        case NODE_FOR:         printf("For\n"); break;
        case NODE_RETURN:      printf("Return\n"); break;
        case NODE_BREAK:       printf("Break\n"); break;
        case NODE_CONTINUE:    printf("Continue\n"); break;
        case NODE_EXPR_STMT:   printf("ExprStmt\n"); break;
        case NODE_ASSIGN:      printf("Assign(=)\n"); break;
        case NODE_BINOP:       printf("BinOp(%s)\n", op_name((unsigned char)root->attr.binop.op)); break;
        case NODE_FUNC_CALL:   printf("FuncCall\n"); break;
        case NODE_ARG_LIST:    printf("ArgList\n"); break;
        case NODE_ARRAY_INDEX: printf("ArrayIndex\n"); break;
        case NODE_IDENT:       printf("Ident(%s)\n", root->attr.ident.name); break;
        case NODE_INT_CONST:   printf("IntConst(%s)\n", root->attr.int_const.text); break;
        case NODE_LONG_CONST:  printf("LongConst(%s)\n", root->attr.long_const.text); break;
        case NODE_FLOAT_CONST: printf("FloatConst(%s)\n", root->attr.float_const.text); break;
        case NODE_CHAR_CONST:  printf("CharConst(%s)\n", root->attr.char_const.text); break;
        case NODE_TYPE:        printf("Type(%s)\n", root->attr.type.name); break;
        case NODE_PREPROCESS:  printf("Preprocess(%s)\n", root->attr.raw.text); break;
        case NODE_COMMENT:     printf("Comment(%s)\n", root->attr.raw.text); break;
        default:               printf("Node(%d)\n", root->tag); break;
    }

    print_tree(root->child1, depth + 1);   /* 孩子：缩进 +1 */
    print_tree(root->child2, depth);       /* 兄弟：缩进不变 */
}
