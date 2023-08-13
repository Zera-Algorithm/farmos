#include <lib/queue.h>
#include <types.h>
#include <lib/hashmap.h>
#include <mm/kmalloc.h>
#include <lock/mutex.h>

hashmap_t *hashmap_init(u64 (*hash)(void *), bool (*equal_to)(void *, void *), void (*free)(void *)) {
	hashmap_t *hashmap = kmalloc(sizeof(hashmap_t));
	hashmap->hash = hash;
	hashmap->equal_to = equal_to;
	hashmap->free = free;
	mtx_init(&hashmap->lock, "hashmap", false, MTX_SPIN);
	return hashmap;
}

/**
 * @brief 放入一个hash表项，其中key为表项的键，data同时包括Key-Value
 */
void hashmap_put(hashmap_t *hashmap, void *key, void *data) {
	mtx_lock(&hashmap->lock);
	u64 hash = hashmap->hash(key) % HASHMAP_CAPACITY;
	hashmap_entry_t *entry = kmalloc(sizeof(hashmap_entry_t));
	entry->data = data;
	LIST_INSERT_HEAD(&hashmap->hashtable[hash], entry, link);
	mtx_unlock(&hashmap->lock);
}

/**
 * @brief 通过key获得一个hash表项，如果找到，返回data的指针，否则返回NULL
 */
void *hashmap_get(hashmap_t *hashmap, void *key) {
	mtx_lock(&hashmap->lock);
	u64 hash = hashmap->hash(key) % HASHMAP_CAPACITY;
	hashentry_head *head = &hashmap->hashtable[hash];
	hashmap_entry_t *entry;
	LIST_FOREACH(entry, head, link) {
		if (hashmap->equal_to(key, entry->data)) {
			void *ret = entry->data;
			mtx_unlock(&hashmap->lock);
			return ret;
		}
	}
	mtx_unlock(&hashmap->lock);
	return NULL;
}

// 要求callback不会加除了pr_lock之外的锁
void hashmap_foreach(hashmap_t *hashmap, void (*callback)(void *)) {
	mtx_lock(&hashmap->lock);
	for (int i = 0; i < HASHMAP_CAPACITY; i++) {
		hashentry_head *head = &hashmap->hashtable[i];
		hashmap_entry_t *entry;
		LIST_FOREACH(entry, head, link) {
			callback(entry->data);
		}
	}
	mtx_unlock(&hashmap->lock);
}

void hashmap_free(hashmap_t *hashmap) {
	for (int i = 0; i < HASHMAP_CAPACITY; i++) {
		hashentry_head *head = &hashmap->hashtable[i];
		hashmap_entry_t *entry;
		LIST_UNTIL_EMPTY(entry, head) {
			hashmap->free(entry->data);
			LIST_REMOVE(entry, link);
			kfree(entry);
		}
	}
	kfree(hashmap);
}

u64 hash_string(void *s) {
	#define MAX_STR_USAGE 100
	char *str = s;
	u64 prime = 9973;
	u64 value = 0;
	for (int i = 0; str[i] && i < MAX_STR_USAGE; i++) {
		value = (value * 128 + (u64)str[i]) % prime;
	}
	return value;
}


