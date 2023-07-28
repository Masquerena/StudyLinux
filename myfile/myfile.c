
//#define FILE_NAME "file.txt"

//int main()
//{
//   FILE* fp = fopen(FILE_NAME,"a");
//    if(fp == NULL)
//    {
//        perror("fopen");
//        return 1;
//    }
//
//
//   int cnt = 5;
//   while(cnt)
//    {
//        fprintf(fp, "%s:%d\n", "hello file", cnt--);
//    }
//
//   fclose(fp);
//    return 0;
//}

#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

//int main()
//{
//    //close(0);
//    //close(2);
//    //close(1);
//
//    //int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
//    int fd = open("test.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
//    if(fd < 0)
//    {
//        perror("open");
//        return 1;
//    }
//
//    dup2(fd, 1);
//
//    printf("open fd : %d\n", fd);
//    
//    const char *msg = "hello\n";
//    write(1, msg, strlen(msg));
//
//    fflush(stdout);
//
//    close(fd);
//    return 0;
//}

int main()
{
    int fd = open("test.txt", O_RDONLY);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    dup2(fd, 0);

    char line[64];
    while(1)
    {
        printf(">");
        if(fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }
        printf("%s", line);
    }
    return 0;
}
