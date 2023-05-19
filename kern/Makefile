modules := $(shell ls -d */)

.PHONY: build clean $(modules)

build: $(modules)

$(modules):
	$(MAKE) build --directory=$@

clean:
	for d in $(modules); \
		do \
			$(MAKE) clean --directory=$$d; \
		done
	rm *.asm
	rm kernel

include ../include.mk