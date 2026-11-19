AS      := as
CC      := gcc
LD      := ld
GRUB    := grub-mkrescue

CFLAGS  := -m32 -std=gnu11 -O2 -Wall -Wextra -ffreestanding -fno-builtin \
           -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
           -fno-unwind-tables -mno-sse -mno-mmx -msoft-float
ASFLAGS := --32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
ISO_DIR := $(BUILD)/iso

.PHONY: all clean check
all: minios.iso

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.s | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/kernel.o: kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/minios.elf: $(BUILD)/boot.o $(BUILD)/kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/boot.o $(BUILD)/kernel.o

$(ISO_DIR)/boot/grub:
	mkdir -p $@

$(ISO_DIR)/boot/minios.elf: $(BUILD)/minios.elf | $(ISO_DIR)/boot/grub
	cp $< $@

$(ISO_DIR)/boot/grub/grub.cfg: grub.cfg | $(ISO_DIR)/boot/grub
	cp $< $@

minios.iso: $(ISO_DIR)/boot/minios.elf $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB) -o $@ $(ISO_DIR)
	test -s $@

check: $(BUILD)/minios.elf
	@readelf -h $(BUILD)/minios.elf | grep -E 'Class:|Machine:|Entry point'
	@objdump -h $(BUILD)/minios.elf | grep -E '\.multiboot2|\.text|\.bss'

clean:
	rm -rf $(BUILD) minios.iso
