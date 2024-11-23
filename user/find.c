#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char *dir, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(dir, O_RDONLY)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", dir);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", dir);
        close(fd);
        return;
    }

    if (st.type != T_DIR)
    {
        fprintf(2, "usage: find [dir][filename]\n");
        return;
    }

    if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        printf("find: path too long\n");
        return;
    }

    strcpy(buf, dir);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if (strcmp(p, filename) == 0)
        {
            printf("%s\n", buf);
        }
        else if (st.type == T_DIR && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0)
        {
            find(buf, filename);
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(2, "usage: find [dir][filename]\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}