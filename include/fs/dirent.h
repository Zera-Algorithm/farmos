#ifndef _DIRENT_H
#define _DIRENT_H

typedef struct Dirent Dirent;

void direntInit();
Dirent *dirent_alloc();
void dirent_dealloc(Dirent *dirent);

#endif
