// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct
{
  struct spinlock lock; //锁
  int count[PHYSTOP / PGSIZE];
} p2v;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&p2v.lock, "p2v");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    p2v.count[PA2INDEX(p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&p2v.lock);
  if (--p2v.count[PA2INDEX(pa)] == 0)
  {
    release(&p2v.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else
  {
    release(&p2v.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    acquire(&p2v.lock);
    p2v.count[PA2INDEX(r)] = 1; // 将引用计数初始化为1
    release(&p2v.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


int
isCowpage(pagetable_t pagetable, uint64 va)
{
  if (va >= MAXVA)
    return -1;
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return -1;
  if ((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_F ? 0 : -1);
}


void*
cowalloc(pagetable_t pagetable, uint64 va)
{
  if (va % PGSIZE != 0)
    return 0;

  uint64 pa = walkaddr(pagetable, va); // 获取对应的物理地址
  if (pa == 0)
    return 0;

  pte_t *pte = walk(pagetable, va, 0); // 获取对应的PTE
  if (p2v.count[PA2INDEX(pa)] == 1)    // 判断是否只有一个进程引用该物理地址
  {
    *pte |= PTE_W;//设定成可写
    *pte &= ~PTE_F;//取反后进行并操作,设定成0,即不是cow页
    return (void *)pa;
  }
  else
  {
    // 多个进程对物理内存存在引用
    // 需要分配新的页面，并拷贝旧页面的内容
    char *mem = kalloc();
    if (mem == 0)
      return 0;
    
    memmove(mem, (char *)pa, PGSIZE); // 复制旧页面内容到新页
    *pte &= ~PTE_V; // 设定PTE_V为0，防止在mappagges时出错

    // 为新页面添加映射 后面这个PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0 可以判断新映射的页有写权限
    if (mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0)
    {
      kfree(mem);
      *pte |= PTE_V; //设定成有效页
      return 0;
    }
    kfree((char *)PGROUNDDOWN(pa)); // 将原来的物理内存引用计数减1
    return mem;
  }
}

int 
p2vCount(void *pa)
{
  return p2v.count[PA2INDEX(pa)];
}


int 
addp2vCount(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&p2v.lock);
  ++p2v.count[PA2INDEX(pa)];
  release(&p2v.lock);
  return 0;
}
