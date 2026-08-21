BEATRICE_DEV_VERSION ?= ON

ifeq ($(OS),Windows_NT)
CMAKE_GENERATOR ?= Visual Studio 17 2022
CMAKE_CONFIGURE_ARGS := -A x64 -DSMTG_USE_STATIC_CRT=ON
else
CMAKE_GENERATOR ?= Xcode
CMAKE_CONFIGURE_ARGS := -DCMAKE_OSX_ARCHITECTURES=arm64 -DSMTG_BUILD_UNIVERSAL_BINARY=OFF
endif

ifneq ($(BEATRICE_PROCESSOR_UID),)
CMAKE_CONFIGURE_ARGS += -DBEATRICE_PROCESSOR_UID="$(BEATRICE_PROCESSOR_UID)"
endif
ifneq ($(BEATRICE_CONTROLLER_UID),)
CMAKE_CONFIGURE_ARGS += -DBEATRICE_CONTROLLER_UID="$(BEATRICE_CONTROLLER_UID)"
endif

all: release

# beatrice.lib is a separately authorized, non-public dependency. It must be
# placed locally before configuring; this Makefile intentionally never fetches it.

configure: CMakeLists.txt
	cmake . -G "$(CMAKE_GENERATOR)" -B build/vs3 -DBEATRICE_DEV_VERSION=$(BEATRICE_DEV_VERSION) $(CMAKE_CONFIGURE_ARGS)

debug: configure $(wildcard src/*/*)
	cmake --build build/vs3 --config Debug

release: configure $(wildcard src/*/*)
	cmake --build build/vs3 --config Release

distribution: configure
	cmake --build build/vs3 --config Release --target distribution

cpplint:
	cpplint --filter=-runtime/references,-build/header_guard,-readability/nolint --recursive src

clean:
	cmake -E rm -rf build

.PHONY: all configure debug release distribution cpplint clean
