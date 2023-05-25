#include "dev/dtb.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

uint64 dtbEntry = 0;
struct MemInfo memInfo;

static void swapChar(void *a, void *b) {
	char c = *(char *)a;
	*(char *)a = *(char *)b;
	*(char *)b = c;
}

/**
 * @brief 将bin中所有32位的数据从大端序转为小端序。要求size必须是4的倍数
 * @example 0x04_03_02_01 -> 0x01_02_03_04
 */
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
	log(LEVEL_MODULE, "Read FDTHeader:\n");
	log(LEVEL_MODULE, "magic             = 0x%lx\n", (long)fdtHeader->magic);
	log(LEVEL_MODULE, "totalsize's addr  = 0x%lx\n", &(fdtHeader->totalsize));
	log(LEVEL_MODULE, "totalsize         = %d\n", fdtHeader->totalsize);
	log(LEVEL_MODULE, "off_dt_struct     = 0x%x\n", fdtHeader->off_dt_struct);
	log(LEVEL_MODULE, "off_dt_strings    = 0x%x\n", fdtHeader->off_dt_strings);
	log(LEVEL_MODULE, "off_mem_rsvmap    = 0x%x\n", fdtHeader->off_mem_rsvmap);
	log(LEVEL_MODULE, "version           = %d\n", fdtHeader->version);
	log(LEVEL_MODULE, "last_comp_version = %d\n", fdtHeader->last_comp_version);
	log(LEVEL_MODULE, "boot_cpuid_phys   = %d\n", fdtHeader->boot_cpuid_phys);
	log(LEVEL_MODULE, "size_dt_strings   = %d\n", fdtHeader->size_dt_strings);
	log(LEVEL_MODULE, "size_dt_struct    = %d\n", fdtHeader->size_dt_struct);
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
static char strBuf[128];
void *parseFdtNode(struct FDTHeader *fdtHeader, void *node, char *parent) {
	char *node_name;
	while (readBigEndian32(node) == FDT_NOP) {
		node += 4;
	}
	assert(readBigEndian32(node) == FDT_BEGIN_NODE);
	node += 4;

	node_name = (char *)node;
	log(LEVEL_MODULE, "node's name:    %s\n", node_name);
	log(LEVEL_MODULE, "node's parent:  %s\n", parent);
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
					log(LEVEL_MODULE, "name:   %s\n", name);
				}
				log(LEVEL_MODULE, "len:    %d\n", len);

				// values需要以info形式输出
				if (len == 4 || len == 8 || len == 16 || len == 32) {
					const char pre[] = "values: ";
					sprintf(strBuf, pre);
					value = (void *)node;
					for (int i = 0; i < len; i++) {
						sprintf(strBuf + sizeof(pre) - 1 + 3 * i, "%02x ",
							*(uint8 *)(node + i));
					}
					log(LEVEL_MODULE, "%s\n", strBuf);
				} else {
					nodeStr = (char *)node;
					log(LEVEL_MODULE, "values: %s\n", nodeStr);
				}

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
	log(LEVEL_MODULE, "\n");

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
	log(LEVEL_GLOBAL, "Find dtbEntry address at 0x%08lx\n", dtbEntry);
	struct FDTHeader *fdt_h = (struct FDTHeader *)dtbEntry;
	parserFdtHeader(fdt_h);

	void *node = (void *)(dtbEntry + fdt_h->off_dt_struct);
	do {
		node = parseFdtNode(fdt_h, node, "root");
	} while (readBigEndian32(node) != FDT_END);

	log(LEVEL_GLOBAL, "Memory Start Addr = 0x%016lx, size = %d MB\n", memInfo.start,
	    memInfo.size / 1024 / 1024);
}
