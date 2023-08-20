#include <fs/chardev.h>
#include <fs/initcall.h>
#include <lib/log.h>
#include <lib/printf.h>

#define INTR_MAXNO 136
u64 interrupts_count[INTR_MAXNO+1];

void interrupts_read(chardev_data_t *data) {
	data->str[0] = 0;
	char *str = data->str;
	for (int i = 0; i <= INTR_MAXNO; i++) {
		sprintf(str, "%d: %d\n", i, interrupts_count[i]);
		while (*str != 0) str++;
	}
	data->size = str - data->str;
}

static int proc_interrupts_init(void) {
	log(LEVEL_GLOBAL, "proc_interrupts_init started!\n");
	create_chardev_file("/proc/interrupts", "5:        0\n8:        0\n10:        0", interrupts_read, NULL);
	return 0;
}

fs_initcall(proc_interrupts_init);
