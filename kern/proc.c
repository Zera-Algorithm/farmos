#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

int cpuid() {
	int id = r_tp();
	return id;
}
