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

// --- Phase 1: reference counting infrastructure for COW fork ---
//
// One entry per physical page in [KERNBASE, PHYSTOP). Protected by
// its own lock (reflock), separate from kmem.lock.
struct spinlock reflock;
int page_ref[(PHYSTOP - KERNBASE) / PGSIZE];

// Convert a physical address into an index into page_ref[].
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

// Increment the reference count of the physical page at pa.
void
incref(uint64 pa)
{
  if(pa < KERNBASE || pa >= PHYSTOP)
    panic("incref: bad pa");

  acquire(&reflock);
  page_ref[PA2IDX(pa)]++;
  release(&reflock);
}

// Decrement the reference count of the physical page at pa.
// If it drops to zero, actually free the page via kfree().
void
decref(uint64 pa)
{
  int c;

  if(pa < KERNBASE || pa >= PHYSTOP)
    panic("decref: bad pa");

  acquire(&reflock);
  c = --page_ref[PA2IDX(pa)];
  release(&reflock);

  if(c < 0)
    panic("decref: negative refcount");
  if(c == 0)
    kfree((void*)pa);
}

// Read the current reference count of the physical page at pa.
int
getref(uint64 pa)
{
  int c;

  if(pa < KERNBASE || pa >= PHYSTOP)
    return 0;

  acquire(&reflock);
  c = page_ref[PA2IDX(pa)];
  release(&reflock);
  return c;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&reflock, "reflock");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    page_ref[PA2IDX((uint64)r)] = 1;
  }
  return (void*)r;
}

// --- Phase 1 test helper -----------------------------------------
// Temporary boot-time sanity check for the refcounting logic, called
// once from main.c. Allocates a few pages, bumps/drops their
// refcounts by hand (as fork/exit will do later), and prints the
// results so we can eyeball that incref/decref/getref behave.
// Safe to leave in the tree; it does not run unless kreftest() is
// called explicitly.
void
kreftest(void)
{
  void *a = kalloc();
  void *b = kalloc();

  printf("kreftest: a=%p ref=%d (expect 1)\n", a, getref((uint64)a));
  printf("kreftest: b=%p ref=%d (expect 1)\n", b, getref((uint64)b));

  incref((uint64)a);
  incref((uint64)a);
  printf("kreftest: after 2x incref(a) ref=%d (expect 3)\n", getref((uint64)a));

  decref((uint64)a);
  printf("kreftest: after 1x decref(a) ref=%d (expect 2)\n", getref((uint64)a));

  decref((uint64)a);
  decref((uint64)a); // this last one should actually free the page
  printf("kreftest: after dropping to 0, ref=%d (expect 0)\n", getref((uint64)a));

  decref((uint64)b); // b had ref=1, this frees it too
  printf("kreftest: b freed, ref=%d (expect 0)\n", getref((uint64)b));

  printf("kreftest: done\n");
}
