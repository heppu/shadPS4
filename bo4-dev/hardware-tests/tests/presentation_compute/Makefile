TEST := presentation_compute
ROOT := $(abspath ../..)
TOOLCHAIN ?= $(ROOT)/.deps/openorbis
OPENGNM ?= $(ROOT)/.deps/opengnm
BUILD := $(ROOT)/build/$(TEST)
OUT := $(ROOT)/out/$(TEST)
OBJ := $(BUILD)/main.o

CC := clang
LD := ld.lld
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -g --target=x86_64-ps4-elf -fPIC \
	-isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
	-I$(OPENGNM)/include -I../common -I$(BUILD)

.PHONY: all check-deps
all: check-deps $(OUT)/$(TEST).elf

check-deps:
	@test -f "$(TOOLCHAIN)/link.x"
	@test -f "$(OPENGNM)/libopengnm.a"

$(BUILD) $(OUT):
	@mkdir -p "$@"

$(BUILD)/copy.o: assets/copy.s | $(BUILD)
	llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx700 -filetype=obj "$<" -o "$@"

$(BUILD)/copy.bin: $(BUILD)/copy.o
	llvm-objcopy --only-section=.text -O binary "$<" "$@"

$(BUILD)/copy.h: $(BUILD)/copy.bin gen_shader.py
	python3 gen_shader.py "$<" "$@"

$(OBJ): src/main.c $(BUILD)/copy.h ../common/shadtest_guest.h ../common/shadtest_pm4.h
	$(CC) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/$(TEST).raw.elf: $(OBJ) $(OPENGNM)/libopengnm.a
	$(LD) -o "$@" "$<" -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x \
		--eh-frame-hdr -L$(TOOLCHAIN)/lib $(OPENGNM)/libopengnm.a \
		-lc -lkernel -lSceGnmDriver -lSceVideoOut \
		$(TOOLCHAIN)/lib/crt1.o $(TOOLCHAIN)/lib/crti.o $(TOOLCHAIN)/lib/crtn.o

$(OUT)/$(TEST).elf: $(BUILD)/$(TEST).raw.elf | $(OUT)
	OO_PS4_TOOLCHAIN="$(TOOLCHAIN)" $(TOOLCHAIN)/bin/linux/create-fself \
		-in="$<" -out="$(BUILD)/$(TEST).oelf" --eboot "$@" --paid 0x3800000000000011
