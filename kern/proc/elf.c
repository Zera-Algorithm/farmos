#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fd.h>
#include <fs/fs.h>
#include <fs/vfs.h>
#include <lib/elf.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/thread.h>

/**
 * @brief 加载一页数据，并映射到进程的地址空间
 * @param offset 相对内存va的偏移
 */
static int loadDataMapper(void *data, u64 va, size_t offset, u64 perm, const void *src,
			  size_t len) {

	// Step1: 分配一个页
	u64 pa = vmAlloc();

	// Step2: 复制段内数据
	if (src != NULL) {
		memcpy((void *)pa + offset, src, len);
	}

	// Step3: 将页pa插入到proc的页表中
	return ptMap((Pte *)data, va, pa, perm);
}

/**
 * @brief 加载ELF文件的各个段到指定的位置
 * @attention 你需要保证proc->trapframe已经设置完毕
 * @param maxva 你分给进程的虚拟地址的最大值，用于计算最初的Program Break
 */
static int loadCode(thread_t *td, const void *binary, size_t size, u64 *maxva) {
	const ElfHeader *elfHeader = getElfFrom(binary, size);

	// 1. 判断ELF文件是否合法
	if (elfHeader == NULL) {
		return -E_BAD_ELF;
	}

	*maxva = 0;

	// 2. 加载文件到指定的虚拟地址
	size_t phOff;
	ELF_FOREACH_PHDR_OFF (phOff, elfHeader) {
		ProgramHeader *ph = (ProgramHeader *)(binary + phOff);
		// 只加载能加载的段
		if (ph->p_type == ELF_PROG_LOAD) {
			panic_on(loadElfSegment(ph, binary + ph->p_off, loadDataMapper, td->td_pt));
			*maxva = MAX(*maxva, ph->p_vaddr + ph->p_memsz - 1);
		}
	}

	*maxva = PGROUNDUP(*maxva);

	// 设置代码入口点
	td->td_trapframe->epc = elfHeader->e_entry;
	return 0;
}

////////////////////////
static char strBuf[1024];
/**
 * @param argv char *[]类型
 */
static inline u64 initStack(void *stackTop, u64 argv) {
	u64 argUPtr[20]; // TODO: 可以允许更多的参数
	u64 argc = 0;
	void *stackNow = stackTop;

	// 1. 存储 argv 数组里面的字符串
	do {
		u64 pStr, len;
		copyIn(argv, &pStr, sizeof(void *));

		// 空指针，表示结束
		if (pStr == 0) {
			break;
		}

		copyInStr(pStr, strBuf, 1024);
		len = strlen(strBuf);

		log(LEVEL_GLOBAL, "passed argv str: %s\n", strBuf);

		stackNow -= (len + 1);
		safestrcpy(stackNow, strBuf, 1024);

		argUPtr[argc++] = USTACKTOP - (stackTop - stackNow);

		argv += sizeof(char *); // 读取下一个字符串
	} while (1);
	argUPtr[argc++] = 0;

	// 2. 存储 char * 数组 argv
	stackNow -= argc * sizeof(char *);
	memcpy(stackNow, argUPtr, argc * sizeof(char *));

	argc -= 1;

	// 3. 存储argc（argv由用户本地计算）
	stackNow -= sizeof(long);
	*(u64 *)stackNow = argc;

	return USTACKTOP - (stackTop - stackNow);
}

// /**
//  * @brief 读取一个文件到内核的虚拟内存
//  * @param binary 返回文件的虚拟地址
//  * @param size 返回文件的大小
//  */
// static void loadFileToKernel(Dirent *file, void **binary, int *size) {
// 	int _size;
// 	void *_binary;

// 	*size = _size = file->fileSize;
// 	*binary = _binary = (void *)KERNEL_TEMP;
// 	extern Pte *kernPd; // 内核页表

// 	// 1. 分配足够的页
// 	int npage = (_size) % PAGE_SIZE == 0 ? (_size / PAGE_SIZE) : (_size / PAGE_SIZE + 1);
// 	for (int i = 0; i < npage; i++) {
// 		u64 pa = vmAlloc();
// 		u64 va = ((u64)_binary) + i * PAGE_SIZE;
// 		panic_on(ptMap(kernPd, va, pa, PTE_R | PTE_W));
// 	}

// 	// 2. 读取文件
// 	fileRead(file, 0, (u64)_binary, 0, _size);
// }

/**
 * @brief 加载用户程序，设置 Trapframe 中的程序入口（epc）。
 */
void td_initucode(thread_t *td, const void *binary, size_t size) {
	panic_on(loadCode(td, binary, size, &td->td_brk));
}