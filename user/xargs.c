#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXSIZE 512

int main(int argc, char *argv[])
{

    char *xargv[MAXARG] = {0};   // 储存参数,全部初始化为空指针
    char buf[MAXSIZE + 1] = {0}; // 缓冲区,全部初始化为空指针
    int used = 0;                // 缓冲区已使用的字节数
    int end = 0;                 // 是否读取全部输入内容

    // 传入xargs将要执行的程序和参数
    for (int i = 1; i < argc; i++)
    {
        xargv[i - 1] = argv[i];
        // printf("%s\n",xargv[i-1]);
    }

    // 读取标准输入作为参数,结束条件是标准输入读取完毕或
    while (!(end && used == 0))
    {
        // 读取标准输入内容至缓冲区
        if (!end)
        {
            int remain = MAXSIZE - used; // 缓冲区剩余大小
            int readRet = read(0, buf + used, remain);
            if (readRet < 0)
            {
                fprintf(2, "xargs: read() error\n");
            }
            else if (readRet == 0)
            {
                close(0);
                end = 1;
            }
            used += readRet;
        }

        char *lineEnd = strchr(buf, '\n');

        while (lineEnd)
        {
            char tempBuf[MAXSIZE + 1] = {0};
            memcpy(tempBuf, buf, lineEnd - buf);
            xargv[argc - 1] = tempBuf;

            if (fork() == 0)
            {
                if (!end)
                {
                    close(0);
                }
                if (exec(argv[1], xargv) < 0)
                {
                    fprintf(2, "xargs: exec() error\n");
                    exit(1);
                }
            }
            else
            {
                memmove(buf, lineEnd + 1, used - (lineEnd - buf) - 1);
                used -= lineEnd - buf + 1;
                memset(buf + used, 0, MAXSIZE - used);
                wait(0);

                lineEnd = strchr(buf, '\n');
            }
        }
    }
    exit(0);
}