ROOT_DIR := $(abspath $(CURDIR)/..)
BUILD_DIR := $(CURDIR)/build
SIMD_MODE ?= sse42
SUPPORTED_SIMD_MODES := scalar sse42 avx128 avx256
TARGET_SUFFIX := $(subst -,_,$(SIMD_MODE))
TARGET := eigen_f32_multi_thread_$(TARGET_SUFFIX)
TARGET_PATH := $(BUILD_DIR)/eigen-f32-multi-thread/$(TARGET)
THREADS ?= 1

.PHONY: all configure build run suite clean

ifeq ($(filter $(SIMD_MODE),$(SUPPORTED_SIMD_MODES)),)
$(error unsupported SIMD_MODE '$(SIMD_MODE)'; supported values: $(SUPPORTED_SIMD_MODES))
endif

all: run

configure:
	cmake -S $(ROOT_DIR) -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build $(BUILD_DIR) --target $(TARGET)

run: build
	$(TARGET_PATH) -j $(THREADS)

suite: build
	./run-benchmark-suite.sh

clean:
	rm -rf $(BUILD_DIR)
