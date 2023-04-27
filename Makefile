





clean:
	@echo "!!! Clean Unimplemented !!!"

.PHONY: check-style fix-style

check-style: clean
	@bash utils/check-style.sh

fix-style: clean
	@bash utils/check-style.sh -f
