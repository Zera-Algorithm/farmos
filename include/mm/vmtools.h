#ifndef _VMTOOLS_H
#define _VMTOOLS_H

#include <types.h>

typedef err_t (*pte_callback_t)(Pte *pd, u64 target_va, Pte *target_pte, void *arg);
typedef err_t (*pt_callback_t)(Pte *pd, Pte *target_pt, u64 ptlevel, void *arg);

err_t pdWalk(Pte *pd, pte_callback_t pte_callback, pt_callback_t pt_callback, void *arg);

extern pte_callback_t vmUnmapper;
extern pt_callback_t kvmUnmapper;

#endif // _VMM_H
