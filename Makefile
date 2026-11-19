CLANG ?= clang
PYTHON ?= python3
NODE ?= node
WASM_LD ?= $(shell command -v wasm-ld || command -v wasm-ld-21 || find "$$(rustc --print sysroot 2>/dev/null)/lib/rustlib" -name rust-lld -print -quit 2>/dev/null)
WASM_LINK = $(WASM_LD) $(if $(findstring rust-lld,$(WASM_LD)),-flavor wasm)
EXPORTS = init init_birdless configure reset_training set_training start_firing frame paint status mode command graph_scroll pointer model_buffer model_capacity model_load export_best export_leader
OBJECTS = $(patsubst wasm/%.c,build/%.o,$(wildcard wasm/*.c))
PORT ?= 8000

.PHONY: all assets serve test
all: dist/dino.wasm assets

build/%.o: wasm/%.c $(wildcard wasm/*.h)
	@mkdir -p build
	$(CLANG) --target=wasm32-unknown-unknown -std=c11 -O3 -ffreestanding -Wall -Wextra -Werror -c $< -o $@

dist/dino.wasm: $(OBJECTS) Makefile
	@mkdir -p dist
	@test -n "$(WASM_LD)" || (echo "Instale wasm-ld (LLVM LLD) ou informe WASM_LD=/caminho/wasm-ld." >&2; exit 1)
	$(WASM_LINK) --no-entry --export-memory -z stack-size=131072 --strip-all $(EXPORTS:%=--export=%) $(OBJECTS) -o $@

assets:
	$(PYTHON) scripts/assets.py

serve: all
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory dist

build/physics-test: tests/physics.c wasm/game.c wasm/network.c $(wildcard wasm/*.h)
	@mkdir -p build
	$(CLANG) -std=c11 -O2 -fno-builtin -Wno-unknown-attributes tests/physics.c wasm/network.c -lm -o $@

test: all build/physics-test
	./build/physics-test
	$(NODE) --test tests/*.test.mjs
