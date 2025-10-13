OUT = build
MESON ?= meson
NINJA ?= ninja

.PHONY: build
build: $(OUT)/build.ninja
	$(NINJA) -C $(OUT)

.PHONY: setup
setup: $(OUT)/build.ninja

$(OUT)/build.ninja:
	$(MESON) setup $(OUT)
