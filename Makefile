# ═══════════════════════════════════════════════════════════════════════════
#  hyprland-monochrome — Native C++20 Build & Verification System
#
#  Zero external dependencies — compiles cleanly with any C++20 compiler
#  (`g++` or `clang++`). Binaries live in gitignored `bin/` and
#  `.config/hypr/bin/`.
#
#    make              compile all C++20 tools & runtime helpers
#    make docs         regenerate docs/KEYBINDS.md, windows/*, docs/preview.html
#    make wallpapers   regenerate 1080p wallpapers + animated GIF loops
#    make test         run the full verification suite (test/run-all.sh)
#    make clean        remove compiled binaries
# ═══════════════════════════════════════════════════════════════════════════

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -Werror -pthread
INCLUDES := -Iinclude

BIN_DIR := bin

BINS := \
	$(BIN_DIR)/mono-calc \
	$(BIN_DIR)/wallpaper-gen \
	$(BIN_DIR)/wallpaper-anim \
	$(BIN_DIR)/keybind-table \
	$(BIN_DIR)/build-windows-kit \
	$(BIN_DIR)/build-preview \
	$(BIN_DIR)/mono-util

.PHONY: all docs wallpapers test check clean

all: $(BINS)

$(BIN_DIR):
	@mkdir -p $@

$(BIN_DIR)/mono-calc: .config/hypr/src/mono-calc.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/wallpaper-gen: wallpapers/generate.cpp wallpapers/generate.hpp include/mono_image.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/wallpaper-anim: wallpapers/animate.cpp include/mono_image.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/keybind-table: tools/keybind-table.cpp include/keybind_table.hpp include/mono_config.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/build-windows-kit: tools/build-windows-kit.cpp include/mono_config.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/build-preview: tools/build-preview.cpp include/keybind_table.hpp include/mono_config.hpp include/mono_image.hpp wallpapers/generate.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

$(BIN_DIR)/mono-util: tools/mono-util.cpp include/mono_config.hpp include/mono_image.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

docs: $(BIN_DIR)/keybind-table $(BIN_DIR)/build-windows-kit $(BIN_DIR)/build-preview
	./$(BIN_DIR)/keybind-table
	./$(BIN_DIR)/build-windows-kit
	./$(BIN_DIR)/build-preview

wallpapers: $(BIN_DIR)/wallpaper-gen $(BIN_DIR)/wallpaper-anim
	./$(BIN_DIR)/wallpaper-gen --out-dir wallpapers
	./$(BIN_DIR)/wallpaper-anim --out-dir wallpapers

test check: all
	bash test/run-all.sh

clean:
	rm -rf $(BIN_DIR)
