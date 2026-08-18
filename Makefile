BUILD_DIR ?= build
BUILD_TYPE ?= Debug
JOBS ?= $(shell nproc 2>/dev/null || echo 2)

.PHONY: all configure build test clean checkpoint

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DSMOOTHWHEEL_WARNINGS_AS_ERRORS=ON

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

checkpoint:
	rm -f ../SmoothWheel-checkpoint.zip
	zip -qr ../SmoothWheel-checkpoint.zip . \
		-x 'build/*' 'build-*/*' 'cmake-build-*/*' '.cache/*' 'dist/*' '*.swp' '*~'
