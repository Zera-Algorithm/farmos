modules := $(shell ls -d */)

.PHONY: build clean $(modules)

build: $(modules)

# 这个modules必须.PHONY，因为必须要触发子模块的make动作
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
