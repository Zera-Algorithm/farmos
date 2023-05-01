#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

int
cpuid()
{
  int id = r_tp();
  return id;
}
