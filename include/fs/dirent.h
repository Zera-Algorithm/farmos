#ifndef _DIRENT_H
#define _DIRENT_H

typedef struct Dirent Dirent;

void dirent_init();
Dirent *dirent_alloc();
void dirent_dealloc(Dirent *dirent);

#endif
