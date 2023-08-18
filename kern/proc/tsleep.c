#include <proc/thread.h>
#include <proc/tsleep.h>
#include <proc/sleep.h>
#include <lib/queue.h>
#include <proc/cpu.h>
#include <dev/timer.h>
#include <lib/log.h>
#include <sys/errno.h>

// 私有结构体声明
#define NTSEVENTS 1024

typedef struct tsleepevent {
    thread_t *tse_td;
    void *tse_wchan;
    u64 tse_wakeus;
    TAILQ_ENTRY(tsleepevent) tse_freeq;
    TAILQ_ENTRY(tsleepevent) tse_usedq;
} tsevent_t;

typedef struct tseventq {
    TAILQ_HEAD(, tsleepevent) tseq_head;
    mutex_t tseq_lock;
} tseventq_t;

#define tseq_critical_enter(tseq) mtx_lock(&(tseq)->tseq_lock)
#define tseq_critical_exit(tseq) mtx_unlock(&(tseq)->tseq_lock)

// 私有变量声明及初始化
tsevent_t tsevents[NTSEVENTS];
tseventq_t tsevent_freeq;
tseventq_t tsevent_usedq;

void tsleep_init() {
    mtx_init(&tsevent_freeq.tseq_lock, "tse_freeq", false, MTX_SPIN);
    mtx_init(&tsevent_usedq.tseq_lock, "tse_usedq", false, MTX_SPIN);
    TAILQ_INIT(&tsevent_freeq.tseq_head);
    TAILQ_INIT(&tsevent_usedq.tseq_head);
    for (int i = NTSEVENTS - 1; i >= 0; i--) {
        tsevent_t *tse = &tsevents[i];
        tse->tse_wchan = NULL;
        tse->tse_td = NULL;
        TAILQ_INSERT_HEAD(&tsevent_freeq.tseq_head, tse, tse_freeq);
    }
}

static int tse_debug(tsevent_t *check, bool should_used, bool should_free) {
    tsevent_t *tmp = NULL;
    // check self loop
    TAILQ_FOREACH(tmp, &tsevent_usedq.tseq_head, tse_usedq) {
        if (tmp == tmp->tse_freeq.tqe_next) {
            return 0;
        }
    }

    // check in
    {
        bool pass = false;
        tmp = NULL;
        TAILQ_FOREACH(tmp, &tsevent_usedq.tseq_head, tse_usedq) {
            if (tmp == check) {
                pass = true;
                break;
            }
        }
        if (pass != should_used) {
            return 0;
        }
    }
    {
        bool pass = false;
        tmp = NULL;
        TAILQ_FOREACH(tmp, &tsevent_freeq.tseq_head, tse_freeq) {
            if (tmp == check) {
                pass = true;
                break;
            }
        }
        if (pass != should_free) {
            return 0;
        }
    }
	return 1;
}

// 资源管理函数
static tsevent_t *tse_alloc(thread_t *td, void *chan, u64 wakeus) {
    tseq_critical_enter(&tsevent_freeq);
    assert(!TAILQ_EMPTY(&tsevent_freeq.tseq_head));
    tsevent_t *tse = TAILQ_FIRST(&tsevent_freeq.tseq_head);
    assert(tse_debug(tse, 0, 1));
    TAILQ_REMOVE(&tsevent_freeq.tseq_head, tse, tse_freeq);
    assert(tse_debug(tse, 0, 0));
    tseq_critical_exit(&tsevent_freeq);

    tse->tse_td = td;
    tse->tse_wchan = chan;
    tse->tse_wakeus = wakeus;

    tseq_critical_enter(&tsevent_usedq);
    assert(tse_debug(tse, 0, 0));
    if (wakeus) {
        tsevent_t *temp_tse = NULL;
        // 从头开始遍历，找到第一个比 wakeus 大的元素，插入到其前面
        TAILQ_FOREACH(temp_tse, &tsevent_usedq.tseq_head, tse_usedq) {
            if (temp_tse->tse_wakeus > wakeus || temp_tse->tse_wakeus == 0) {
                break;
            }
        }
        if (temp_tse) {
            // 插入到 temp_tse 前面
            TAILQ_INSERT_BEFORE(temp_tse, tse, tse_usedq);
        } else {
            // 插入到队尾
            TAILQ_INSERT_TAIL(&tsevent_usedq.tseq_head, tse, tse_usedq);
        }
    } else {
        TAILQ_INSERT_TAIL(&tsevent_usedq.tseq_head, tse, tse_usedq);
    }
    assert(tse_debug(tse, 1, 0));
    return tse;
}

static void tse_set_unused(tsevent_t *tse, bool timeout) {
    assert(mtx_hold(&tsevent_usedq.tseq_lock));
    assert(tse_debug(tse, 1, 0));
    TAILQ_REMOVE(&tsevent_usedq.tseq_head, tse, tse_usedq);
    assert(tse_debug(tse, 0, 0));
    // warn("%08x(%08x) WAS WAKEUP(%d) at %d before %d\n", tse->tse_td->td_tid, tse->tse_td->td_proc->p_pid, timeout, getUSecs(), tse->tse_wakeus);
    tse->tse_wchan = timeout ? (void *)-1 : NULL;
}

static err_t tse_free(tsevent_t *tse) {
    err_t r = tse->tse_wchan == NULL ? 0 : -ETIMEDOUT;
    tse->tse_td = NULL;
    tse->tse_wchan = NULL;
    tse->tse_wakeus = 0;

    tseq_critical_enter(&tsevent_freeq);
    assert(tse_debug(tse, 0, 0));
    TAILQ_INSERT_HEAD(&tsevent_freeq.tseq_head, tse, tse_freeq);
    assert(tse_debug(tse, 0, 1));
    tseq_critical_exit(&tsevent_freeq);

    return r;
}


// 接口函数

/**
 * @note sleep 的包裹，会额外注册一个超时唤醒事件
 */
err_t tsleep(void *chan, mutex_t *mtx, const char *msg, u64 wakeus) {
    tsevent_t *tse = tse_alloc(cpu_this()->cpu_running, chan, wakeus);
    // 已经获取了 tsevent_usedq 的锁，释放 mtx 后再睡眠
    // 睡眠时先获取睡眠队列的锁，然后再释放 tsevent_usedq 的锁
    log(0, "%08x(%08x) TSLEEP(%s) UNTIL %lu->%lu\n", cpu_this()->cpu_running->td_tid, cpu_this()->cpu_running->td_proc->p_pid, msg, time_mono_us(), wakeus);
    if (mtx) {
        tseq_critical_exit(&tsevent_usedq);
    }
    sleep(chan, mtx ? mtx : &tsevent_usedq.tseq_lock, msg);
    if (mtx) {
        tseq_critical_enter(&tsevent_usedq);
    }
    err_t r = tse_free(tse);
    tseq_critical_exit(&tsevent_usedq);
    return r;
}

void twakeup(void *chan) {
    tseq_critical_enter(&tsevent_usedq);
    tsevent_t *tse;
	while (1) {
		int removed = 0;
		TAILQ_FOREACH(tse, &tsevent_usedq.tseq_head, tse_usedq) {
			if (tse->tse_wchan == chan) {
				tse_set_unused(tse, false);
				removed = 1;
			}
		}
		if (!removed) break;
	}
    wakeup(chan);
    tseq_critical_exit(&tsevent_usedq);
}

void tcleanup(thread_t *td) {
    tseq_critical_enter(&tsevent_usedq);
    tsevent_t *tse;
    while (1) {
        int removed = 0;
        TAILQ_FOREACH(tse, &tsevent_usedq.tseq_head, tse_usedq) {
            if (tse->tse_td == td) {
                tse_set_unused(tse, false);
                removed = 1;
            }
        }
        if (!removed) break;
    }
    tseq_critical_exit(&tsevent_usedq);
}

void tsleep_check() {
    tseq_critical_enter(&tsevent_usedq);
    tsevent_t *tse;
    u64 now = time_mono_us();
    while ((tse = TAILQ_FIRST(&tsevent_usedq.tseq_head))) {
        if (tse->tse_wakeus > now || tse->tse_wakeus == 0) {
            break;
        }
        void *chan = tse->tse_wchan;
        tse_set_unused(tse, true);
        wakeup(chan);
    }
    tseq_critical_exit(&tsevent_usedq);
}
