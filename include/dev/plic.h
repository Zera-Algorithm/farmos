#ifndef _PLIC_H
#define _PLIC_H
void plicInit();
void plicInitHart();
int plicClaim();
void plicComplete(int irq);

#endif
