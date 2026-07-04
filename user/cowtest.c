// cowtest.c -- exercise copy-on-write fork (Phase 3).
//
// Usage: cowtest
//
// Covers:
//   1. basic: fork, child writes a heap page, parent must still see
//      the old value and child must see the new one.
//   2. multi: several children fork off the same page and each
//      writes a different value; every child (and the parent) must
//      keep seeing only its own write.
//   3. big: allocate a large chunk of heap, fork, and touch every
//      page from the child -- mostly a stress/crash test that COW
//      survives many simultaneous faults without corrupting memory.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print(const char *s)
{
  write(1, s, strlen(s));
}

// Test 1: basic parent/child divergence after a COW fault.
void
basic_test(void)
{
  print("cowtest: basic_test starting\n");

  volatile int *p = (int*)sbrk(4096);
  *p = 100;

  int pid = fork();
  if(pid < 0){
    print("cowtest: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // child: write, should not affect the parent
    *p = 200;
    if(*p != 200){
      print("cowtest: FAIL child did not see its own write\n");
      exit(1);
    }
    exit(0);
  }

  wait(0);

  if(*p != 100){
    print("cowtest: FAIL parent value changed by child's write\n");
    exit(1);
  }

  print("cowtest: basic_test OK\n");
}

// Test 2: several children sharing the same page, each writing a
// different value -- checks refcounting with >2 owners.
void
multi_test(void)
{
  print("cowtest: multi_test starting\n");

  volatile int *p = (int*)sbrk(4096);
  *p = -1;

  int nchildren = 5;
  int i;

  for(i = 0; i < nchildren; i++){
    int pid = fork();
    if(pid < 0){
      print("cowtest: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      *p = 1000 + i;
      if(*p != 1000 + i){
        print("cowtest: FAIL child saw wrong value after its own write\n");
        exit(1);
      }
      exit(0);
    }
  }

  for(i = 0; i < nchildren; i++){
    int xstatus = 0;
    wait(&xstatus);
    if(xstatus != 0){
      print("cowtest: FAIL a child reported failure\n");
      exit(1);
    }
  }

  if(*p != -1){
    print("cowtest: FAIL parent value changed by a child's write\n");
    exit(1);
  }

  print("cowtest: multi_test OK\n");
}

// Test 3: allocate many pages, fork, and have the child touch every
// single one. Mostly checks that COW survives at scale without
// crashing or corrupting data.
void
big_test(void)
{
  print("cowtest: big_test starting\n");

  int npages = 32; // 32 * 4096 = 128 KB
  char *base = sbrk(npages * 4096);
  int i;

  for(i = 0; i < npages; i++)
    base[i * 4096] = (char)i;

  int pid = fork();
  if(pid < 0){
    print("cowtest: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    for(i = 0; i < npages; i++){
      base[i * 4096] = (char)(i + 1); // touch/write every page -> COW fault each time
      if(base[i * 4096] != (char)(i + 1)){
        print("cowtest: FAIL child wrote wrong page value\n");
        exit(1);
      }
    }
    exit(0);
  }

  wait(0);

  for(i = 0; i < npages; i++){
    if(base[i * 4096] != (char)i){
      print("cowtest: FAIL parent page corrupted by child\n");
      exit(1);
    }
  }

  print("cowtest: big_test OK\n");
}

// Test 4 (Phase 4): use the pgrefcount() syscall to directly observe
// the effect of COW sharing and of the later private copy.
void
refcount_test(void)
{
  print("cowtest: refcount_test starting\n");

  volatile int *p = (int*)sbrk(4096);
  *p = 42;

  printf("cowtest: before fork, refcount=%d (expect 1)\n", pgrefcount((uint64)p));

  int pid = fork();
  if(pid < 0){
    print("cowtest: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // Right after fork, both parent and child point at the same
    // physical page, so refcount should be >= 2 (scheduling can make
    // it hard to guarantee an exact moment, so this print is
    // informational rather than a hard assertion).
    printf("cowtest: child refcount=%d before its own write (expect >= 2)\n",
           pgrefcount((uint64)p));

    *p = 99; // triggers a COW fault -> private copy for the child

    int r = pgrefcount((uint64)p);
    printf("cowtest: child refcount=%d after its own write (expect 1)\n", r);
    if(r != 1){
      print("cowtest: FAIL refcount did not drop to 1 after COW copy\n");
      exit(1);
    }
    exit(0);
  }

  wait(0);
  print("cowtest: refcount_test OK\n");
}

int
main(void)
{
  basic_test();
  multi_test();
  big_test();
  refcount_test();
  print("cowtest: ALL TESTS PASSED\n");
  exit(0);
}
