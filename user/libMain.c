#include <libMain.h>
#include <unistd.h>

void libMain() {
	int argc = 4;
	char *argv[] = {"test_argv", "arg1", "arg2", "arg3", NULL, "env1=1", NULL};
	char **envp = &argv[argc + 5];
	int ret = main(argc, argv, envp);
	exit(ret);
}
