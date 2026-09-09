/* 04_punct.c — A3+A4+A5 运算符/定界符/双字符运算符/注释/预处理覆盖测试
 * 任务书 3.3.1：注释两种形式 + 预处理 #include #define
 * 验收期望：注释和预处理行被跳过（或识别为 PREPROCESS），不污染单词表 */
#include <stdio.h>
#define N 10

int main()
{
    int a;   /* 块注释：变量说明 */
    int b;   // 行注释：变量说明
    a = 1;
    b = 2;
    a = b + 1;
    a = b - 1;
    a = b * 2;
    a = b / 2;
    a = b % 2;
    if (a == 0) { a = 1; }   // ==
    if (a != 0) { a = 1; }   // !=
    if (a < 0)  { a = 0; }   // <
    if (a > 0)  { a = 0; }   // >
    if (a <= 0) { a = 1; }   // <=
    if (a >= 0) { a = 1; }   // >=
    if (a && b) { a = 0; }   // &&
    if (a || b) { a = 0; }   // ||
    return 0;
}
