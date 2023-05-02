#include "dtb.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "types.h"

uint64 dtb_entry;
struct MemInfo {
	uint64 start;
	uint64 size;
} memInfo;

static void swap(void *a, void *b) {
	char c = *(char *)a;
	*(char *)a = *(char *)b;
	*(char *)b = c;
}

// 将bin中所有32位的数据转为大端序
// 4321 -> 1234
void endian_big2little(void *bin, int size) {
	for (int i = 0; i < size; i += 4) {
		swap(bin + i, bin + i + 3);
		swap(bin + i + 1, bin + i + 2);
	}
}

// dtb中大部分字段都是32位，且是大端序存储
void parser_fdt_header(struct fdt_header *fdt_h) {
	endian_big2little(fdt_h, sizeof(struct fdt_header));
	printf("Read fdt_header:\n");
	printf("magic             = 0x%lx\n", (long)fdt_h->magic);
	printf("totalsize's addr  = 0x%lx\n", &(fdt_h->totalsize));
	printf("totalsize         = %d\n", fdt_h->totalsize);
	printf("off_dt_struct     = 0x%x\n", fdt_h->off_dt_struct);
	printf("off_dt_strings    = 0x%x\n", fdt_h->off_dt_strings);
	printf("off_mem_rsvmap    = 0x%x\n", fdt_h->off_mem_rsvmap);
	printf("version           = %d\n", fdt_h->version);
	printf("last_comp_version = %d\n", fdt_h->last_comp_version);
	printf("boot_cpuid_phys   = %d\n", fdt_h->boot_cpuid_phys);
	printf("size_dt_strings   = %d\n", fdt_h->size_dt_strings);
	printf("size_dt_struct    = %d\n", fdt_h->size_dt_struct);
}

inline uint32 read_bigendian32(void *p) {
	uint32 ret = 0;
	char *p1 = (char *)p, *p2 = (char *)(&ret);
	p2[0] = p1[3];
	p2[1] = p1[2];
	p2[2] = p1[1];
	p2[3] = p1[0];
	return ret;
}

inline uint64 read_bigendian64(void *p) {
	uint64 ret = 0;
	char *p1 = (char *)p, *p2 = (char *)(&ret);
	p2[0] = p1[7];
	p2[1] = p1[6];
	p2[2] = p1[5];
	p2[3] = p1[4];
	p2[4] = p1[3];
	p2[5] = p1[2];
	p2[6] = p1[1];
	p2[7] = p1[0];
	return ret;
}

void assert(int v) {
	if (!v) {
		panic("assert error");
	}
}

#define FOURROUNDUP(sz) (((sz) + 4 - 1) & ~(4 - 1))

// parse a tree structure node, return the end addr of the node
// parent argument表示上一级节点的名称
void *parser_fdt_node(struct fdt_header *fdt_h, void *node, char *parent) {
	char *node_name;
	while (read_bigendian32(node) == FDT_NOP) {
		node += 4;
	}
	assert(read_bigendian32(node) == FDT_BEGIN_NODE);
	node += 4;

	node_name = (char *)node;
	printf("node's name:    %s\n", node_name);
	printf("node's parent:  %s\n", parent);
	node += (strlen((char *)node) + 1);
	node = (void *)FOURROUNDUP((uint64)node); // roundup to multiple of 4

	// scan properties
	while (1) {
		while (read_bigendian32(node) == FDT_NOP) {
			node += 4;
		}
		if (read_bigendian32(node) == FDT_PROP) {
			char *nodeStr = NULL;
			void *value = NULL;

			while (read_bigendian32(node) == FDT_PROP) {
				node += 4;
				uint32 len = read_bigendian32(node);
				node += 4;
				uint32 nameoff = read_bigendian32(node);
				node += 4;
				char *name = (char *)(node + fdt_h->off_dt_strings + nameoff);

				if (name[0] != '\0') {
					printf("name:   %s\n", name);
				}
				printf("len:    %d\n", len);
				printf("values: ");
				if (len == 4 || len == 8 || len == 16 || len == 32) {
					value = (void *)node;
					for (int i = 0; i < len; i++) {
						printf("0x%x ", *(uint8 *)(node + i));
					}
				} else {
					nodeStr = (char *)node;
					printf("%s", nodeStr);
				}
				printf("\n");
				node = (void *)FOURROUNDUP((uint64)(node + len));
				while (read_bigendian32(node) == FDT_NOP) {
					node += 4;
				}
			}

			// 读完所有的FDT_PROP了
			if (nodeStr != NULL && strncmp(nodeStr, "memory", 7) == 0) {
				memInfo.start = read_bigendian64(value);
				memInfo.size = read_bigendian64(value + 8);
			}
		} else {
			break;
		}
	}
	printf("\n");

	while (read_bigendian32(node) != FDT_END_NODE) {
		node = parser_fdt_node(fdt_h, node, node_name);
		while (read_bigendian32(node) == FDT_NOP) {
			node += 4;
		}
	}

	return node + 4;
}

void dtb_parser() {
	extern uint64 dtb_entry;
	printf("dtb_entry = %lx\n", dtb_entry);
	struct fdt_header *fdt_h = (struct fdt_header *)dtb_entry;
	parser_fdt_header(fdt_h);

	void *node = (void *)(dtb_entry + fdt_h->off_dt_struct);
	do {
		node = parser_fdt_node(fdt_h, node, "root");
	} while (read_bigendian32(node) != FDT_END);

	printf("Memory Start Addr = 0x%016lx, size = %d MB\n", memInfo.start,
	       memInfo.size / 1024 / 1024);
}