/*
 *  kianv.v - RISC-V rv32ima
 *
 *  copyright (c) 2025 hirosh dabui <hirosh@dabui.de>
 *
 *  permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  the software is provided "as is" and the author disclaims all warranties
 *  with regard to this software including all implied warranties of
 *  merchantability and fitness. in no event shall the author be liable for
 *  any special, direct, indirect, or consequential damages or any damages
 *  whatsoever resulting from loss of use, data or profits, whether in an
 *  action of contract, negligence or other tortious action, arising out of
 *  or in connection with the use or performance of this software.
 *
 */
#include "kianv_io_utils.h"

#include <stdint.h>

/* Base addresses (SoC specific) */
#define IO_BASE 0x10000000
#define UART_TX (volatile uint32_t*) (IO_BASE + 0x0000)
#define UART_LSR (volatile uint8_t*) (IO_BASE + 0x0005)
#define CPU_FREQ_REG (volatile uint32_t*) (IO_BASE + 0x0014)

/* ====================================================================
   Basic 64-bit division (safe for bootloader, no libgcc dependency)
   ==================================================================== */

static inline uint32_t clz32(uint32_t x) {
  if (!x) { return 32; }
  uint32_t n = 0;
  if (!(x & 0xFFFF0000)) {
    n += 16;
    x <<= 16;
  }
  if (!(x & 0xFF000000)) {
    n += 8;
    x <<= 8;
  }
  if (!(x & 0xF0000000)) {
    n += 4;
    x <<= 4;
  }
  if (!(x & 0xC0000000)) {
    n += 2;
    x <<= 2;
  }
  if (!(x & 0x80000000)) { n += 1; }
  return n;
}

/* Unsigned 64-bit divide */
uint64_t __udivdi3(uint64_t num, uint64_t den) {
  if (!den) { return 0xFFFFFFFFFFFFFFFFULL; }
  if (den > num) { return 0; }
  if (den == num) { return 1; }

  int shift = clz32(den >> 32 ? (uint32_t) (den >> 32) : (uint32_t) den);
  den <<= shift;
  uint64_t quot = 0;

  for (int i = 0; i <= shift; i++) {
    quot <<= 1;
    if (num >= den) {
      num -= den;
      quot |= 1;
    }
    den >>= 1;
  }
  return quot;
}

/* Unsigned 64-bit modulo */
uint64_t __umoddi3(uint64_t num, uint64_t den) {
  if (!den) { return num; }
  uint64_t q = __udivdi3(num, den);
  return num - (q * den);
}

/* ====================================================================
   Timing / delay routines
   ==================================================================== */

uint64_t read_cpu_cycles(void) {
  uint32_t hi, lo;
  asm volatile("rdcycleh %0" : "=r"(hi));
  asm volatile("rdcycle  %0" : "=r"(lo));
  return ((uint64_t) hi << 32) | lo;
}

uint32_t read_cpu_frequency(void) {
  uint32_t q = *CPU_FREQ_REG & 0xFFFFu; // Q8.8
  if (q == 0u) q = (20u << 8);          // 20 MHz Fallback (Q8.8)
  // Hz = round(q * 1e6 / 256)
  return (uint32_t) ((((uint64_t) q * 1000000u) + 128u) >> 8);
}

uint32_t set_cpu_frequency(uint32_t freq_hz) {
  if (freq_hz == 0u) freq_hz = 20000000u; // 20 MHz fallback
  // Q8.8 = round(freq_hz * 256 / 1e6), 16 Bit clamp
  uint32_t q = (uint32_t) ((((uint64_t) freq_hz * 256u) + 500000u) / 1000000u);
  if (q > 0xFFFFu) q = 0xFFFFu;
  *CPU_FREQ_REG = (*CPU_FREQ_REG & 0xFFFF0000u) | q; // low16 = Q8.8
  // Rückgabe: tatsächlich gesetzte Hz (aus Q8.8 zurückgerechnet)
  return (uint32_t) ((((uint64_t) q * 1000000u) + 128u) >> 8);
}

void delay_cycles(uint64_t cycles) {
  uint64_t start = read_cpu_cycles();
  while ((read_cpu_cycles() - start) < cycles);
}

void delay_microseconds(uint32_t us) {
  uint32_t freq = read_cpu_frequency();
  if (freq && us) { delay_cycles((uint64_t) us * (freq / 1000000ULL)); }
}

void delay_milliseconds(uint32_t ms) {
  uint32_t freq = read_cpu_frequency();
  if (freq && ms) { delay_cycles((uint64_t) ms * (freq / 1000ULL)); }
}

void delay_seconds(uint32_t sec) {
  uint32_t freq = read_cpu_frequency();
  if (freq && sec) { delay_cycles((uint64_t) sec * freq); }
}

/* Time conversions */
uint64_t elapsed_microseconds(void) {
  uint32_t freq = read_cpu_frequency();
  if (!freq) { return 0; }
  return read_cpu_cycles() / (freq / 1000000ULL);
}

uint64_t elapsed_milliseconds(void) {
  uint32_t freq = read_cpu_frequency();
  if (!freq) { return 0; }
  return read_cpu_cycles() / (freq / 1000ULL);
}

uint64_t elapsed_seconds(void) {
  uint32_t freq = read_cpu_frequency();
  if (!freq) { return 0; }
  return read_cpu_cycles() / freq;
}

/* ====================================================================
   UART output
   ==================================================================== */

void uart_putchar(char c) {
  while (!(*UART_LSR & 0x60)); // wait for TX empty
  *UART_TX = (c == '\r') ? '\n' : (uint32_t) c;
}

void uart_print_char(char ch) { uart_putchar(ch); }
