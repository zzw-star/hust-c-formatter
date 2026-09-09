int g_count;
int arr[10];
int add(int a, int b);
int sub(int a, int b);

int add(int a, int b)
{
    return a + b;
}

int main()
{
    int i;
    int sum;
    int x;
    int y;
    sum = 0;
    i = 0;
    while (i < 10) {
        sum = sum + arr[i];
        i = i + 1;
    }
    for (i = 0; i < 5; i = i + 1) {
        if (arr[i] > 100) {
            break;
        } else {
            continue;
        }
    }
    x = add(1, 2);
    y = sub(5, 3);
    g_count = sum + x * y;
    return 0;
}

int sub(int a ,int b)
{
    return a - b;
}
