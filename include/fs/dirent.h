#ifndef _DIRENT_H
#define _DIRENT_H

typedef struct Dirent Dirent;

void direntInit();
Dirent *direntAlloc();
void direntDeAlloc(Dirent *dirent);

#endif
