#include <fs/chardev.h>
#include <fs/initcall.h>
#include <lib/log.h>

static int proc_meminfo_init(void) {
	log(LEVEL_GLOBAL, "proc_meminfo_init started!\n");
	create_chardev_file("/proc/meminfo", "meminfo", NULL, NULL);
	return 0;
}

fs_initcall(proc_meminfo_init);
