#include <fs/buf.h>
#include <fs/cluster.h>
#include <fs/fat32.h>
#include <fs/fs.h>
#include <lib/error.h>
#include <lib/log.h>
#include <lib/queue.h>
#include <lib/string.h>

#define MAX_DIRENT 1024

// 待分配的dirent
static Dirent dirents[MAX_DIRENT];
struct DirentList direntFreeList = {NULL};

void direntInit() {
	for (int i = 0; i < MAX_DIRENT; i++) {
		LIST_INSERT_HEAD(&direntFreeList, &dirents[i], direntLink);
	}
}

Dirent *direntAlloc() {
	panic_on(LIST_EMPTY(&direntFreeList));
	Dirent *dirent = LIST_FIRST(&direntFreeList);
	LIST_REMOVE(dirent, direntLink);
	return dirent;
}

void direntDeAlloc(Dirent *dirent) {
	memset(dirent, 0, sizeof(Dirent));
	LIST_INSERT_HEAD(&direntFreeList, dirent, direntLink);
}
