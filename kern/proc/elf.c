#include <lib/elf.h>
#include <lib/log.h>
#include <lib/string.h>
#include <lib/transfer.h>
#include <mm/vmm.h>
#include <proc/proc.h>
#include <proc/thread.h>

/**
 * @brief 从src加载数据，填充va指向的页（起始于offset），并映射到进程的地址空间
 * @param offset 相对内存va的偏移
 */
int loadDataMapper(void *data, u64 va, size_t offset, u64 perm, const void *src, size_t len) {

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
			panic_on(loadElfSegment(ph, binary + ph->p_off, loadDataMapper,
						td->td_proc->p_pt));
			*maxva = MAX(*maxva, ph->p_vaddr + ph->p_memsz - 1);
		}
	}

	*maxva = PGROUNDUP(*maxva);

	// 设置代码入口点
	td->td_trapframe.epc = elfHeader->e_entry;
	return 0;
}

void proc_initucode(proc_t *p, thread_t *inittd, const void *bin, size_t size) {
	panic_on(loadCode(inittd, bin, size, &p->p_brk));
}