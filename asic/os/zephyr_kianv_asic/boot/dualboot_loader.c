// SPDX-License-Identifier: Apache-2.0
/* Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de> */

#include <stdint.h>

#define IO_BASE 0x10000000u
#define UART_TX ((volatile uint32_t *)(IO_BASE + 0x0000u))
#define UART_RX ((volatile uint8_t *)(IO_BASE + 0x0000u))
#define UART_LSR ((volatile uint8_t *)(IO_BASE + 0x0005u))
#define UART_DIV ((volatile uint32_t *)(IO_BASE + 0x0010u))

#ifndef CPU_FREQ
#define CPU_FREQ 30000000u
#endif

#ifndef BAUDRATE
#define BAUDRATE 115200u
#endif

#ifndef IMAGE_FLASH_ADDR
#define IMAGE_FLASH_ADDR 0x20110000u
#endif

#ifndef IMAGE_RAM_ADDR
#define IMAGE_RAM_ADDR 0x80000000u
#endif

#ifndef IMAGE_SIZE
#define IMAGE_SIZE 0u
#endif

#ifndef MICROPY_FLASH_ADDR
#define MICROPY_FLASH_ADDR 0x20300000u
#endif

#define COPY_PROGRESS_WORDS (4096u / 4u)
#define UART_LSR_DR 0x01u

#ifndef PRE_COPY_DELAY
#define PRE_COPY_DELAY 10000u
#endif

static void putchar_raw(char c)
{
	while ((*UART_LSR & 0x60u) == 0u) {
	}
	*UART_TX = (uint32_t)(uint8_t)c;
}

static void print_str(const char *p)
{
	while (*p != '\0') {
		putchar_raw(*p++);
	}
}

static int poll_getchar(void)
{
	if ((*UART_LSR & UART_LSR_DR) == 0u) {
		return -1;
	}

	return (int)*UART_RX;
}

static void print_hex32(uint32_t val)
{
	print_str("0x");
	for (int shift = 28; shift >= 0; shift -= 4) {
		uint32_t nibble = (val >> shift) & 0xfu;

		putchar_raw((char)(nibble < 10u ? ('0' + nibble) : ('a' + nibble - 10u)));
	}
}

int main(void)
{
	volatile uint32_t *dst32 = (volatile uint32_t *)IMAGE_RAM_ADDR;
	const volatile uint32_t *src32 = (const volatile uint32_t *)IMAGE_FLASH_ADDR;
	volatile uint8_t *dst8 = (volatile uint8_t *)IMAGE_RAM_ADDR;
	const volatile uint8_t *src8 = (const volatile uint8_t *)IMAGE_FLASH_ADDR;
	uint32_t words = IMAGE_SIZE / 4u;
	uint32_t tail = IMAGE_SIZE & 3u;
	uint32_t checksum = 0u;

	*UART_DIV = ((CPU_FREQ / 10000u) << 16) | (CPU_FREQ / BAUDRATE);

	print_str("\nKianV RISC-V uLinux ASIC Tiny Tapeout SOC\n");
	print_str("bootloader\n");
	for (volatile uint32_t delay = 0; delay < PRE_COPY_DELAY; delay++) {
	}
	for (;;) {
		print_str("z: Zephyr  m: MicroPython\n");
		for (volatile uint32_t wait = 0; wait < 10000u; wait++) {
			int ch = poll_getchar();

			if ((ch == 'z') || (ch == 'Z')) {
				goto boot_zephyr;
			}
			if ((ch == 'm') || (ch == 'M')) {
				goto boot_micropython;
			}
		}
	}

boot_micropython:
	print_str("Boot key received\n");
	print_str("MicroPython loader\n");
	print_str("jump ");
	print_hex32(MICROPY_FLASH_ADDR);
	print_str("\n");

	__asm__ volatile("fence.i" ::: "memory");
	((void (*)(void))MICROPY_FLASH_ADDR)();

boot_zephyr:
	print_str("Boot key received\n");
	print_str("KianV Zephyr loader\n");
	print_str("flash=");
	print_hex32(IMAGE_FLASH_ADDR);
	print_str(" ram=");
	print_hex32(IMAGE_RAM_ADDR);
	print_str(" size=");
	print_hex32(IMAGE_SIZE);
	print_str("\ncopy");

	for (uint32_t i = 0; i < words; i++) {
		uint32_t word = src32[i];

		dst32[i] = word;
		checksum ^= word;
		if ((i & (COPY_PROGRESS_WORDS - 1u)) == 0u) {
			putchar_raw('.');
		}
	}

	for (uint32_t i = 0; i < tail; i++) {
		uint32_t offset = words * 4u + i;

		dst8[offset] = src8[offset];
		checksum ^= ((uint32_t)src8[offset]) << (i * 8u);
	}

	print_str("\nchecksum=");
	print_hex32(checksum);
	print_str("\njump ");
	print_hex32(IMAGE_RAM_ADDR);
	print_str("\n");

	__asm__ volatile("fence.i" ::: "memory");
	((void (*)(void))IMAGE_RAM_ADDR)();

	for (;;) {
	}
}
