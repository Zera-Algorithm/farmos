#include <lib/profiling.h>
#include <lib/hashmap.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <mm/kmalloc.h>
#include <dev/timer.h>
#include <lock/mutex.h>

static hashmap_t *hashmap;

typedef struct profiling {
	char item[128];
	u64 total_runtime_us;
	u64 run_count;
	mutex_t lock;
} profiling_t;

static bool equal_to(void *key, void *profiling) {
	return (strncmp((const char *)key, ((profiling_t *)profiling)->item, 128) == 0);
}

void profiling_init() {
	hashmap = hashmap_init(hash_string, equal_to, kfree);
}

void profiling_end(const char *file, const char *func, u64 begin) {
	u64 end = time_rtc_us();
	if (hashmap == NULL) {
		// 此时hashmap未初始化好，直接跳过
		return;
	}
	u64 time = end - begin;
	profiling_end_with_time(file, func, time);
}

void profiling_end_with_time(const char *file, const char *func, u64 time) {
	profiling_t *profiling = hashmap_get(hashmap, (void *)func);
	if (profiling == NULL) {
		profiling = kmalloc(sizeof(profiling_t));
		strncpy(profiling->item, func, 128);
		profiling->total_runtime_us = 0;
		profiling->run_count = 0;
		mtx_init(&profiling->lock, "profiling", false, MTX_SPIN);
		hashmap_put(hashmap, (void *)func, profiling);
	}
	mtx_lock(&profiling->lock);
	profiling->total_runtime_us += time;
	profiling->run_count++;
	mtx_unlock(&profiling->lock);
}

static void print_profiling(void *data) {
	profiling_t *profiling = data;
	printf("%25s %12llu %8llu %10llu\n", profiling->item, profiling->total_runtime_us, profiling->run_count, profiling->total_runtime_us / profiling->run_count);
}

void profiling_report() {
	printf("\n============================ PROFILING RESULTS =================================\n");
	printf("%25s %12s %8s %10s\n", "ITEM", "TOTAL/US", "TIMES", "PERCALL/US");
	hashmap_foreach(hashmap, print_profiling);
}
