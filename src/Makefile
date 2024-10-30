# @Students, do not modify this Makefile (unless you know what you are doing).
# This is just a wrapper over CMake commands, and does not do the actual building.
# To add more source files, modify CMakeLists.txt instead.
# If you just added include files, you do not need to modify anything.
# Any modifications to this file will be ignored by Autolab.

ifeq ($(DEBUG),1)
BUILD_TYPE := Debug
else
BUILD_TYPE := RelWDebInfo
endif

.PHONY: all examples build-all build-cloudfs build-snapshot
# For checkpoints 1 and 2, may remove build-snapshot
all: build-cloudfs build-snapshot
examples: build-all

# Order-only prerequisite for build/ directory. This is a fancy
# GNU CMake feature to ensure that we create the build directory
# if it does not exist, but pay no attention to its mtime.
configure: | build
build:
	mkdir build; cd build; cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..;

build-all: configure
	cd build; make all;

build-cloudfs: configure
	@echo "---------------------------------------------------"
	@echo "Building CloudFS using -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..."
	@echo "To build examples, run 'make examples'"
	@echo "---------------------------------------------------"

	cd build; make cloudfs;

build-snapshot: build-all
	cp build/snapshot/snapshot scripts

.PHONY: test_1_% test_2_% test_3_%

# Even though we switch the build target to DEBUG and vice versa,
# The test scripts currently always choose the release build
test_1_%: build-cloudfs
	./tests/checkpoint_1/test_1_$*/test_1_$*.sh

test_2_%: build-cloudfs
	./tests/checkpoint_2/test_2_$*/test_2_$*.sh

test_3_%: build-cloudfs
	./tests/checkpoint_3/test_3_$*/test_3_$*.sh

clean:
	rm -rf build
	rm scripts/snapshot
