//
// formatted console output -- printf, panic.
//

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

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...)
{
  // TODO: TO IMPLEMENT

}

void
panic(char *s)
{
  // TODO: TO IMPLEMENT
  
  while(1);
}
