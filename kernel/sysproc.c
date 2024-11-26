#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  backtrace();
  
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


uint64
sys_sigalarm(void)
{
  struct proc* myProc = myproc();
  int period;
  uint64 handler;
  argint(0, &period);
  argaddr(1,&handler);
  myProc->periodTime = period;
  // 把获得的函数地址值转换成函数指针,存入proc
  myProc->handler = (void (*)())handler;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc* myProc = myproc();

  memmove(myProc->trapframe, myProc->alarmTrapframe, sizeof(struct trapframe));
  myProc->inAlarm = 0;
  return myProc->alarmTrapframe->a0;
  // return 0xac; 这个是test3手动修改的a0值,这个值被保存在用户态的trapframe里
}