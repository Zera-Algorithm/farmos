#include <fs/fd.h>
#include <fs/kload.h>
#include <fs/vfs.h>
#include <lib/log.h>
#include <mm/kmalloc.h>
#include <mm/mmu.h>
#include <mm/vmm.h>
#include <proc/interface.h>
#include <proc/proc.h>
#include <sys/syscall.h>
#include <sys/syscall_fs.h>
#include <sys/syscall_mmap.h>
#include <types.h>

/**
 * @brief 本文件管理mmap相关系统调用
 * 一切线程发起的针对进程的map和unmap都需要加进程的锁，因为可能会有并发
 */

// // 记录mmap信息的队列
// static mmap_fd_info_list_t mmap_fd_info_list = {NULL};

static void swap(u64 *a, u64 *b) {
	u64 tmp = *a;
	*a = *b;
	*b = tmp;
}

/**
 * @brief 计算两个区间的交集
 * @return 如果没有交集，则len返回0
 */
static inline void getIntersection(u64 start1, u64 len1, u64 start2, u64 len2, u64 *intersect_start,
				   u64 *intersect_len) {
	// 保证1的left最小，2的left最大
	if (start1 > start2) {
		swap(&start1, &start2);
		swap(&len1, &len2);
	}

	if (start2 >= start1 + len1) {
		// 两个区间没有交集
		*intersect_start = 0;
		*intersect_len = 0;
	} else {
		*intersect_start = start2;
		*intersect_len = MIN(start1 + len1 - start2, len2);
	}
}

static inline u64 get_perm_by_prot(int prot) {
	u64 perm = PTE_U;
	if (prot & PROT_EXEC) {
		perm |= PTE_X;
	}
	if (prot & PROT_READ) {
		perm |= PTE_R;
	}
	if (prot & PROT_WRITE) {
		perm |= PTE_W;
	}
	return perm;
}

/**
 * @brief 将文件映射到进程的虚拟内存空间
 * @note 如果start == 0，则由内核指定虚拟地址
 * @todo 实现匿名映射（由flags的MAP_ANONYMOUS标志决定），即不映射到文件
 */
// TODO: 需要考虑flags的其他字段，但暂未考虑
// TODO: 实现将映射地址区域与文件结合，实现msync同步内存到文件
void *sys_mmap(u64 start, size_t len, int prot, int flags, int fd, off_t off) {
	mtx_lock(&cur_proc()->p_lock);

	// 打印参数
	log(LEVEL_GLOBAL,
	    "mmap: start = %lx, len = %lx, prot = %x, flags = %lx, fd = %d, off = %d\n", start, len,
	    prot, flags, fd, off);

	int r = 0;
	u64 perm = 0;
	Dirent *file;

	// 将len向上提升至分页的整数倍
	len = PGROUNDUP(len);
	if (len == 0) {
		warn("mmap len is 0!\n");
		mtx_unlock(&cur_proc()->p_lock);
		return MAP_FAILED;
	}
	start = PGROUNDUP(start);

	// 1. 当start为0时，由内核指定用户的虚拟地址，记录到start中
	if (start == 0) {
		// Note: 指定的固定地址在MMAP_START到MMAP_END之间
		start = cur_proc_fs_struct()->mmap_addr;
		u64 new_addr = PGROUNDUP(start + len);

		if (new_addr > MMAP_END) {
			warn("no more free mmap space to alloc!");
			mtx_unlock(&cur_proc()->p_lock);
			return MAP_FAILED;
		}

		cur_proc_fs_struct()->mmap_addr = new_addr;
	}

	// 2. 指定权限位
	perm = get_perm_by_prot(prot);

	if (flags & MAP_ANONYMOUS) {
		// A. 匿名映射
		r = sys_map(start, len, perm);

		if (r < 0) {
			warn("sys_map error!\n");
			mtx_unlock(&cur_proc()->p_lock);
			return MAP_FAILED;
		}

		mtx_unlock(&cur_proc()->p_lock);
		return (void *)start;
	} else {
		mtx_unlock(&cur_proc()->p_lock);

		// B. 文件映射
		// 3. 通过fd获取文件
		r = getDirentByFd(fd, &file, NULL);
		if (r < 0) {
			warn("get fd(%d) error!\n", fd);
			return MAP_FAILED;
		}

		return file_map(cpu_this()->cpu_running, file, start, len, perm, off);
	}
}

/**
 * @brief 释放从start到len的映射
 */
err_t sys_unmap(u64 start, u64 len) {
	u64 from = PGROUNDUP(start);
	u64 to = PGROUNDDOWN(start + len - 1);

	mtx_lock(&cur_proc()->p_lock);
	for (u64 va = from; va <= to; va += PAGE_SIZE) {
		// 释放虚拟地址所在的页
		panic_on(ptUnmap(cur_proc_pt(), va));
	}
	mtx_unlock(&cur_proc()->p_lock);

	return 0;
}

// 根据cjy的OS，可以暂不实现
err_t sys_msync(u64 addr, size_t length, int flags) {
	return 0;
}

// 仅仅改变映射了的页的属性
err_t sys_mprotect(u64 addr, size_t len, int prot) {
	u64 from = PGROUNDDOWN(addr);
	u64 to = PGROUNDUP(addr + len - 1);
	u64 perm = get_perm_by_prot(prot);

	pte_t *pt = cur_proc_pt();
	for (u64 va = from; va < to; va += PAGE_SIZE) {
		// 若虚拟地址对应的物理地址不存在，则跳过
		u64 pte = ptLookup(pt, va);
		if (pte & PTE_PASSIVE) {
			// 被动有效 -> 被动有效（更新权限）
			panic_on(ptMap(pt, va, 0, perm | PTE_PASSIVE));
		} else if (pte & PTE_V) {
			// 有效 -> 有效（更新权限）
			panic_on(ptMap(pt, va, pteToPa(pte), perm));
		} else {
			// 无效页不应该调用 mprotect
			warn("sys_mprotect: va = %lx, pte = %lx\n", va, pte);
		}
	}
	return 0;
}
