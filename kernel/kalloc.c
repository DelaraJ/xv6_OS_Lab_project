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

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP); //  از ته فایل لینکر kernel.ld تا 128مگ
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start); // آدرس شروع رند بشه به 4096 تایی
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) //4096 تا 4096 تا جلو میره وپوینترشو میفرسته به تابع
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa) // پیجی که بهش دادیم رو ریلیز میکنه
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE); // پیج رو با یک پر میکنه که داده اپ قبلی پاک بشه

  r = (struct run*)pa; // داره pa رو به یه پوینتر تبدیل میکنه که بتونه به لیست اضافه کنه

  acquire(&kmem.lock); // دونفر همزمان لیستو دست نزنن
  r->next = kmem.freelist; // به ابتدای لینک لیست اضافه میکنیم
  kmem.freelist = r; // فری لیست به یک پیج ای اشاره داره
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
  r = kmem.freelist; // اولین پیج توی لیست
  if(r) // اگر صفر باشه یعنی هیچ پیجی وجود نداره
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) // اگر پیج رو گرفته باشه مفدار داره
    memset((char*)r, 5, PGSIZE); // fill with junk // پاک کردن محتویات
  return (void*)r; // در نهایت آدرس پیج رو خروجی میدیم
}
