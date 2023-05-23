#include <libMain.h>
#include <unistd.h>

void libMain(int argc, char **argv) {
	int ret = main(argc, argv);
	exit(ret);
}
