#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p1[2];      // 子进程->父进程使用的pipe
    int p2[2];      // 父进程->子进程使用的pipe
    char buf = 'b'; // 传输的字节
    pipe(p1);
    pipe(p2);

    int pid = fork();
    int exitStatus = 0;

    if (pid < 0) // 错误情况
    {
        fprintf(2, "fork() error!\n");
        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);
        exit(1);
    }
    else if (pid == 0) // 子进程
    {
        close(p1[0]);
        close(p2[1]);
        if (read(p2[0], &buf, sizeof(char)) != sizeof(char))
        {
            fprintf(2, "child read() error!\n");
            exitStatus = 1;
        }
        else
        {
            fprintf(1, "%d: received ping\n", getpid());
        }
        if (write(p1[1], &buf, sizeof(char) != sizeof(char)))
        {
            fprintf(2, "child write() error!\n");
            exitStatus = 1;
        }
        close(p1[1]);
        close(p2[0]);
        exit(exitStatus);
    }
    else // 父进程
    {
        close(p2[0]);
        close(p1[1]);
        if (write(p2[1], &buf, sizeof(char)) != sizeof(char))
        {
            fprintf(2, "parent write() error!\n");
            exitStatus = 1;
        }

        if (read(p1[0], &buf, sizeof(char) != sizeof(char)))
        {
            fprintf(2, "parent read() error!\n");
            exitStatus = 1;
        }
        else
        {
            fprintf(1, "%d: received pong\n", getpid());
        }
        close(p2[1]);
        close(p1[0]);
        exit(exitStatus);
    }
}