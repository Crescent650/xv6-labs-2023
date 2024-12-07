// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(id) (id % NBUCKET)

struct hashbuf
{
  struct buf head;      // 头节点
  struct spinlock lock; // 锁
};

struct
{
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];
} bcache;

void binit(void)
{
  struct buf *b;
  char name[16];

  for (int i = 0; i < NBUCKET; ++i)
  {
    // 初始化散列桶的自旋锁
    snprintf(name, sizeof(name), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, name);

    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }
  // 建立双链表到桶0的头节点上
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id = HASH(blockno);            // 使用blockno获取相应id
  acquire(&bcache.buckets[id].lock); // 获取当前桶的锁

  // 遍历当前桶,如果找到就记录并返回指定buf块
  for (b = bcache.buckets[id].head.next; b != &bcache.buckets[id].head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;

      // 记录使用时间戳
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 如果没找到,把buf块清零;设置临时buf块
  b = 0;
  struct buf *tmp;

  // 从当前桶开始查找
  for (int i = id, count = 0; count != NBUCKET; i = (i + 1) % NBUCKET)
  {
    count++; // 记录是否全找过了
    // 如果遍历到当前散列桶，则不重新获取锁
    if (i != id)
    {
      if (!holding(&bcache.buckets[i].lock))
        acquire(&bcache.buckets[i].lock);
      else
        continue;
    }

    for (tmp = bcache.buckets[i].head.next; tmp != &bcache.buckets[i].head; tmp = tmp->next)
      // 使用时间戳进行LRU算法，而不是根据结点在链表中的位置
      if (tmp->refcnt == 0 && (b == 0 || tmp->timestamp < b->timestamp))
        b = tmp;

    if (b) // 如果b不为0,即在其他桶里找到了空的buf块
    {
      // 如果是从其他桶窃取的，则将其以头插法插入到当前桶
      if (i != id)
      {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[i].lock);

        b->next = bcache.buckets[id].head.next;
        b->prev = &bcache.buckets[id].head;
        bcache.buckets[id].head.next->prev = b;
        bcache.buckets[id].head.next = b;
      }

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
    else
    {
      // 在当前散列桶中未找到，则直接释放锁
      if (i != id)
        release(&bcache.buckets[i].lock);
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  int id = HASH(b->blockno);

  releasesleep(&b->lock);

  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  release(&bcache.buckets[id].lock);
}

void bpin(struct buf *b)
{
  int id = HASH(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt++;
  release(&bcache.buckets[id].lock);
}

void bunpin(struct buf *b)
{
  int id = HASH(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  release(&bcache.buckets[id].lock);
}
