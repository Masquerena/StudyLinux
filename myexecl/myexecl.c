#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/types.h>
int main()
{
    printf("process running...\n");
    pid_t id = fork();
    if(id == 0)
    {
        //子进程
        sleep(1);
        //execlp("ls", "ls", "-a", "-l", "--color=auto", NULL);
        //execl("./mybin", "mybin", NULL);
        //char* const argv[] = {
            //"ls",
            //"-a",
            //"-l",
           //"--color=auto",
           // NULL
      //};
        //execv("/usr/bin/ls", argv);
        //execv("/usr/bin/ls", argv);
        
        //char* const envp_[] = {
            //(char*)"MYENV=11223344",
           // NULL
        //};
        //execle("./mybin","mybin", NULL, envp_);
        extern char** environ;
        putenv((char*)"MYENV=44332211");
        execle("./mybin","mybin", NULL, environ);
        exit(1);
    }

    //父进程
    int status = 0;
    pid_t ret = waitpid(id, &status, 0);
    if(ret > 0)
    {
        printf("wait success! exit code:%d, sig:%d\n", (status>>8) & 0xFF, (status & 0x7F));
    }
    return 0;
}
