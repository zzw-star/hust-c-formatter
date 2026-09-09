/* 01_variables.c — 阶段0/1 测试样例：外部变量、数组、函数、局部变量
 * 注：本语言子集不含变量初始化（文法 <变量> 只有声明形式），
 *     因此样例中不写 int x = 1.5; 这类初始化。 */
int i,j;
float x,y;
char ch;
int a[10];

int fun(int p, float q)
{
    int m;
    if (p>q) m=p;
    else m=q;
    return m;
}

int main()
{
    int s;
    s = fun(1, 2.5);
    return s;
}
