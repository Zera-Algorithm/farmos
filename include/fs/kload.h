#ifndef _KLOAD_H
#define _KLOAD_H

#include <types.h>

typedef struct thread thread_t;
typedef struct Dirent Dirent;

fileid_t file_load(const char *path, void **bin, size_t *size);
void file_unload(fileid_t fileid);
void *file_map(thread_t *proc, Dirent *file, u64 va, size_t len, int perm, int fileOffset);


#endif
