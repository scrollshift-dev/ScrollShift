BUILD_DIR ?= build
BUILD_TYPE ?= Debug
JOBS ?= $(shell nproc 2>/dev/null || echo 2)

.PHONY: all configure build test install uninstall clean checkpoint

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DSMOOTHWHEEL_WARNINGS_AS_ERRORS=ON

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: build
	cmake --install $(BUILD_DIR)
	@echo
	@echo "Next: sudo smoothwheel configure /dev/input/eventN"
	@echo "Then: sudo smoothwheel service enable"

uninstall:
	systemctl disable --now smoothwheel.service 2>/dev/null || true
	rm -f /usr/local/bin/smoothwheel /usr/local/lib/systemd/system/smoothwheel.service
	systemctl daemon-reload 2>/dev/null || true
	@echo "Configuration in /etc/smoothwheel is intentionally preserved."

clean:
	rm -rf $(BUILD_DIR)

checkpoint:
	rm -f ../SmoothWheel-checkpoint.zip
	zip -qr ../SmoothWheel-checkpoint.zip . \
		-x 'build/*' 'build-*/*' 'cmake-build-*/*' '.cache/*' 'dist/*' '*.swp' '*~'
