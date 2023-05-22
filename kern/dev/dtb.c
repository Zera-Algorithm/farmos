#include "dev/dtb.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

uint64 dtbEntry = 0;
struct MemInfo memInfo;
static int debug = 0;

static void swapChar(void *a, void *b) {
	char c = *(char *)a;
	*(char *)a = *(char *)b;
	*(char *)b = c;
}

// 将bin中所有32位的数据从大端序转为小端序
// 4321 -> 1234
void endianBigToLittle(void *bin, int size) {
	for (int i = 0; i < size; i += 4) {
		swapChar(bin + i, bin + i + 3);
		swapChar(bin + i + 1, bin + i + 2);
	}
}

/**
 * @brief 解析flatten device tree header的结构，将解析得到的信息打印出来
 * @param fdtHeader：设备树结构头指针
 * @returns 没有返回值
 * @note dtb中大部分字段都是32位，且是大端序存储
 */
void parserFdtHeader(struct FDTHeader *fdtHeader) {
	endianBigToLittle(fdtHeader, sizeof(struct FDTHeader));
	logIf(debug, "Read FDTHeader:\n");
	logIf(debug, "magic             = 0x%lx\n", (long)fdtHeader->magic);
	logIf(debug, "totalsize's addr  = 0x%lx\n", &(fdtHeader->totalsize));
	logIf(debug, "totalsize         = %d\n", fdtHeader->totalsize);
	logIf(debug, "off_dt_struct     = 0x%x\n", fdtHeader->off_dt_struct);
	logIf(debug, "off_dt_strings    = 0x%x\n", fdtHeader->off_dt_strings);
	logIf(debug, "off_mem_rsvmap    = 0x%x\n", fdtHeader->off_mem_rsvmap);
	logIf(debug, "version           = %d\n", fdtHeader->version);
	logIf(debug, "last_comp_version = %d\n", fdtHeader->last_comp_version);
	logIf(debug, "boot_cpuid_phys   = %d\n", fdtHeader->boot_cpuid_phys);
	logIf(debug, "size_dt_strings   = %d\n", fdtHeader->size_dt_strings);
	logIf(debug, "size_dt_struct    = %d\n", fdtHeader->size_dt_struct);
}

inline uint32 readBigEndian32(void *p) {
	uint32 ret = 0;
	char *p1 = (char *)p, *p2 = (char *)(&ret);
	p2[0] = p1[3];
	p2[1] = p1[2];
	p2[2] = p1[1];
	p2[3] = p1[0];
	return ret;
}

inline uint64 readBigEndian64(void *p) {
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

#define FOURROUNDUP(sz) (((sz) + 4 - 1) & ~(4 - 1))

/**
 * @brief 解析flatten device tree blob的单个Node，获取设备树的信息
 * @param fdtHeader：设备树的头指针
 * @param node：设备树节点指针
 * @param parent：node节点的父节点的名称
 * @returns 设备树节点node的尾地址
 */
// parse a tree structure node, return the end addr of the node
// parent argument表示上一级节点的名称
void *parseFdtNode(struct FDTHeader *fdtHeader, void *node, char *parent) {
	char *node_name;
	while (readBigEndian32(node) == FDT_NOP) {
		node += 4;
	}
	assert(readBigEndian32(node) == FDT_BEGIN_NODE);
	node += 4;

	node_name = (char *)node;
	logIf(debug, "node's name:    %s\n", node_name);
	logIf(debug, "node's parent:  %s\n", parent);
	node += (strlen((char *)node) + 1);
	node = (void *)FOURROUNDUP((uint64)node); // roundup to multiple of 4

	// scan properties
	while (1) {
		while (readBigEndian32(node) == FDT_NOP) {
			node += 4;
		}
		if (readBigEndian32(node) == FDT_PROP) {
			char *nodeStr = NULL;
			void *value = NULL;

			while (readBigEndian32(node) == FDT_PROP) {
				node += 4;
				uint32 len = readBigEndian32(node);
				node += 4;
				uint32 nameoff = readBigEndian32(node);
				node += 4;
				char *name = (char *)(node + fdtHeader->off_dt_strings + nameoff);

				if (name[0] != '\0') {
					logIf(debug, "name:   %s\n", name);
				}
				logIf(debug, "len:    %d\n", len);

				// values需要以info形式输出
				logIf(debug, "values: ");
				if (len == 4 || len == 8 || len == 16 || len == 32) {
					value = (void *)node;
					for (int i = 0; i < len; i++) {
						printfIf(debug, "0x%x ", *(uint8 *)(node + i));
					}
				} else {
					nodeStr = (char *)node;
					printfIf(debug, "%s", nodeStr);
				}
				printfIf(debug, "\n");
				// values输出完毕

				node = (void *)FOURROUNDUP((uint64)(node + len));
				while (readBigEndian32(node) == FDT_NOP) {
					node += 4;
				}
			}

			// 读完所有的FDT_PROP了
			if (nodeStr != NULL && strncmp(nodeStr, "memory", 7) == 0) {
				memInfo.start = readBigEndian64(value);
				memInfo.size = readBigEndian64(value + 8);
			}
		} else {
			break;
		}
	}
	printfIf(debug, "\n");

	while (readBigEndian32(node) != FDT_END_NODE) {
		node = parseFdtNode(fdtHeader, node, node_name);
		while (readBigEndian32(node) == FDT_NOP) {
			node += 4;
		}
	}

	return node + 4;
}

void parseDtb() {
	extern uint64 dtbEntry;
	logIf(debug, "dtbEntry = %lx\n", dtbEntry);
	struct FDTHeader *fdt_h = (struct FDTHeader *)dtbEntry;
	parserFdtHeader(fdt_h);

	void *node = (void *)(dtbEntry + fdt_h->off_dt_struct);
	do {
		node = parseFdtNode(fdt_h, node, "root");
	} while (readBigEndian32(node) != FDT_END);

	log("Memory Start Addr = 0x%016lx, size = %d MB\n", memInfo.start,
	    memInfo.size / 1024 / 1024);
}