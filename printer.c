/*
 * printer.c — 格式化输出模块实现
 * 功能：对 AST 先根遍历，按缩进规范输出源程序
 * 输入：输出文件指针 out、AST 根结点指针 root
 * 输出：向 out 写入格式化代码
 *
 * 格式规范（Allman 风格）：
 *   - 缩进 4 空格，每层 +4
 *   - 函数/复合语句的花括号独占一行
 *   - 二元运算符两侧各一个空格
 *   - 顶层定义之间空一行
 */
#include <stdio.h>
#include "printer.h"
#include "token.h"

static int indent = 0;

static void emit_indent(FILE *out)
{
    for (int i = 0; i < indent; i++)
        fputs("    ", out);
}

/* 取节点的第 n 个孩子（0 起）。孩子链：child1=第一个孩子，其后用 child2 串联 */
static ASTNode *child_n(ASTNode *node, int n)
{
    ASTNode *c = node->child1;
    for (int i = 0; i < n && c != NULL; i++)
        c = c->child2;
    return c;
}

/* 二元运算符编码 → 字符串 */
static const char *op_str(int k)
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

static void emit_expr(FILE *out, ASTNode *e);
static void emit_stmt(FILE *out, ASTNode *s);

/* 输出控制语句的 body：复合语句不额外缩进，单语句 +1 层 */
static void emit_body(FILE *out, ASTNode *body)
{
    if (body != NULL && body->tag == NODE_COMPOUND) {
        emit_stmt(out, body);
    } else {
        indent++;
        emit_stmt(out, body);
        indent--;
    }
}

static void emit_expr(FILE *out, ASTNode *e)
{
    if (e == NULL) return;
    switch (e->tag) {
        case NODE_IDENT:
            fputs(e->attr.ident.name, out);
            break;
        case NODE_INT_CONST:
            fputs(e->attr.int_const.text, out);
            break;
        case NODE_LONG_CONST:
            fputs(e->attr.long_const.text, out);
            break;
        case NODE_FLOAT_CONST:
            fputs(e->attr.float_const.text, out);
            break;
        case NODE_CHAR_CONST:
            fputs(e->attr.char_const.text, out);
            break;
        case NODE_ASSIGN:
            emit_expr(out, child_n(e, 0));
            fputs(" = ", out);
            emit_expr(out, child_n(e, 1));
            break;
        case NODE_BINOP:
            emit_expr(out, child_n(e, 0));
            fprintf(out, " %s ", op_str((unsigned char)e->attr.binop.op));
            emit_expr(out, child_n(e, 1));
            break;
        case NODE_FUNC_CALL: {
            ASTNode *fn = child_n(e, 0);              /* 函数名 */
            fputs(fn->attr.ident.name, out);
            fputs("(", out);
            int first = 1;
            for (ASTNode *arg = fn->child2; arg; arg = arg->child2) {
                if (!first) fputs(", ", out);
                emit_expr(out, arg);
                first = 0;
            }
            fputs(")", out);
            break;
        }
        case NODE_ARRAY_INDEX:
            emit_expr(out, child_n(e, 0));            /* 名字 */
            fputs("[", out);
            emit_expr(out, child_n(e, 1));            /* 下标/大小 */
            fputs("]", out);
            break;
        default:
            break;
    }
}

/* 输出变量定义：类型 变量, 变量 ; */
static void emit_vardef(FILE *out, ASTNode *d)
{
    emit_indent(out);
    ASTNode *type = child_n(d, 0);
    fputs(type->attr.type.name, out);
    fputs(" ", out);
    int first = 1;
    for (ASTNode *v = type->child2; v; v = v->child2) {
        if (!first) fputs(", ", out);
        emit_expr(out, v);
        first = 0;
    }
    fputs(";\n", out);
}

/* 输出形式参数序列 */
static void emit_params(FILE *out, ASTNode *params)
{
    int first = 1;
    for (ASTNode *p = params->child1; p; p = p->child2) {
        if (!first) fputs(", ", out);
        fputs(child_n(p, 0)->attr.type.name, out);          /* 参数类型 */
        fputs(" ", out);
        fputs(child_n(p, 1)->attr.ident.name, out);         /* 参数名 */
        first = 0;
    }
}

/* 输出函数定义 / 声明 */
static void emit_funcdef(FILE *out, ASTNode *d)
{
    ASTNode *type   = child_n(d, 0);
    ASTNode *fname  = child_n(d, 1);
    ASTNode *params = child_n(d, 2);
    ASTNode *body   = child_n(d, 3);               /* 仅 FUNC_DEF 有 */

    emit_indent(out);
    fputs(type->attr.type.name, out);
    fputs(" ", out);
    fputs(fname->attr.ident.name, out);
    fputs("(", out);
    emit_params(out, params);
    fputs(")", out);

    if (body != NULL) {                             /* 函数定义 */
        fputs("\n", out);
        emit_stmt(out, body);
    } else {                                        /* 函数声明 */
        fputs(";\n", out);
    }
}

/* 输出语句 */
static void emit_stmt(FILE *out, ASTNode *s)
{
    if (s == NULL) return;
    switch (s->tag) {
        case NODE_EXPR_STMT:
            emit_indent(out);
            emit_expr(out, child_n(s, 0));
            fputs(";\n", out);
            break;

        case NODE_COMPOUND:
            emit_indent(out);
            fputs("{\n", out);
            indent++;
            for (ASTNode *c = s->child1; c; c = c->child2) {
                if (c->tag == NODE_EXT_VAR_DEF)
                    emit_vardef(out, c);
                else
                    emit_stmt(out, c);
            }
            indent--;
            emit_indent(out);
            fputs("}\n", out);
            break;

        case NODE_IF:
            emit_indent(out);
            fputs("if (", out);
            emit_expr(out, child_n(s, 0));
            fputs(")\n", out);
            emit_body(out, child_n(s, 1));
            break;

        case NODE_IF_ELSE:
            emit_indent(out);
            fputs("if (", out);
            emit_expr(out, child_n(s, 0));
            fputs(")\n", out);
            emit_body(out, child_n(s, 1));
            emit_indent(out);
            fputs("else\n", out);
            emit_body(out, child_n(s, 2));
            break;

        case NODE_WHILE:
            emit_indent(out);
            fputs("while (", out);
            emit_expr(out, child_n(s, 0));
            fputs(")\n", out);
            emit_body(out, child_n(s, 1));
            break;

        case NODE_FOR:
            emit_indent(out);
            fputs("for (", out);
            if (child_n(s, 0) && child_n(s, 0)->child1) emit_expr(out, child_n(s, 0)->child1);
            fputs("; ", out);
            if (child_n(s, 1) && child_n(s, 1)->child1) emit_expr(out, child_n(s, 1)->child1);
            fputs("; ", out);
            if (child_n(s, 2) && child_n(s, 2)->child1) emit_expr(out, child_n(s, 2)->child1);
            fputs(")\n", out);
            emit_body(out, child_n(s, 3));
            break;

        case NODE_RETURN:
            emit_indent(out);
            fputs("return", out);
            if (child_n(s, 0)) { fputs(" ", out); emit_expr(out, child_n(s, 0)); }
            fputs(";\n", out);
            break;

        case NODE_BREAK:
            emit_indent(out);
            fputs("break;\n", out);
            break;

        case NODE_CONTINUE:
            emit_indent(out);
            fputs("continue;\n", out);
            break;

        case NODE_PREPROCESS:            /* 预处理行整行原样写回 */
        case NODE_COMMENT:               /* 注释原文写回（块注释可能多行） */
            emit_indent(out);
            fputs(s->attr.raw.text, out);
            fputc('\n', out);
            break;

        default:
            break;
    }
}

/* 输出外部定义 */
static void emit_def(FILE *out, ASTNode *d)
{
    switch (d->tag) {
        case NODE_EXT_VAR_DEF:
            emit_vardef(out, d);
            break;
        case NODE_FUNC_DEF:
        case NODE_FUNC_DECL:
            emit_funcdef(out, d);
            break;
        case NODE_PREPROCESS:            /* 顶层的预处理/注释也写回 */
        case NODE_COMMENT:
            emit_indent(out);
            fputs(d->attr.raw.text, out);
            fputc('\n', out);
            break;
        default:
            break;
    }
}

void format_source(FILE *out, ASTNode *root)
{
    indent = 0;
    for (ASTNode *d = root->child1; d; d = d->child2) {
        emit_def(out, d);
        fputs("\n", out);       /* 顶层定义之间空一行 */
    }
}
