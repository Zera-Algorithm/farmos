#include <lib/queue.h>
#include <lock/mutex.h>
#include <types.h>

typedef struct hashmap_entry {
	void *data;
	LIST_ENTRY(hashmap_entry) link;
} hashmap_entry_t;

typedef LIST_HEAD(, hashmap_entry) hashentry_head;

// 下面的数为质数
#define HASHMAP_CAPACITY 1013

typedef struct hashmap {
	hashentry_head hashtable[HASHMAP_CAPACITY];
	mutex_t lock;
	u64 (*hash)(void *key); // 对key的hash函数
	bool (*equal_to)(void *key, void *data); // 比较函数，比较key与存储的数据是否一致
	void (*free)(void *data); // 释放data的函数
} hashmap_t;

hashmap_t *hashmap_init(u64 (*hash)(void *), bool (*equal_to)(void *, void *), void (*free)(void *));
void hashmap_put(hashmap_t *hashmap, void *key, void *data);
void *hashmap_get(hashmap_t *hashmap, void *key);
void hashmap_free(hashmap_t *hashmap);
u64 hash_string(void *s);
void hashmap_foreach(hashmap_t *hashmap, void (*callback)(void *));
