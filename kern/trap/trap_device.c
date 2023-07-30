#include <dev/plic.h>
#include <dev/timer.h>
#include <dev/interface.h>
#include <lib/log.h>
#include <proc/cpu.h>
#include <trap/trap.h>
#include <mm/memlayout.h>

void trap_device() {
	log(DEFAULT, "externel interrupt on CPU %d!\n", cpu_this_id());
	int irq = plicClaim();

	if (irq == VIRTIO0_IRQ) {
		// Note: call virtio intr handler
		log(DEFAULT, "[cpu %d] catch virtio intr\n", cpu_this_id());
		disk_intr();
	} else {
		log(DEFAULT, "[cpu %d] unknown externel interrupt irq = %d\n", cpu_this_id(), irq);
	}

	if (irq) {
		plicComplete(irq);
	}
}