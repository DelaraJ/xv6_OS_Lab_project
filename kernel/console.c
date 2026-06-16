//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
    // توی consolewrite
    // اگر یکی ارسالش تموم نشده باشه اسلیپ میشه
    // اینجا بحای اسلیپ توی وایلی busy waiting میشه اسلیپ نمیشه
    // دیگه منتظر اینتراپت نمیمونه
    // همون لحظه که از رجیستر آیدل بشه مقدارش، کارکتر رو رایت میکنه به کاربر
    // این برای سریع تر شدن برگشته هست
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
// یه بافر از کاربر گرفته میاد تا اون ان تایی که کاربر گفته بفرست
{
  char buf[32];
  int i = 0;

  while(i < n){
    int nn = sizeof(buf);
    if(nn > n - i)
      nn = n - i;
      // اول توی buf کپی میکنه
    if(either_copyin(buf, user_src, src+i, nn) == -1)
      break;
    uartwrite(buf, nn);
    i += nn;
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
// یه بافر از کاربر گرفته 
// و میزانی که باید ازش بخونه
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){ // این یعنی سر و تهش برابره = بافرمون خالیه
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock); // پشت cons.r بخواب هروقت اینتراپتی رسید تورو بیدار میکنیم
      // هروقت یه خط کاملو گرفته باشه ازین اسلیپه بیدار میشه
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];
    // کارکتر به کارکتر میخونه میره جلو

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    // هر کارکتری که میخونه کپی میزنه به بافر user_dst
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);
  // انگاری کل دستورو برمیگردونه به شل
  // یعنی الان دیگه کل دستورو شل خوند از واسط سریال(کنسول) 
  // بعد دیکه اون پردازش میکنه و درجواب console_Write میکنه 
  return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      // کارکتری که گرفتیم رو به کاربر پس میدیم که تو کنسول ببینه چی نوشته 
      // ولی consolewrite رو صدا نزده که
      // توی consolewrite
      // اگر یکی ارسالش تموم نشده باشه اسلیپ میشه
      // اینجا بحای اسلیپ توی وایلی busy waiting میشه اسلیپ نمیشه
      consputc(c);

      // store for consumption by consoleread().
      // یه بایتی که گرفته رو داخل بافره مینویسه
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      // ولی تا وقتی که به انتهای لاین برسه کسی رو بیدار نمیکنه
      // تا وقتی که کاربر یه دستور کامل زد و اینتر زد
      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        // حالا همه کسایی که منتظر خوندن بافر بودنو بیدار میکنه
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
