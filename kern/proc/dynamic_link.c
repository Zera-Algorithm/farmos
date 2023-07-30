#include <proc/procarg.h>
#include <proc/proc.h>
#include <lib/elf.h>
#include <lib/log.h>
#include <lib/error.h>
#include <lib/string.h>
#include <proc/dynamic_link.h>
#include <proc/thread.h>
#include <fs/kload.h>
#include <mm/kmalloc.h>

// 将动态链接库映射到虚拟地址空间
static void map_dynamic_so(thread_t *td, const void *binary, size_t size) {
	const ElfHeader *elfHeader = getElfFrom(binary, size);

	// 1. 判断ELF文件是否合法
	if (elfHeader == NULL) {
		panic("bad dynamic so elf!");
	}

	// 2. 加载文件到指定的虚拟地址
	size_t phOff;
	ELF_FOREACH_PHDR_OFF (phOff, elfHeader) {
		ProgramHeader new_ph = *(ProgramHeader *)(binary + phOff);
		// 只加载能加载的段
		if (new_ph.p_type == PT_LOAD) {
			// libc.so是位置无关的库，可以将其虚拟地址空间平移到U_DYNAMIC_SO_START位置
			new_ph.p_vaddr += U_DYNAMIC_SO_START;
			panic_on(loadElfSegment(&new_ph, binary + new_ph.p_off, loadDataMapper,
						td->td_proc->p_pt));
		}
	}

	// 3. 将程序入口地址设为动态链接库的入口地址
	td->td_trapframe.epc = elfHeader->e_entry + U_DYNAMIC_SO_START;
}

/**
 * @brief 加载动态链接库到内存，如果没有动态链接库，返回0
 */
u64 load_dynamic_so(thread_t *td, const void *binary, size_t size, const ElfHeader *elf) {
	// 1. 找到动态链接库名称
	char so_name[MAX_NAME_LEN];
	void *so_binary;
	size_t so_size;
	u64 so_base = U_DYNAMIC_SO_START; // 加载动态链接库的起始地址

	size_t phOff;
	int is_find = 0;
	ELF_FOREACH_PHDR_OFF (phOff, elf) {
		ProgramHeader *ph = (ProgramHeader *)(binary + phOff);
		if (ph->p_type == PT_INTERP) {
			// 找到动态链接库名称
			const char *so_name_ptr = (const char *)(binary + ph->p_off);
			strncpy(so_name, so_name_ptr, ph->p_filesz);
			is_find = 1;
			break;
		}
	}
	if (!is_find) {
		log(PROC_GLOBAL, "no dynamic so\n");
		return 0;
	}

	// 2. 加载动态链接库
	fileid_t file_id = file_load(so_name, &so_binary, &so_size);
	log(DEBUG, "load dynamic so BEGIN: %s\n", so_name);
	map_dynamic_so(td, so_binary, so_size);
	log(DEBUG, "load dynamic so END: %s\n", so_name);
	file_unload(file_id);
	return so_base;
}


// 解析ELF文件信息，以填充辅助数组
void parseElf(thread_t *td, const void *binary, size_t size, stack_arg_t *parg) {
	const ElfHeader *elf = getElfFrom(binary, size);
	if (elf == NULL) {
		panic("bad elf");
	}

	// 1. 寻找段表的虚拟地址
	u64 va_segtable = 0;
	size_t phtable_off = elf->e_phoff;
	size_t phOff;
	int find_segtable = 0;
	ELF_FOREACH_PHDR_OFF (phOff, elf) {
		ProgramHeader *ph = (ProgramHeader *)(binary + phOff);
		// 只加载能加载的段
		if (ph->p_type == ELF_PROG_LOAD) {
			if (ph->p_off <= phtable_off && phtable_off < ph->p_off + ph->p_filesz) {
				// 说明段表在此段中
				va_segtable = ph->p_vaddr + (phtable_off - ph->p_off);
				find_segtable = 1;
				log(PROC_GLOBAL, "we find va of segment table = 0x%lx\n", va_segtable);
				break;
			}
		}
	}
	assert(find_segtable);

	// 2. 加载动态链接库
	u64 dynamic_base = load_dynamic_so(td, binary, size, elf);

	// 16bytes 随机字符串
	char u_rand_bytes[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
	u64 uaddr_rand = push_data(td->td_proc->p_pt, &td->td_trapframe.sp, u_rand_bytes, 16, true);
	// 3. 拷入辅助数组
	/**
	 * 辅助数组的字段由type和value两个组成，都是u64类型
	 * 辅助数组以NULL, NULL表示结束
	 */
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_HWCAP, 0}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_PAGESZ, PAGE_SIZE}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_PHDR, va_segtable}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_PHENT, sizeof(ProgramHeader)}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_PHNUM, elf->e_phnum}, &parg->total_len);

	if (dynamic_base != 0) {
		append_auxiliary_vector(parg->argvbuf, (u64[]){AT_BASE, dynamic_base}, &parg->total_len);
	}

	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_ENTRY, elf->e_entry}, &parg->total_len);
	// 不启动安全模式，从环境变量加载动态链接库地址
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_SECURE, 0}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_RANDOM, uaddr_rand}, &parg->total_len);
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_EXECFN, (u64)parg->argvbuf[0]}, &parg->total_len); // 程序的名称
	append_auxiliary_vector(parg->argvbuf, (u64[]){AT_NULL, AT_NULL}, &parg->total_len);
	// reference: glibc

	// 4. 将argvbuf压入用户栈
	push_data(td->td_proc->p_pt, &td->td_trapframe.sp, parg->argvbuf, parg->total_len * sizeof(char *), true);

	// 5. 将参数数量压入用户栈
	push_data(td->td_proc->p_pt, &td->td_trapframe.sp, &parg->argc, sizeof(u64), false);

	// 释放分配的argvbuf
	kfree(parg->argvbuf);
}
