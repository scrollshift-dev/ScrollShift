BUILD_DIR ?= build
BUILD_TYPE ?= Debug
JOBS ?= $(shell nproc 2>/dev/null || echo 2)

.PHONY: all configure build test install uninstall clean checkpoint

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DSCROLLSHIFT_WARNINGS_AS_ERRORS=ON

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: build
	cmake --install $(BUILD_DIR)
	/usr/local/bin/scrollshift service install
	@echo
	@echo "ScrollShift uses automatic mouse discovery by default; no device configuration is required."
	@echo "Check it with: sudo scrollshift service status"
	@echo "If an existing manual configuration kept the service stopped, run: sudo scrollshift service start"
	@echo "Use `scrollshift devices` and `scrollshift configure DEVICE` only for a manual override."

uninstall:
	@if [ -x /usr/local/bin/scrollshift ]; then /usr/local/bin/scrollshift service uninstall; fi
	rm -f /usr/local/bin/scrollshift
	@echo "Configuration in /etc/scrollshift is intentionally preserved."

clean:
	rm -rf $(BUILD_DIR)

checkpoint:
	rm -f ../ScrollShift-checkpoint.zip
	zip -qr ../ScrollShift-checkpoint.zip . \
		-x 'build/*' 'build-*/*' 'cmake-build-*/*' '.cache/*' 'dist/*' '*.swp' '*~'
