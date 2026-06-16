#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    // از ته فایل لینکر kernel.ld تا 128مگ
    // رو پیج پیج جدا میکنه و لینک لیست درست میکنه از آدرساشون
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    // با نوشتن آدرس کرنل پیج تیبل(آدرس پیج لول 2) توی satp واحد سخت افزاری ویرچوال مموری فعال میشه 

    // چون مموری بین همه کور ها مشترکه
    // با یه آدرس مشترک این کار برای همه کور ها انجام میشه
    procinit();      // process table

    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk

    // اولین پراسس ساخته میشه
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  // یه حلقه بینهایت که
  // همه پراسس هارو بررسی میکنه
  // اولین پراسسی که رانیبل باشه رو ا نتخاب میکنه
  // به آن سوییچ میکنه
  // وقتی هیچ فرایندی قابل ران نیست همون حلقه بینهایت اجرا میشه که pid=0
  // اولین پراسس واقعی که اجرا میشه pid=1 که توسط userinit()
  // اولین پراسسی که اجرا میشه initproc
  // بقیشون عددای بیشتر

  scheduler();    // What proc is it going to call? Init with PID=0
}
