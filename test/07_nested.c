int factorial(int n);

int fib(int n)
{
    if (n <= 1) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

int factorial(int n)
{
    int result;
    int i;
    result = 1;
    for (i = 1; i <= n; i = i + 1) {
        result = result * i;
    }
    return result;
}

int main()
{
    int a;
    int b;
    int i;
    int j;
    int sum;
    sum = 0;
    for (i = 0; i < 10; i = i + 1) {
        for (j = 0; j < 10; j = j + 1) {
            if (i == j) {
                sum = sum + 1;
            } else if (i < j) {
                sum = sum + 2;
            } else {
                sum = sum + 3;
            }
        }
    }
    a = fib(10);
    b = factorial(5);
    while (sum > 100) {
        sum = sum - 10;
        if (sum < 0) {
            break;
        }
    }
    if (a > 0) {
        if (b > 0) {
            sum = sum + 1;
        }
    }
    return sum + a + b;
}
