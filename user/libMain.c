#include <libMain.h>
#include <syscallLib.h>

void libMain(int argc, char **argv) {
	int ret = main(argc, argv);
	exit(ret);
}
