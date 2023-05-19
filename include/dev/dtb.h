#ifndef _DTB_H
#define _DTB_H
#include "types.h"
// device tree blob: 设备树

struct FDTHeader {
	unsigned int magic;
	unsigned int totalsize;
	unsigned int off_dt_struct;
	unsigned int off_dt_strings;
	unsigned int off_mem_rsvmap;
	unsigned int version;
	unsigned int last_comp_version;
	unsigned int boot_cpuid_phys;
	unsigned int size_dt_strings;
	unsigned int size_dt_struct;
};

struct MemInfo {
	uint64 start;
	uint64 size;
};

extern uint64 dtbEntry;
void parseDtb();

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009
#endif
