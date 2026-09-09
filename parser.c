/*
 * parser.c — 语法分析模块实现
 * 功能：按 grammar.txt 中的 BNF 文法，递归下降分析并建 AST
 * 输入：源文件指针 fp
 * 输出：程序根结点指针（成功）/ NULL（失败）
 *
 * 设计：
 *   1. 递归下降子程序法：每个语法成分对应一个子程序（指导书要求）。
 *   2. 表达式用「运算符优先 + 操作数栈/运算符栈」双栈算法（指导书指定）。
 *   3. 括号 ( ) 、函数调用 f() 、数组下标 a[] 作为「因子」递归处理，
 *      不进入运算符栈——这比纯双栈处理括号更简单可靠，语义等价。
 *   4. AST 用孩子兄弟表示法：child1=第一孩子，child2=下一兄弟。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"
#include "error.h"
#include "ast.h"
#include "token.h"

/* 全局：当前 token（供所有子程序访问） */
static int  w_kind;
static char w_text[256];
static FILE *w_fp;
static int  w_line    = 1;   /* 当前 token 所在行号 */
static int  prev_line = 1;   /* 上一个 token 所在行号（"缺少分号"类错误要用它定位） */

/* -------------------- 注释/预处理结点的截获与挂载 --------------------
 * 任务书 3.3.3(5)：生成的 AST 要包含编译预处理和注释。
 * 做法：advance() 读到 COMMENT/PREPROCESS 时不当作语法成分，
 * 而是建结点存入待挂链（语法分析流程完全不受干扰，注释出现在
 * 表达式中间也不会报错）；program()/Compound() 循环里把待挂结点
 * 挂到当前层的孩子链上——AST 与格式化输出都能保留原文。
 */
static ASTNode *pp_head = NULL;   /* 待挂链首 */
static ASTNode *pp_tail = NULL;   /* 待挂链尾 */
static void add_child(ASTNode *parent, ASTNode *child);   /* 前置声明 */

/* 把当前 COMMENT/PREPROCESS token 建成结点，追加到待挂链 */
static void stash_pp(void)
{
    ASTNode *n = new_node(w_kind == PREPROCESS ? NODE_PREPROCESS : NODE_COMMENT);
    strncpy(n->attr.raw.text, w_text, sizeof(n->attr.raw.text) - 1);
    n->attr.raw.text[sizeof(n->attr.raw.text) - 1] = '\0';
    if (pp_tail != NULL) { pp_tail->child2 = n; pp_tail = n; }
    else                 { pp_head = pp_tail = n; }
}

/* 把待挂链上的结点逐个挂到 parent 的孩子链末尾 */
static void drain_pp(ASTNode *parent)
{
    while (pp_head != NULL) {
        ASTNode *n = pp_head;
        pp_head = n->child2;
        n->child2 = NULL;             /* 解开链上的临时链接 */
        if (pp_head == NULL) pp_tail = NULL;
        add_child(parent, n);
    }
}

/* 出错时丢弃待挂链（配合 free_tree 释放整棵树） */
static void discard_pp(void)
{
    while (pp_head != NULL) {
        ASTNode *n = pp_head;
        pp_head = n->child2;
        free_tree(n);
    }
    pp_tail = NULL;
}

/* 推进一个 token：读下一个到 w_kind / w_text
 * 注释/预处理不进入语法分析，被截获到待挂链后继续读下一个 */
static void advance(void)
{
    do {
        prev_line = w_line;              /* 先记住上一个 token 的行号 */
        w_kind = gettoken(w_fp);
        strncpy(w_text, token_text, sizeof(w_text) - 1);
        w_text[sizeof(w_text) - 1] = '\0';
        w_line = line_no;                /* gettoken 读完后 line_no 即当前 token 所在行 */
        if (w_kind == PREPROCESS || w_kind == COMMENT)
            stash_pp();
    } while (w_kind == PREPROCESS || w_kind == COMMENT);
}

/* 把 child 追加到 parent 的孩子链表末尾（孩子兄弟表示法） */
static void add_child(ASTNode *parent, ASTNode *child)
{
    if (parent == NULL || child == NULL) return;
    if (parent->child1 == NULL) {
        parent->child1 = child;
    } else {
        ASTNode *s = parent->child1;
        while (s->child2 != NULL) s = s->child2;
        s->child2 = child;
    }
}

/* 前置声明（互递归调用；parse_expr 已在 parser.h 声明） */
static ASTNode *Statement(void);
static ASTNode *Compound(void);

/* 当前 token 是不是类型说明符 int/float/char */
static int is_type_spec(void)
{
    return w_kind == INT || w_kind == FLOAT || w_kind == CHAR;
}

/* -------------------- 表达式：双栈算法 -------------------- */

/* 二元运算符优先级（数值越大优先级越高） */
static int prec_level(int k)
{
    switch (k) {
        case ASSIGN:                              return 1;
        case OR:                                  return 2;
        case AND:                                 return 3;
        case EQ: case NE:                         return 4;
        case LT: case GT: case LE: case GE:       return 5;
        case PLUS: case MINUS:                    return 6;
        case STAR: case SLASH: case PERCENT:      return 7;
        default:                                  return 0;
    }
}

/* 右结合运算符（只有赋值 =） */
static int is_right_assoc(int k) { return k == ASSIGN; }

/* 是不是二元运算符 */
static int is_binop(int k)
{
    switch (k) {
        case PLUS: case MINUS: case STAR: case SLASH: case PERCENT:
        case EQ: case NE: case LT: case GT: case LE: case GE:
        case AND: case OR: case ASSIGN:
            return 1;
        default:
            return 0;
    }
}

/* 归约：op 栈顶算符出栈，opd 栈顶两个操作数出栈建树压回。
 * 赋值建 NODE_ASSIGN，其它二元算符建 NODE_BINOP。返回 1 成功 / 0 失败 */
static int reduce(int *op, int *op_top, ASTNode **opd, int *opd_top)
{
    if (*op_top < 0)             { syntax_error(prev_line, "运算符栈空"); return 0; }
    int opk = op[*op_top];
    if (opk == EOF_TOKEN)        { syntax_error(prev_line, "栈底无可归约算符"); return 0; }
    if (*opd_top < 1)            { syntax_error(prev_line, "表达式缺少操作数"); return 0; }

    (*op_top)--;
    ASTNode *right = opd[*opd_top]; (*opd_top)--;
    ASTNode *left  = opd[*opd_top]; (*opd_top)--;

    ASTNode *node = new_node(opk == ASSIGN ? NODE_ASSIGN : NODE_BINOP);
    if (opk != ASSIGN) node->attr.binop.op = (char)opk;
    add_child(node, left);      /* 统一用孩子兄弟链（child1+child2 兄弟） */
    add_child(node, right);

    (*opd_top)++;
    opd[*opd_top] = node;
    return 1;
}

/* 读一个因子（操作数）：IDENT / 常量 / ( 表达式 ) / 函数调用 / 数组下标 */
static ASTNode *read_factor(void)
{
    /* 标识符：可能是普通变量、函数调用 f(...)、数组下标 a[...] */
    if (w_kind == IDENT) {
        char name[64];
        strncpy(name, w_text, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        advance();

        if (w_kind == LP) {                 /* 函数调用 */
            advance();                       /* 吃掉 ( */
            ASTNode *call = new_node(NODE_FUNC_CALL);
            ASTNode *fn = new_node(NODE_IDENT);
            strncpy(fn->attr.ident.name, name, sizeof(fn->attr.ident.name) - 1);
            add_child(call, fn);

            if (w_kind != RP) {             /* 有实参 */
                for (;;) {
                    ASTNode *arg = parse_expr(COMMA);
                    if (arg == NULL) return NULL;
                    add_child(call, arg);
                    if (w_kind == COMMA) { advance(); continue; }
                    break;
                }
            }
            if (w_kind != RP) { syntax_error(prev_line, "函数调用缺少右括号"); return NULL; }
            advance();                       /* 吃掉 ) */
            return call;
        }

        if (w_kind == LB) {                 /* 数组下标 a[...] */
            advance();                       /* 吃掉 [ */
            ASTNode *arr = new_node(NODE_ARRAY_INDEX);
            ASTNode *an = new_node(NODE_IDENT);
            strncpy(an->attr.ident.name, name, sizeof(an->attr.ident.name) - 1);
            add_child(arr, an);
            ASTNode *idx = parse_expr(RB);
            if (idx == NULL) return NULL;
            add_child(arr, idx);
            if (w_kind != RB) { syntax_error(prev_line, "数组下标缺少右方括号"); return NULL; }
            advance();                       /* 吃掉 ] */
            return arr;
        }

        /* 普通标识符 */
        ASTNode *id = new_node(NODE_IDENT);
        strncpy(id->attr.ident.name, name, sizeof(id->attr.ident.name) - 1);
        return id;
    }

    /* 常量 */
    if (w_kind == INT_CONST || w_kind == LONG_CONST
        || w_kind == FLOAT_CONST || w_kind == CHAR_CONST) {
        ASTNode *leaf = new_node(
            w_kind == INT_CONST   ? NODE_INT_CONST :
            w_kind == LONG_CONST  ? NODE_LONG_CONST :
            w_kind == FLOAT_CONST ? NODE_FLOAT_CONST : NODE_CHAR_CONST);
        if (w_kind == INT_CONST) {
            leaf->attr.int_const.value = (int)strtol(w_text, NULL, 0);  /* 0 自动识别 0x/0 八进制 */
            strncpy(leaf->attr.int_const.text, w_text, sizeof(leaf->attr.int_const.text) - 1);
        } else if (w_kind == LONG_CONST) {
            leaf->attr.long_const.value = strtol(w_text, NULL, 0);
            strncpy(leaf->attr.long_const.text, w_text, sizeof(leaf->attr.long_const.text) - 1);
        } else if (w_kind == FLOAT_CONST) {
            leaf->attr.float_const.value = (float)atof(w_text);
            strncpy(leaf->attr.float_const.text, w_text, sizeof(leaf->attr.float_const.text) - 1);
        } else {
            leaf->attr.char_const.value = w_text[1];
            strncpy(leaf->attr.char_const.text, w_text, sizeof(leaf->attr.char_const.text) - 1);
        }
        advance();
        return leaf;
    }

    /* 括号表达式 ( ... ) */
    if (w_kind == LP) {
        advance();                          /* 吃掉 ( */
        ASTNode *e = parse_expr(RP);
        if (e == NULL) return NULL;
        if (w_kind != RP) { syntax_error(prev_line, "缺少右括号"); return NULL; }
        advance();                          /* 吃掉 ) */
        return e;
    }

    syntax_error(w_line, "期望因子（标识符/常量/括号）");
    return NULL;
}

/* parse_expr(endsym)：分析表达式，到 endsym 为止。
 * 双栈算法：opd 操作数栈 + op 运算符栈。 */
ASTNode *parse_expr(int endsym)
{
    if (w_kind == endsym) return NULL;       /* 空表达式（如 for(;;) 的空段） */

    ASTNode *opd[256];  int opd_top = -1;
    int      op[256];   int op_top  = 0;
    op[0] = EOF_TOKEN;                       /* 栈底标记 */

    for (;;) {
        if (w_kind == EOF_TOKEN) { syntax_error(prev_line, "表达式未结束"); return NULL; }

        /* ① 因子（操作数） */
        if (w_kind == IDENT || w_kind == INT_CONST || w_kind == LONG_CONST
            || w_kind == FLOAT_CONST || w_kind == CHAR_CONST || w_kind == LP) {
            ASTNode *f = read_factor();
            if (f == NULL) return NULL;
            opd[++opd_top] = f;
            continue;
        }

        /* ② 结束符：endsym 或嵌套结束符 ) ] —— 把栈里算符全部归约，结束 */
        if (w_kind == endsym || w_kind == RP || w_kind == RB) {
            while (op_top > 0) {
                if (!reduce(op, &op_top, opd, &opd_top)) return NULL;
            }
            break;
        }

        /* ③ 二元算符：按优先级决定归约或压栈 */
        if (is_binop(w_kind)) {
            int cur = w_kind;
            while (op_top > 0) {
                int top = op[op_top];
                int should_reduce = is_right_assoc(top)
                    ? (prec_level(top) >  prec_level(cur))
                    : (prec_level(top) >= prec_level(cur));
                if (!should_reduce) break;
                if (!reduce(op, &op_top, opd, &opd_top)) return NULL;
            }
            op[++op_top] = cur;
            advance();
            continue;
        }

        /* ④ 表达式在此处提前结束，但当前 token 不是预期的结束符。
         * 按 endsym 判断到底缺什么，给出准确提示：
         *   表达式语句 / return 以分号结束 → 缺分号
         *   括号内表达式以 ) 结束          → 缺右括号
         *   数组下标以 ] 结束              → 缺右方括号 */
        if (endsym == SEMI) {
            syntax_error(prev_line, "表达式缺少分号");
        } else if (endsym == RP) {
            syntax_error(prev_line, "表达式缺少右括号");
        } else if (endsym == RB) {
            syntax_error(prev_line, "表达式缺少右方括号");
        } else if (endsym == COMMA) {
            syntax_error(prev_line, "实参语法错误（缺少逗号或右括号）");
        } else {
            syntax_error(prev_line, "表达式语法错误");
        }
        return NULL;
    }

    if (opd_top != 0) { syntax_error(prev_line, "表达式语法错误（操作数与运算符不匹配）"); return NULL; }
    return opd[0];
}

/* -------------------- 类型 / 变量定义 -------------------- */

/* 类型说明符 int/float/char → NODE_TYPE */
static ASTNode *TypeSpec(void)
{
    ASTNode *t = new_node(NODE_TYPE);
    strncpy(t->attr.type.name, w_text, sizeof(t->attr.type.name) - 1);
    advance();
    return t;
}

/* 读单个变量（名字已读，w_kind 是下一个 token）：
 * IDENT 或 IDENT[INT_CONST]（数组），返回变量节点 */
static ASTNode *read_one_var(const char *name)
{
    if (w_kind == LB) {                     /* 数组 int a[10] */
        advance();                          /* 吃掉 [ */
        if (w_kind != INT_CONST) { syntax_error(w_line, "数组大小须为整型常量"); return NULL; }
        ASTNode *arr = new_node(NODE_ARRAY_INDEX);
        ASTNode *id = new_node(NODE_IDENT);
        strncpy(id->attr.ident.name, name, sizeof(id->attr.ident.name) - 1);
        ASTNode *sz = new_node(NODE_INT_CONST);
        sz->attr.int_const.value = (int)strtol(w_text, NULL, 0);
        strncpy(sz->attr.int_const.text, w_text, sizeof(sz->attr.int_const.text) - 1);
        add_child(arr, id);
        add_child(arr, sz);
        advance();                          /* 吃掉常量 */
        if (w_kind != RB) { syntax_error(prev_line, "数组声明缺少右方括号"); return NULL; }
        advance();                          /* 吃掉 ] */
        return arr;
    }
    ASTNode *id = new_node(NODE_IDENT);
    strncpy(id->attr.ident.name, name, sizeof(id->attr.ident.name) - 1);
    return id;
}

/* 变量定义：类型 变量序列 ;（第一个名字已读，w_kind 是下一个） */
static ASTNode *finish_var_def(ASTNode *type, const char *first_name)
{
    ASTNode *node = new_node(NODE_EXT_VAR_DEF);
    add_child(node, type);

    ASTNode *var = read_one_var(first_name);
    if (var == NULL) return NULL;
    add_child(node, var);

    while (w_kind == COMMA) {
        advance();
        if (w_kind != IDENT) { syntax_error(w_line, "期望变量名"); return NULL; }
        char name[64];
        strncpy(name, w_text, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        advance();
        var = read_one_var(name);
        if (var == NULL) return NULL;
        add_child(node, var);
    }
    if (w_kind != SEMI) { syntax_error(prev_line, "变量定义缺少分号"); return NULL; }
    advance();                              /* 吃掉 ; */
    return node;
}

/* 局部/外部变量定义入口：类型 变量序列 ; */
static ASTNode *VarDef(void)
{
    ASTNode *type = TypeSpec();
    if (w_kind != IDENT) { syntax_error(w_line, "期望变量名"); return NULL; }
    char name[64];
    strncpy(name, w_text, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    advance();
    return finish_var_def(type, name);
}

/* -------------------- 函数 -------------------- */

/* 函数定义/声明：类型 名字 ( 参数 ) { 体 } 或 ... ; 
 * 调用前：type 已读，name 已读，w_kind == LP */
static ASTNode *FuncDef(ASTNode *type, const char *name)
{
    advance();                              /* 吃掉 ( */

    ASTNode *params = new_node(NODE_PARAM_LIST);
    while (is_type_spec()) {                /* 形式参数：类型 名字 */
        ASTNode *ptype = TypeSpec();
        if (w_kind != IDENT) { syntax_error(w_line, "参数缺少名字"); return NULL; }
        ASTNode *param = new_node(NODE_EXT_VAR_DEF);
        add_child(param, ptype);
        ASTNode *pid = new_node(NODE_IDENT);
        strncpy(pid->attr.ident.name, w_text, sizeof(pid->attr.ident.name) - 1);
        add_child(param, pid);
        advance();
        add_child(params, param);
        if (w_kind == COMMA) { advance(); continue; }
        break;
    }
    if (w_kind != RP) { syntax_error(prev_line, "函数参数缺少右括号"); return NULL; }
    advance();                              /* 吃掉 ) */

    ASTNode *fname = new_node(NODE_IDENT);
    strncpy(fname->attr.ident.name, name, sizeof(fname->attr.ident.name) - 1);

    if (w_kind == SEMI) {                   /* 函数声明（原型） */
        ASTNode *decl = new_node(NODE_FUNC_DECL);
        add_child(decl, type);
        add_child(decl, fname);
        add_child(decl, params);
        advance();                          /* 吃掉 ; */
        return decl;
    }

    if (w_kind == LBRA) {                   /* 函数定义 */
        ASTNode *body = Compound();
        if (body == NULL) return NULL;
        ASTNode *def = new_node(NODE_FUNC_DEF);
        add_child(def, type);
        add_child(def, fname);
        add_child(def, params);
        add_child(def, body);
        return def;
    }

    syntax_error(prev_line, "函数声明后期望 ; 或 {");
    return NULL;
}

/* -------------------- 语句 -------------------- */

static ASTNode *IfStmt(void)
{
    advance();                              /* 吃掉 if */
    if (w_kind != LP) { syntax_error(prev_line, "if 后缺左括号"); return NULL; }
    advance();
    ASTNode *cond = parse_expr(RP);
    if (cond == NULL) return NULL;
    if (w_kind != RP) { syntax_error(prev_line, "if 条件缺右括号"); return NULL; }
    advance();
    ASTNode *then = Statement();
    if (then == NULL) return NULL;

    if (w_kind == ELSE) {
        advance();
        ASTNode *els = Statement();
        if (els == NULL) return NULL;
        ASTNode *node = new_node(NODE_IF_ELSE);
        add_child(node, cond);
        add_child(node, then);
        add_child(node, els);
        return node;
    }

    ASTNode *node = new_node(NODE_IF);
    add_child(node, cond);
    add_child(node, then);
    return node;
}

static ASTNode *WhileStmt(void)
{
    advance();                              /* 吃掉 while */
    if (w_kind != LP) { syntax_error(prev_line, "while 后缺左括号"); return NULL; }
    advance();
    ASTNode *cond = parse_expr(RP);
    if (cond == NULL) return NULL;
    if (w_kind != RP) { syntax_error(prev_line, "while 条件缺右括号"); return NULL; }
    advance();
    ASTNode *body = Statement();
    if (body == NULL) return NULL;

    ASTNode *node = new_node(NODE_WHILE);
    add_child(node, cond);
    add_child(node, body);
    return node;
}

static ASTNode *ForStmt(void)
{
    advance();                              /* 吃掉 for */
    if (w_kind != LP) { syntax_error(prev_line, "for 后缺左括号"); return NULL; }
    advance();

    ASTNode *init = (w_kind == SEMI) ? NULL : parse_expr(SEMI);
    if (w_kind != SEMI) { syntax_error(prev_line, "for 初值缺分号"); return NULL; }
    advance();                              /* 吃掉第一个 ; */

    ASTNode *cond = (w_kind == SEMI) ? NULL : parse_expr(SEMI);
    if (w_kind != SEMI) { syntax_error(prev_line, "for 条件缺分号"); return NULL; }
    advance();                              /* 吃掉第二个 ; */

    ASTNode *upd = (w_kind == RP) ? NULL : parse_expr(RP);
    if (w_kind != RP) { syntax_error(prev_line, "for 缺右括号"); return NULL; }
    advance();                              /* 吃掉 ) */

    ASTNode *body = Statement();
    if (body == NULL) return NULL;

    ASTNode *node = new_node(NODE_FOR);
    /* 三段表达式（含空段）都包成 NODE_EXPR_STMT，保证孩子链固定 4 个：
       [init段, cond段, upd段, body]，方便格式化输出按位置取 */
    ASTNode *seg_init = new_node(NODE_EXPR_STMT); seg_init->child1 = init;
    ASTNode *seg_cond = new_node(NODE_EXPR_STMT); seg_cond->child1 = cond;
    ASTNode *seg_upd  = new_node(NODE_EXPR_STMT); seg_upd->child1  = upd;
    add_child(node, seg_init);
    add_child(node, seg_cond);
    add_child(node, seg_upd);
    add_child(node, body);
    return node;
}

static ASTNode *ReturnStmt(void)
{
    advance();                              /* 吃掉 return */
    ASTNode *node = new_node(NODE_RETURN);
    if (w_kind != SEMI) {                   /* return 表达式 ; */
        ASTNode *e = parse_expr(SEMI);
        if (e == NULL) return NULL;
        add_child(node, e);
    }
    if (w_kind != SEMI) { syntax_error(prev_line, "return 缺分号"); return NULL; }
    advance();
    return node;
}

static ASTNode *ExprStmt(void)
{
    ASTNode *e = parse_expr(SEMI);
    if (e == NULL) return NULL;
    if (w_kind != SEMI) { syntax_error(prev_line, "表达式语句缺分号"); return NULL; }
    advance();
    ASTNode *node = new_node(NODE_EXPR_STMT);
    add_child(node, e);
    return node;
}

/* 语句分派：覆盖任务书全部语句 */
static ASTNode *Statement(void)
{
    switch (w_kind) {
        case LBRA:     return Compound();
        case IF:       return IfStmt();
        case WHILE:    return WhileStmt();
        case FOR:      return ForStmt();
        case RETURN:   return ReturnStmt();
        case BREAK:
            advance();
            if (w_kind != SEMI) { syntax_error(prev_line, "break 缺分号"); return NULL; }
            advance();
            return new_node(NODE_BREAK);
        case CONTINUE:
            advance();
            if (w_kind != SEMI) { syntax_error(prev_line, "continue 缺分号"); return NULL; }
            advance();
            return new_node(NODE_CONTINUE);
        default:       return ExprStmt();
    }
}

/* 复合语句 { 局部变量定义序列 语句序列 } */
static ASTNode *Compound(void)
{
    if (w_kind != LBRA) { syntax_error(prev_line, "期望 {"); return NULL; }
    advance();                              /* 吃掉 { */

    ASTNode *node = new_node(NODE_COMPOUND);
    while (w_kind != RBRA && w_kind != EOF_TOKEN) {
        drain_pp(node);                      /* 注释/预处理也挂进 AST */
        ASTNode *child;
        if (is_type_spec()) {
            child = VarDef();               /* 局部变量定义 */
        } else {
            child = Statement();            /* 语句 */
        }
        if (child == NULL) return NULL;
        add_child(node, child);
    }
    drain_pp(node);                          /* } 之前的注释也不丢 */
    if (w_kind != RBRA) { syntax_error(prev_line, "复合语句缺 }"); return NULL; }
    advance();                              /* 吃掉 } */
    return node;
}

/* -------------------- 外部定义 -------------------- */

/* 外部定义：变量定义 | 函数定义 | 函数声明 */
static ASTNode *ExtDef(void)
{
    /* 外部定义必须以类型说明符开头，否则报准确错误（而不是笼统的"期望标识符"） */
    if (!is_type_spec()) {
        syntax_error(w_line, "期望类型说明符（int/float/char）");
        return NULL;
    }
    ASTNode *type = TypeSpec();
    if (w_kind != IDENT) { syntax_error(w_line, "期望标识符"); return NULL; }
    char name[64];
    strncpy(name, w_text, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    advance();

    if (w_kind == LP) return FuncDef(type, name);   /* 函数 */
    return finish_var_def(type, name);              /* 外部变量 */
}

/* 表达式子程序初始化（供 main 的 -p 模式调用） */
void parser_init(FILE *fp)
{
    w_fp = fp;
    advance();
}

/* program：语法分析入口 */
ASTNode *program(FILE *fp)
{
    w_fp = fp;
    advance();                              /* 读第一个 token */

    ASTNode *root = new_node(NODE_PROGRAM);
    while (w_kind != EOF_TOKEN) {
        drain_pp(root);                      /* 注释/预处理也挂进 AST */
        ASTNode *def = ExtDef();
        if (def == NULL) { discard_pp(); free_tree(root); return NULL; }
        add_child(root, def);
    }
    drain_pp(root);                          /* 文件末尾的注释也不丢 */
    return root;
}
