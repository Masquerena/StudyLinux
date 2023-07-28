#include<stdio.h>
#include<stdlib.h>
int main()
{
    printf("我是一个程序\n");
    printf("HOME(%s)\n", getenv("HOME"));
    printf("PATH(%s)\n", getenv("PATH"));
    printf("PWD(%s)\n", getenv("PWD"));
    printf("MYENV(%s)\n",getenv("MYENV"));
    return 0;
}
