#include "proc/proc.h"
#include "defs.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

struct cpu cpus[NCPU];

int cpuid() {
	int id = r_tp();
	return id;
}

struct cpu *mycpu(void) {
	int id = cpuid();
	return (&cpus[id]);
}
