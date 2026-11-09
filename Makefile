# ═══════════════════════════════════════════════════════════════════════════
#  hyprland-monochrome — Native Pure C11 Build & Verification System
#
#  Zero external dependencies — compiles cleanly with any ISO C11 compiler
#  (`cc`, `gcc`, or `clang`) plus POSIX threads and libm. Binaries live in
#  gitignored `bin/` and `.config/hypr/bin/`.
#
#    make              compile all Pure C11 tools & runtime helpers
#    make docs         regenerate docs/KEYBINDS.md, windows/*, docs/preview.html
#    make wallpapers   regenerate 1080p live animated GIF89a wallpapers
#    make test         run the full verification suite (test/run-all.sh)
#    make clean        remove compiled binaries
# ═══════════════════════════════════════════════════════════════════════════

CC       ?= cc
CFLAGS   ?= -std=c11 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -O3 -Wall -Wextra -Werror -pthread
INCLUDES := -Iinclude
LDLIBS   := -lm

BIN_DIR := bin

BINS := \
	$(BIN_DIR)/mono-calc \
	$(BIN_DIR)/mono-ipc \
	$(BIN_DIR)/mono-tui \
	$(BIN_DIR)/cursor-gen \
	$(BIN_DIR)/wallpaper-gen \
	$(BIN_DIR)/wallpaper-anim \
	$(BIN_DIR)/keybind-table \
	$(BIN_DIR)/build-windows-kit \
	$(BIN_DIR)/build-preview \
	$(BIN_DIR)/mono-util \
	$(BIN_DIR)/serve-site \
	$(BIN_DIR)/trailer-studio

.PHONY: all docs cursors site serve wallpapers test check clean

all: $(BINS)

$(BIN_DIR):
	@mkdir -p $@

$(BIN_DIR)/mono-calc: .config/hypr/src/mono-calc.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN_DIR)/mono-ipc: .config/hypr/src/mono-ipc.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN_DIR)/mono-tui: .config/hypr/src/mono-tui.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN_DIR)/cursor-gen: .config/hypr/src/mono-cursor-gen.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BIN_DIR)/wallpaper-gen: wallpapers/generate.c wallpapers/generate.h include/mono_image.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/wallpaper-anim: wallpapers/animate.c wallpapers/generate.h include/mono_image.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/keybind-table: tools/keybind-table.c include/keybind_table.h include/mono_config.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/build-windows-kit: tools/build-windows-kit.c include/mono_config.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/build-preview: tools/build-preview.c include/keybind_table.h include/mono_config.h include/mono_image.h wallpapers/generate.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/mono-util: tools/mono-util.c include/mono_config.h include/mono_image.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDLIBS)

$(BIN_DIR)/serve-site: tools/serve-site.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR)/trailer-studio: tools/trailer-studio.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

cursors: $(BIN_DIR)/cursor-gen
	./$(BIN_DIR)/cursor-gen --out-dir cursors

docs: $(BIN_DIR)/keybind-table $(BIN_DIR)/build-windows-kit $(BIN_DIR)/build-preview $(BIN_DIR)/mono-util cursors
	./$(BIN_DIR)/keybind-table
	./$(BIN_DIR)/build-windows-kit
	@mkdir -p site
	./$(BIN_DIR)/mono-util keybinds-json docs/KEYBINDS.md site/keybinds.json
	./$(BIN_DIR)/build-preview

site: docs $(BIN_DIR)/serve-site

serve: $(BIN_DIR)/serve-site
	@chmod +x $(BIN_DIR)/serve-site
	./$(BIN_DIR)/serve-site --port 8080

wallpapers: $(BIN_DIR)/wallpaper-anim
	./$(BIN_DIR)/wallpaper-anim --out-dir wallpapers

test check: all
	bash test/run-all.sh

clean:
	rm -rf $(BIN_DIR)
