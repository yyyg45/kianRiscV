#
# kianv.v - RISC-V rv32ima
#
# copyright (c) 2025 hirosh dabui <hirosh@dabui.de>
#
# permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# the software is provided "as is" and the author disclaims all warranties
# with regard to this software including all implied warranties of
# merchantability and fitness. in no event shall the author be liable for
# any special, direct, indirect, or consequential damages or any damages
# whatsoever resulting from loss of use, data or profits, whether in an
# action of contract, negligence or other tortious action, arising out of
# or in connection with the use or performance of this software.
#

# ---- project ----
PROJ := firmware

# Sources
SRCS := start.S custom_stdlib.c kianv_io_utils.c bootloader.c sd.c spi.c timer.c

# Objects & deps
OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
DEPS := $(OBJS:.o=.d)

# ---- toolchain ----
TOOLPREFIX ?= riscv32-buildroot-linux-gnu-
CC      := $(TOOLPREFIX)gcc
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump
SIZE    := $(TOOLPREFIX)size

# Find libgcc path automatically
LIBGCC := $(shell $(CC) -print-libgcc-file-name)

# ---- git metadata (safe outside a repo) ----
GIT_HASH  := $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo unknown)
GIT_DIRTY := $(shell git diff --quiet 2>/dev/null || echo -dirty)
GIT_DESC  := $(shell git describe --tags --dirty --always 2>/dev/null || echo $(GIT_HASH)$(GIT_DIRTY))

# ---- flags ----
CFLAGS  := -Wall -Wextra -Os -march=rv32ima_zicsr_zifencei -mabi=ilp32 \
           -ffunction-sections -fdata-sections --freestanding \
           -fno-stack-protector -fno-pic -fno-common -fno-builtin \
           -DGIT_HASH=\"$(GIT_HASH)$(GIT_DIRTY)\" -DGIT_DESC=\"$(GIT_DESC)\"
ASFLAGS := $(CFLAGS)

LDFLAGS := -T riscv.ld -static -nostdlib -L$(dir $(LIBGCC)) -lgcc \
           -Wl,--gc-sections -Wl,--no-dynamic-linker -Wl,-z,norelro -Wl,--disable-new-dtags

# ---- default ----
all: $(PROJ)

# ---- compile rules (with dep gen) ----
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

# ---- link ----
$(PROJ).elf: $(OBJS) riscv.ld
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@$(SIZE) $@

# ---- binary / hex ----
$(PROJ).bin: $(PROJ).elf
	$(OBJCOPY) $< -O binary $@

$(PROJ).hex: $(PROJ).bin
	xxd -e $< | cut -c 11-46 > $@

# Disassemble (usage: `make firmware.obj`)
%.obj: %.elf
	$(OBJDUMP) -d -S $< > $@

# Main target to generate hex and split image
$(PROJ): $(PROJ).hex
	./split_firmware.py

# ---- utility ----
print-version:
	@echo "GIT_HASH=$(GIT_HASH)$(GIT_DIRTY)"
	@echo "GIT_DESC=$(GIT_DESC)"

clean:
	rm -f $(OBJS) $(DEPS) $(PROJ).elf $(PROJ).bin $(PROJ).hex *.obj

.PHONY: all clean print-version $(PROJ)

# Include auto-generated dependencies
-include $(DEPS)
