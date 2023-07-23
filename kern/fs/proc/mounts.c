#include <fs/chardev.h>
#include <fs/initcall.h>
#include <lib/log.h>

static int proc_mounts_init(void) {
	log(LEVEL_GLOBAL, "proc_mounts_init started!\n");
	create_chardev_file("/proc/mounts", "mounts", NULL, NULL);
	return 0;
}

fs_initcall(proc_mounts_init);
