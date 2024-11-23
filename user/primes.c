#include "kernel/types.h"
#include "user/user.h"

void primes(int fd)
{
  int left = 0;   // 代表读取左边边管道的值
  int forked = 0; // 判断是否fork()过子进程
  int temp = 0;   // 读取的第一个数
  int p[2];
  // printf("im new\n");
  // printf("%d\n",getpid());
  // printf("%d\n",fd);

  while (1)
  {

    // 读取左边管道的值
    int readBytes = read(fd, &left, sizeof(int));

    // 管道关闭且管道为空,说明左边无值可读
    if (readBytes == 0)
    {
      close(fd);
      if (forked)
      {
        // 等待子进程结束
        close(p[1]);
        wait(0);
      }
      exit(0);
    }

    // 第一次读取数据
    if (temp == 0)
    {
      temp = left;
      printf("prime %d\n", temp);
    }

    if (left % temp != 0)
    {
      if (!forked)
      {
        pipe(p);
        forked = 1;
        // 如果该进程是子进程,进入新的函数调用,传入右边管道的读端
        if (fork() == 0)
        {
          close(p[1]);
          close(fd);
          primes(p[0]);
        }
        // 如果该进程是父进程,关闭右边管道的读端
        else
        {
          close(p[0]);
        }
      }
      // 把这个不能整除的数写进右边管道
      write(p[1], &left, sizeof(int));
    }
  }
}

int main(int argc, char *agrv[])
{
  int p[2];
  pipe(p);
  for (int i = 2; i <= 35; i++)
  {
    write(p[1], &i, sizeof(int));
  }
  close(p[1]);
  primes(p[0]);
  exit(0);
}