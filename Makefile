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
	@echo
	@echo "Next: sudo scrollshift configure /dev/input/eventN"
	@echo "Then: sudo scrollshift service enable"

uninstall:
	systemctl disable --now scrollshift.service 2>/dev/null || true
	rm -f /usr/local/bin/scrollshift /usr/local/lib/systemd/system/scrollshift.service
	systemctl daemon-reload 2>/dev/null || true
	@echo "Configuration in /etc/scrollshift is intentionally preserved."

clean:
	rm -rf $(BUILD_DIR)

checkpoint:
	rm -f ../ScrollShift-checkpoint.zip
	zip -qr ../ScrollShift-checkpoint.zip . \
		-x 'build/*' 'build-*/*' 'cmake-build-*/*' '.cache/*' 'dist/*' '*.swp' '*~'
