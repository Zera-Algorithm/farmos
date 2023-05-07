#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "printf.h"

void initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

void acquire(struct spinlock *lk) {
    // disable interrupts
    push_off();

    if (holding(lk)) {
        panic("acquire error. already hold lock %s", lk->name);
    }

    // 尝试获得锁
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0);

    // fence，保证锁确实已被获得
    __sync_synchronize();
    lk->cpu = mycpu();
}

void release(struct spinlock *lk) {
    if (!holding(lk)) {
        panic("release error. not hold lock %d", lk->name);
    }
    lk->cpu = 0;

    // fence，保证之前的操作生效
    __sync_synchronize();

    // 之所以不用C的赋值语句，因为C中的赋值语句可能被多次实现而引发问题
    __sync_lock_release(&lk->locked);

    pop_off();
}

int holding(struct spinlock *lk) {
    int r;
    // 当前cpu holding条件：lk锁住，且持有的cpu是本cpu
    r = (lk->locked && lk->cpu == mycpu());
    return r;
}

// 叠加关中断的层次
void push_off(void) {
    int old = intr_get();
    intr_off();
    if (mycpu()->noff == 0) {
        mycpu()->intena = old;
    }
    mycpu()->noff += 1;
}

void pop_off(void) {
    struct cpu *c = mycpu();
    c->noff -= 1;
    if (c->noff == 0 && c->intena) {
        intr_on();
    }
}