#include <libMain.h>
#include <unistd.h>

void libMain(long *p) {
	int argc = p[0];
	char **argv = (void *)(p + 1);
	int ret = main(argc, argv);
	exit(ret);
}
