#include <lib/log.h>
#include <mm/vmm.h>
#include <mm/vmtools.h>

err_t _vmUnmapper(Pte *pd, u64 target_va, Pte *target_pte, void *arg) {
	// 对传入的页表项进行解映射
	return ptUnmap(pd, target_va);
}

pte_callback_t vmUnmapper = _vmUnmapper;

err_t _kvmUnmapper(Pte *pd, Pte *target_pt, u64 ptlevel, void *arg) {
	// 对传入的页表进行解引用
	kvmFree((u64)target_pt);
	return 0;
}

pt_callback_t kvmUnmapper = _kvmUnmapper;

err_t pdWalk(Pte *pd, pte_callback_t pte_callback, pt_callback_t pt_callback, void *arg) {
	// 遍历进程页目录
	for (u64 i = 0; i < PAGE_INDEX_MAX; i++) {
		// 页目录项（二级页表基地址）有效时递归回收
		if (pd[i] & PTE_V) {
			Pte *pt1 = (Pte *)pteToPa(pd[i]);

			for (u64 j = 0; j < PAGE_INDEX_MAX; j++) {
				// 页表项（三级页表基地址）有效时递归回收
				if (pt1[j] & PTE_V) {
					Pte *pt2 = (Pte *)pteToPa(pt1[j]);

					for (u64 k = 0; k < PAGE_INDEX_MAX; k++) {
						if (pt2[k] != 0) {
							u64 va = ((((i << PAGE_INDEX_LEN) + j) << PAGE_INDEX_LEN) + k)
								 << PAGE_SHIFT;
							// 操作物理页
							if (pte_callback != NULL) {
								unwrap(pte_callback(pd, va, &pt2[k], arg));
							}
						}
					}
					// 操作三级页表
					if (pt_callback != NULL) {
						unwrap(pt_callback(pd, pt2, 3, arg));
					}
				}
			}
			// 操作二级页表
			if (pt_callback != NULL) {
				unwrap(pt_callback(pd, pt1, 2, arg));
			}
		}
	}
	// 操作一级页表（页目录）
	if (pt_callback != NULL) {
		unwrap(pt_callback(pd, pd, 1, arg));
	}
	return 0;
}