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
#include "custom_stdlib.h"
#include "kianv_io_utils.h"
#include "sd.h"

#include <stddef.h>
#include <stdint.h>

/* ==== SoC address map (keep in sync with RTL) ==== */
#define DIV_ADDR0 0x1000000Cu            /* HI: SPI0 div, LO: UART div */
#define DIV_ADDR1 0x10000010u            /* HI: SPI1 div, LO: CLINT div */
#define CPU_FREQ_REG_ADDR 0x10000014u    /* CPU frequency (RW) */
#define CPU_MEMSIZE_REG_ADDR 0x10000018u /* SDRAM size (RO) */

#define REG32(a) (*(volatile uint32_t*) (uintptr_t) (a))

/* ==== SDRAM controller MMIO ==== */
#define SDRAM_CFG_BASE 0x10600000u
#define SDRAM_CFG_REG(n) (SDRAM_CFG_BASE + ((uint32_t) (n) << 2))

enum {
  SDRAM_TIM_TRP_CYC = 0x00,
  SDRAM_TIM_TRCD_CYC = 0x01,
  SDRAM_TIM_TMRD_CYC = 0x02,
  SDRAM_TIM_TRFC_CYC = 0x03,
  SDRAM_TIM_TWR_CYC = 0x04,
  SDRAM_TIM_TDAL_CYC = 0x05,
  SDRAM_TIM_TREFI_CYC = 0x06,
  SDRAM_CAP_CAS_LAT = 0x07,
  SDRAM_POL_KEEP_OPEN = 0x08,
  SDRAM_CAP_READ_NEGEDGE = 0x09,
  SDRAM_CAP_READ_EXTRA_CYC = 0x0A,
  SDRAM_REF_CREDITS_MAX = 0x0B,
  SDRAM_REF_FORCE_THRESH = 0x0C,
  SDRAM_REF_SOFT_THRESH = 0x0D,
  SDRAM_CTL_CTRL = 0x10,
  SDRAM_STAT_STATUS = 0x11
};
#define SDRAM_CTL_MRS (1u << 0)
#define SDRAM_CTL_REINIT (1u << 1)
#define SDRAM_STAT_INIT (1u << 0)

static inline void data_fence(void) { __asm__ volatile("fence rw, rw" ::: "memory"); }

/* ==== tiny MMIO helpers ==== */
static inline void sdram_cfg_w(uint32_t idx, uint32_t v) {
  *(volatile uint32_t*) SDRAM_CFG_REG(idx) = v;
}
static inline uint32_t sdram_cfg_r(uint32_t idx) {
  return *(volatile uint32_t*) SDRAM_CFG_REG(idx);
}

/* ==== utils ==== */
static inline uint16_t clamp16(uint32_t v) {
  if (v < 1u) { return 1u; }
  if (v > 0xFFFFu) { return 0xFFFFu; }
  return (uint16_t) v;
}
static inline uint16_t div_from_hz(uint32_t fclk, uint32_t fout) {
  if (!fout) { return 1u; }
  return clamp16((fclk + (fout / 2u)) / fout);
}
static inline uint32_t pack_div(uint16_t hi, uint16_t lo) { return ((uint32_t) hi << 16) | lo; }
static inline uint32_t ns_to_cycles(uint32_t ns, uint32_t f_hz) {
  uint32_t f_mhz = f_hz / 1000000u;
  uint64_t num = (uint64_t) ns * (uint64_t) f_mhz + 999u;
  uint32_t cyc = (uint32_t) (num / 1000u);
  return cyc ? cyc : 1u;
}

/* ==== public: program dividers ==== */
// Helper: compute a 16-bit divider from fclk and target frequency (Hz),
// rounded to nearest, clamped to [1..0xFFFF].
static inline uint16_t div_from_hz16(uint32_t fclk_hz, uint32_t target_hz) {
  if (target_hz == 0u) return 1u;
  uint32_t d = (fclk_hz + (target_hz / 2u)) / target_hz; // round
  if (d < 1u) d = 1u;
  if (d > 0xFFFFu) d = 0xFFFFu;
  return (uint16_t) d;
}

// Program divider CSRs:
//  - DIV_ADDR0: [31:16] = SPI0 divider, [15:0] = UART divider
//  - DIV_ADDR1: [31:16] = SPI1 divider, [15:0] = CLINT us-per-tick (NOT a freq
//  divider)
//    Pass clint_us_per_tick = 1 for 1 MHz mtime (1 µs per tick).
void soc_program_dividers(uintptr_t div_addr0,
                          uintptr_t div_addr1,
                          uint32_t uart_baud_hz,
                          uint32_t spi0_sclk_hz,
                          uint32_t spi1_sclk_hz,
                          uint32_t clint_us_per_tick) {
  const uint32_t fclk = read_cpu_frequency(); // returns Hz (has Q8.8 handling inside)

  const uint16_t uart_div = div_from_hz16(fclk, uart_baud_hz);
  const uint16_t spi0_div = div_from_hz16(fclk, spi0_sclk_hz);
  const uint16_t spi1_div = div_from_hz16(fclk, spi1_sclk_hz);

  // CLINT field is microseconds per mtime tick (>=1), NOT a divider from
  // fclk.
  uint16_t clint_div = (clint_us_per_tick < 1u)
                           ? 1u
                           : (clint_us_per_tick > 0xFFFFu ? 0xFFFFu : (uint16_t) clint_us_per_tick);

  // Write packed 32-bit words
  REG32(div_addr0) = ((uint32_t) spi0_div << 16) | (uint32_t) uart_div;
  REG32(div_addr1) = ((uint32_t) spi1_div << 16) | (uint32_t) clint_div;
}

/* ==== SDRAM init ==== */
static int sdram_wait_done(uint32_t max_poll) {
  while (max_poll--)
    if (sdram_cfg_r(SDRAM_STAT_STATUS) & SDRAM_STAT_INIT) { return 0; }
  return -1;
}

int sdram_controller_init(
    uint32_t trefi_ns, uint32_t trp_ns, uint32_t trcd_ns, uint32_t twr_ns, uint32_t cas) {
  uint32_t f_hz = read_cpu_frequency();

  uint32_t trp_cyc = ns_to_cycles(trp_ns, f_hz);
  uint32_t trcd_cyc = ns_to_cycles(trcd_ns, f_hz);
  uint32_t twr_cyc = ns_to_cycles(twr_ns, f_hz);
  uint32_t trefi_cyc = ns_to_cycles(trefi_ns, f_hz);
  uint32_t tdal_cyc = twr_cyc + trp_cyc;
  uint32_t tmrd_cyc = ns_to_cycles(2u, f_hz);
  if (tmrd_cyc < 2u) { tmrd_cyc = 2u; }

  sdram_cfg_w(SDRAM_TIM_TRP_CYC, trp_cyc);
  sdram_cfg_w(SDRAM_TIM_TRCD_CYC, trcd_cyc);
  sdram_cfg_w(SDRAM_TIM_TMRD_CYC, tmrd_cyc);
  sdram_cfg_w(SDRAM_TIM_TRFC_CYC, ns_to_cycles(66u, f_hz));
  sdram_cfg_w(SDRAM_TIM_TWR_CYC, twr_cyc);
  sdram_cfg_w(SDRAM_TIM_TDAL_CYC, tdal_cyc);
  sdram_cfg_w(SDRAM_TIM_TREFI_CYC, trefi_cyc);
  sdram_cfg_w(SDRAM_CAP_CAS_LAT, cas & 7u);
  sdram_cfg_w(SDRAM_POL_KEEP_OPEN, 1u);
  sdram_cfg_w(SDRAM_CAP_READ_NEGEDGE, 1u);
  sdram_cfg_w(SDRAM_CAP_READ_EXTRA_CYC, 0u);

  data_fence();
  sdram_cfg_w(SDRAM_CTL_CTRL, SDRAM_CTL_MRS);

  if (sdram_wait_done(1000000u)) {
    printf(COLOR_RED "[SDRAM] init_done timeout\n" COLOR_RESET);
    return -1;
  }

  printf(COLOR_GREEN "[SDRAM] MRS OK (CAS=%u, tRP=%u/%u, tRCD=%u/%u, "
                     "tWR=%u/%u, tREFI=%u)\n" COLOR_RESET,
         (unsigned) cas,
         (unsigned) trp_ns,
         (unsigned) trp_cyc,
         (unsigned) trcd_ns,
         (unsigned) trcd_cyc,
         (unsigned) twr_ns,
         (unsigned) twr_cyc,
         (unsigned) trefi_ns);
  return 0;
}

int sdram_init_default(void) {
  const uint32_t TREFI_NS = 7800u, TRP_NS = 15u, TRCD_NS = 15u, TWR_NS = 15u, CAS = 2u;
  return sdram_controller_init(TREFI_NS, TRP_NS, TRCD_NS, TWR_NS, CAS);
}

void sdram_cfg_dump(void) {
  printf("[SDRAM] CFG dump:\n");
  printf("  TRP_CYC   = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TRP_CYC));
  printf("  TRCD_CYC  = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TRCD_CYC));
  printf("  TMRD_CYC  = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TMRD_CYC));
  printf("  TRFC_CYC  = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TRFC_CYC));
  printf("  TWR_CYC   = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TWR_CYC));
  printf("  TDAL_CYC  = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TDAL_CYC));
  printf("  TREFI_CYC = 0x%08x\n", sdram_cfg_r(SDRAM_TIM_TREFI_CYC));
  printf("  CAS_LAT   = 0x%08x\n", sdram_cfg_r(SDRAM_CAP_CAS_LAT));
  printf("  KEEP_OPEN = 0x%08x\n", sdram_cfg_r(SDRAM_POL_KEEP_OPEN));
  printf("  NEG_EDGE  = 0x%08x\n", sdram_cfg_r(SDRAM_CAP_READ_NEGEDGE));
  printf("  STATUS    = 0x%08x\n", sdram_cfg_r(SDRAM_STAT_STATUS));
}

/* ==== Boot config ==== */
#define IMAGE_MB_SIZE 20
#define SD_BLOCK_OFFSET (1024 * 1024 / 512)
#define CHUNK_SIZE 512
#define TEST_LIMIT_MB 16
#define SDRAM_ROW_SIZE_BYTES 1024
#define ON_MEMTEST_FAIL_TRY_BOOT 0
#define SPI_ADDR_BASE 0x20000000
#define KERNEL_IMAGE (SPI_ADDR_BASE + 1024 * 1024 * 1)
#define SDRAM_START 0x80000000

static const volatile uint32_t* const SDRAM_SIZE = (volatile uint32_t*) CPU_MEMSIZE_REG_ADDR;
#define SDRAM_END (SDRAM_START + (*SDRAM_SIZE))

/* ==== I$ sync ==== */
static inline void icache_sync_range(void* start, uint32_t size) {
  data_fence();
#if defined(__riscv_zifencei)
  __asm__ volatile("fence.i" ::: "memory");
  (void) start;
  (void) size;
#else
  extern void __builtin___clear_cache(char*, char*);
  __builtin___clear_cache((char*) start, (char*) ((uintptr_t) start + size));
#endif
}

/* ==== small helpers ==== */
static void print_size(uint32_t bytes) {
  if (bytes >= (1024u * 1024u)) {
    printf("%u MiB", (unsigned) (bytes / (1024u * 1024u)));
  } else if (bytes >= 1024u) {
    printf("%u KiB", (unsigned) (bytes / 1024u));
  } else {
    printf("%u B", (unsigned) bytes);
  }
}

static void clear_sdram(void) {
  volatile uint32_t* p = (volatile uint32_t*) SDRAM_START;
  volatile uint32_t* end = (volatile uint32_t*) (SDRAM_START + (*SDRAM_SIZE - 1024 * 1024));
  while (p < end) { *p++ = 0; }
  data_fence();
}

/* ==== memory tests (trimmed) ==== */
static int mem_fill_verify_u32_ex(uint32_t s,
                                  uint32_t e,
                                  uint32_t pat,
                                  const char* name,
                                  uint32_t* first_fail_addr,
                                  uint32_t* first_got) {
  volatile uint32_t* p;
  printf(COLOR_CYAN "[MEM] Fill %s... " COLOR_RESET, name);
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) { *p = pat; }
  data_fence();
  printf("verify... ");
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) {
    uint32_t v = *p;
    if (v != pat) {
      if (first_fail_addr) { *first_fail_addr = (uint32_t) (uintptr_t) p; }
      if (first_got) { *first_got = v; }
      printf(COLOR_RED "\n[MEM][FAIL] %s @0x%08x: exp=0x%08x "
                       "got=0x%08x\n" COLOR_RESET,
             name,
             (unsigned) (uintptr_t) p,
             (unsigned) pat,
             (unsigned) v);
      return -1;
    }
  }
  printf(COLOR_GREEN "ok\n" COLOR_RESET);
  return 0;
}
static int mem_fill_verify_u32(uint32_t s, uint32_t e, uint32_t pat, const char* name) {
  return mem_fill_verify_u32_ex(s, e, pat, name, 0, 0);
}
static uint32_t mem_stuck_mask_zero(uint32_t s, uint32_t e, uint32_t* first_addr_out) {
  volatile uint32_t* p;
  uint32_t stuck1 = 0, first = 0;
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) {
    uint32_t v = *p;
    if (v && first == 0) { first = (uint32_t) (uintptr_t) p; }
    stuck1 |= v;
  }
  if (first_addr_out) { *first_addr_out = first; }
  return stuck1;
}
static void mem_bit_toggle(uint32_t a) {
  volatile uint32_t* p = (volatile uint32_t*) a;
  struct {
    uint32_t pat;
    const char* name;
  } vec[] = {{0x00000000u, "zero"},
             {0xFFFFFFFFu, "ones"},
             {0x00000020u, "only bit5"},
             {0xFFFFFFDFu, "all but bit5"},
             {0xAAAAAAAAu, "A"},
             {0x55555555u, "5"}};
  for (unsigned i = 0; i < sizeof(vec) / sizeof(vec[0]); ++i) {
    *p = vec[i].pat;
    data_fence();
    uint32_t v = *p;
    printf("[MEM] toggle @0x%08x %s write=0x%08x read=0x%08x\n", a, vec[i].name, vec[i].pat, v);
  }
}
static int mem_row_pingpong(uint32_t base, uint32_t size) {
  uint32_t a0 = base, a1 = base + SDRAM_ROW_SIZE_BYTES;
  if (a1 + 4 > base + size) { return 0; }
  volatile uint32_t* p0 = (volatile uint32_t*) a0;
  volatile uint32_t* p1 = (volatile uint32_t*) a1;
  printf(COLOR_CYAN "[MEM] Row ping-pong... " COLOR_RESET);
  for (int i = 0; i < 2048; ++i) {
    *p0 = (0x11110000u ^ (uint32_t) i);
    *p1 = (0x22220000u ^ ((uint32_t) i << 1));
  }
  data_fence();
  uint32_t v0 = *p0, v1 = *p1, e0 = (0x11110000u ^ 2047u), e1 = (0x22220000u ^ (2047u << 1));
  if (v0 != e0 || v1 != e1) {
    printf(COLOR_RED "\n[MEM][FAIL] RowPP @0x%08x/0x%08x "
                     "exp=%08x/%08x got=%08x/%08x\n" COLOR_RESET,
           (unsigned) a0,
           (unsigned) a1,
           (unsigned) e0,
           (unsigned) e1,
           (unsigned) v0,
           (unsigned) v1);
    return -1;
  }
  printf(COLOR_GREEN "ok\n" COLOR_RESET);
  return 0;
}
static int data_bus_test32(uint32_t base) {
  volatile uint32_t* p = (volatile uint32_t*) base;
  for (uint32_t bit = 0; bit < 32; ++bit) {
    uint32_t pat = (1u << bit);
    *p = pat;
    data_fence();
    if (*p != pat) {
      printf(COLOR_RED "[MEM][FAIL] DataBus w1 bit%u @0x%08x "
                       "got=0x%08x\n" COLOR_RESET,
             bit,
             base,
             *p);
      return -1;
    }
  }
  for (uint32_t bit = 0; bit < 32; ++bit) {
    uint32_t pat = ~(1u << bit);
    *p = pat;
    data_fence();
    if (*p != pat) {
      printf(COLOR_RED "[MEM][FAIL] DataBus w0 bit%u @0x%08x "
                       "got=0x%08x\n" COLOR_RESET,
             bit,
             base,
             *p);
      return -1;
    }
  }
  return 0;
}
static int addr_bus_test32(uint32_t base, uint32_t bytes) {
  volatile uint32_t* p;
  const uint32_t words = bytes >> 2;
  const uint32_t PAT = 0xAAAAAAAAu, APAT = 0x55555555u;
  for (uint32_t i = 0; i < words; ++i) {
    p = (volatile uint32_t*) (base + (i << 2));
    *p = PAT;
  }
  data_fence();
  for (uint32_t off = 1; off < words; off <<= 1) {
    volatile uint32_t* tgt = (volatile uint32_t*) (base + (off << 2));
    *tgt = APAT;
    data_fence();
    if (*(volatile uint32_t*) base != PAT) {
      printf(COLOR_RED "[MEM][FAIL] AddrBus alias: base "
                       "changed when off=0x%x\n" COLOR_RESET,
             off << 2);
      return -1;
    }
    if (*tgt != APAT) {
      printf(COLOR_RED "[MEM][FAIL] AddrBus target mismatch off=0x%x "
                       "exp=%08x got=%08x\n" COLOR_RESET,
             off << 2,
             APAT,
             *tgt);
      return -1;
    }
    *tgt = PAT;
    data_fence();
  }
  for (uint32_t i = 0; i < words; ++i) {
    p = (volatile uint32_t*) (base + (i << 2));
    if (*p != PAT) {
      printf(COLOR_RED "[MEM][FAIL] AddrBus post-verify @+%08x "
                       "exp=%08x got=%08x\n" COLOR_RESET,
             i << 2,
             PAT,
             *p);
      return -1;
    }
  }
  return 0;
}
static int march_c_minus(uint32_t s, uint32_t e) {
  volatile uint32_t* p;
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) { *p = 0x00000000u; }
  data_fence();
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) {
    if (*p != 0) { return -1; }
    *p = 0xFFFFFFFFu;
  }
  data_fence();
  for (p = (volatile uint32_t*) s; (uint32_t) p < e; ++p) {
    if (*p != 0xFFFFFFFFu) { return -1; }
    *p = 0x00000000u;
  }
  data_fence();
  for (p = (volatile uint32_t*) (e - 4); (uint32_t) p >= s; --p) {
    if (*p != 0) { return -1; }
    *p = 0xFFFFFFFFu;
    if ((uint32_t) p == s) { break; }
  }
  data_fence();
  for (p = (volatile uint32_t*) (e - 4); (uint32_t) p >= s; --p) {
    if (*p != 0xFFFFFFFFu) { return -1; }
    *p = 0x00000000u;
    if ((uint32_t) p == s) { break; }
  }
  data_fence();
  for (p = (volatile uint32_t*) (e - 4); (uint32_t) p >= s; --p) {
    if (*p != 0) { return -1; }
    if ((uint32_t) p == s) { break; }
  }
  return 0;
}
static inline uint32_t lfsr32_step(uint32_t s) {
  uint32_t b = (s ^ (s >> 1) ^ (s >> 21) ^ (s >> 31)) & 1u;
  return (s >> 1) | (b << 31);
}
static int prbs_stream(uint32_t s, uint32_t e, uint32_t seed, uint32_t stride_words) {
  volatile uint32_t* p;
  uint32_t st = (seed ? seed : 0x1u);
  uint32_t words = (e - s) >> 2;
  for (uint32_t i = 0; i < words; i += stride_words) {
    p = (volatile uint32_t*) (s + (i << 2));
    *p = st;
    st = lfsr32_step(st);
  }
  data_fence();
  st = (seed ? seed : 0x1u);
  for (uint32_t i = 0; i < words; i += stride_words) {
    p = (volatile uint32_t*) (s + (i << 2));
    uint32_t exp = st, got = *p;
    if (got != exp) {
      printf(COLOR_RED "[MEM][FAIL] PRBS @+%08x exp=%08x "
                       "got=%08x\n" COLOR_RESET,
             (unsigned) (i << 2),
             (unsigned) exp,
             (unsigned) got);
      return -1;
    }
    st = lfsr32_step(st);
  }
  return 0;
}
static int row_hammer_small(uint32_t base, uint32_t row_sz, uint32_t iters) {
  volatile uint32_t* victim = (volatile uint32_t*) (base + row_sz);
  volatile uint32_t* agg0 = (volatile uint32_t*) (base + 0 * row_sz);
  volatile uint32_t* agg1 = (volatile uint32_t*) (base + 2 * row_sz);
  *victim = 0xCAFEBABEu;
  data_fence();
  for (uint32_t i = 0; i < iters; i++) {
    *agg0 = i;
    *agg1 = ~i;
  }
  data_fence();
  if (*victim != 0xCAFEBABEu) {
    printf(COLOR_RED "[MEM][FAIL] RowHammer victim flipped: "
                     "exp=CAFEBABE got=%08x\n" COLOR_RESET,
           *victim);
    return -1;
  }
  return 0;
}
static int sdram_memtest(void) {
  uint32_t total = *SDRAM_SIZE;
  uint32_t limit = (TEST_LIMIT_MB * 1024u * 1024u);
  if (limit == 0 || limit > total) { limit = total; }
  uint32_t start = SDRAM_START, end = SDRAM_START + limit;
  printf(COLOR_YELLOW "[MEM] Testing ");
  print_size(limit);
  printf(" / ");
  print_size(total);
  printf("...\n" COLOR_RESET);
  uint32_t fail_addr = 0, fail_got = 0;
  if (mem_fill_verify_u32_ex(start, end, 0x00000000u, "zero", &fail_addr, &fail_got)) {
    uint32_t mask_first_addr = 0;
    uint32_t stuck1 = mem_stuck_mask_zero(start, end, &mask_first_addr);
    printf("[MEM] stuck1 (after zero) = 0x%08x, first @0x%08x\n", stuck1, mask_first_addr);
    uint32_t probe = fail_addr ? fail_addr : (SDRAM_START + 0x4000u);
    mem_bit_toggle(probe);
    return -1;
  }
  if (mem_fill_verify_u32(start, end, 0xFFFFFFFFu, "ones")) { return -1; }
  {
    volatile uint32_t* p;
    printf(COLOR_CYAN "[MEM] Checkerboard A... " COLOR_RESET);
    for (p = (volatile uint32_t*) start; (uint32_t) p < end; ++p) {
      uint32_t off = ((uint32_t) p - start) >> 2;
      *p = (off & 1) ? 0x55555555u : 0xAAAAAAAAu;
    }
    data_fence();
    printf("verify... ");
    for (p = (volatile uint32_t*) start; (uint32_t) p < end; ++p) {
      uint32_t off = ((uint32_t) p - start) >> 2;
      uint32_t exp = (off & 1) ? 0x55555555u : 0xAAAAAAAAu;
      uint32_t got1 = *p;
      if (got1 != exp) {
        uint32_t got2 = *p;
        printf(COLOR_RED "\n[MEM][FAIL] CheckerA @0x%08x: "
                         "exp=0x%08x got1=0x%08x "
                         "got2=0x%08x\n" COLOR_RESET,
               (unsigned) (uintptr_t) p,
               (unsigned) exp,
               (unsigned) got1,
               (unsigned) got2);
        return -1;
      }
    }
    printf(COLOR_GREEN "ok\n" COLOR_RESET);
    printf(COLOR_CYAN "[MEM] Checkerboard B... " COLOR_RESET);
    for (p = (volatile uint32_t*) start; (uint32_t) p < end; ++p) {
      uint32_t off = ((uint32_t) p - start) >> 2;
      *p = (off & 1) ? 0xAAAAAAAAu : 0x55555555u;
    }
    data_fence();
    printf("verify... ");
    for (p = (volatile uint32_t*) start; (uint32_t) p < end; ++p) {
      uint32_t off = ((uint32_t) p - start) >> 2;
      uint32_t exp = (off & 1) ? 0xAAAAAAAAu : 0x55555555u;
      uint32_t got1 = *p;
      if (got1 != exp) {
        uint32_t got2 = *p;
        printf(COLOR_RED "\n[MEM][FAIL] CheckerB @0x%08x: "
                         "exp=0x%08x got1=0x%08x "
                         "got2=0x%08x\n" COLOR_RESET,
               (unsigned) (uintptr_t) p,
               (unsigned) exp,
               (unsigned) got1,
               (unsigned) got2);
        return -1;
      }
    }
    printf(COLOR_GREEN "ok\n" COLOR_RESET);
  }
  if (mem_row_pingpong(SDRAM_START, (64u * 1024u))) { return -1; }
  if (data_bus_test32(start) != 0) { return -1; }
  if (addr_bus_test32(start, 128u * 1024u) != 0) { return -1; }
  if (march_c_minus(start, end) != 0) {
    printf(COLOR_RED "[MEM][FAIL] March C-\n" COLOR_RESET);
    return -1;
  }
  if (prbs_stream(start, end, 0x1D872B41u, 4u) != 0) { return -1; }
  if (row_hammer_small(start, SDRAM_ROW_SIZE_BYTES, 100000u) != 0) { return -1; }
  printf(COLOR_GREEN "[MEM] SDRAM test PASSED\n" COLOR_RESET);
  return 0;
}

/* ==== main ==== */
typedef void (*func_ptr)(int, char*);
/* ==== address print helpers (portable) ==== */
#if UINTPTR_MAX == 0xffffffffu
#define PTR_FMT "0x%08x"
#define PTR_ARG(p) (unsigned) ((uintptr_t) (p))
#else
#define PTR_FMT "0x%016llx"
#define PTR_ARG(p) (unsigned long long) ((uintptr_t) (p))
#endif

void main(void) {
  /* Program divider CSRs from current CPU clock:
   *  - DIV_ADDR0: [31:16]=SPI0 -> 12 MHz, [15:0]=UART -> 115200 baud
   *  - DIV_ADDR1: [31:16]=SPI1 -> 24 MHz, [15:0]=CLINT -> 1 MHz tick
   */
  soc_program_dividers(DIV_ADDR0, DIV_ADDR1, 115200u, 12000000u, 24000000u, 1u);

  delay_seconds(1);

/* --- banner without rulers --- */
#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif
#ifndef GIT_DESC
#define GIT_DESC GIT_HASH
#endif

  printf(COLOR_BLUE COLOR_BOLD "\n[BOOT] KianV RISC-V RV32IMA + SSTC, ZICNTR\n"
                               "[BOOT] SV32 RLE ROM Bootloader v1.0\n"
                               "[BOOT] Build: " __DATE__ " " __TIME__ " (" GIT_HASH ")\n"
                               "[BOOT] Version: " GIT_DESC "\n\n" COLOR_RESET);

  /* --- cpu freq --- */
  uint32_t freq = read_cpu_frequency();
  printf(COLOR_CYAN "[CPU] Frequency: " COLOR_RESET "%u Hz\n", freq);

  /* --- sdram init --- */
  if (sdram_init_default() != 0) {
    printf(COLOR_RED "[SDRAM] init failed — halting.\n" COLOR_RESET);
    while (1) {}
  }
  sdram_cfg_dump();

  printf(COLOR_YELLOW "[SDRAM] " COLOR_RESET "Size reported: ");
  print_size(*SDRAM_SIZE);
  printf("\n\n");

  /*
  printf("[SDRAM] Clearing...\n");
  clear_sdram();
  */
  if (sdram_memtest() != 0) {
#if ON_MEMTEST_FAIL_TRY_BOOT
    printf(COLOR_RED "[MEM] Errors detected, continuing "
                     "anyway...\n" COLOR_RESET);
#else
    printf(COLOR_RED "[MEM] Errors detected, halting boot.\n" COLOR_RESET);
    while (1) {}
#endif
  }

  printf("[SDRAM] Clearing...\n");
  clear_sdram();

  /* --- wait for sd card (ANSI cursor save/restore) --- */
  {
    const char spinner[] = "|/-\\";
    int si = 0;
    for (;;) {
      printf("\033[s"); /* save cursor */
      printf(COLOR_YELLOW "[SD] Insert SD card %c" COLOR_RESET, spinner[si]);
      // fflush(stdout);
      if (sd_init() == 0) {
        printf("\033[K\r[OK] SD card initialized.\n"); /* clear line, CR, print */
        break;
      } else {
        printf("\033[u"); /* restore cursor */
      }
      si = (si + 1) & 3;
    }
  }

  /* --- load RLE image --- */
  printf(COLOR_PURPLE "[SD] Loading RLE image...\n" COLOR_RESET);

  uint8_t buffer[CHUNK_SIZE];
  int block_index = 0;
  uint32_t sdram_offset = 0;
  unsigned char* sdram_ptr = (unsigned char*) SDRAM_START;
  const uint32_t sdram_size = *SDRAM_SIZE;

  for (uint32_t loaded = 0; loaded < (IMAGE_MB_SIZE * 1024u * 1024u); loaded += CHUNK_SIZE) {
    if (sd_readsector(SD_BLOCK_OFFSET + block_index++, buffer, 1)) {
      /* decode pairs (value,count) */
      for (size_t j = 0; j + 1 < CHUNK_SIZE; j += 2) {
        unsigned char value = buffer[j];
        unsigned char count = buffer[j + 1];

        if (count == 0) {
          printf(COLOR_GREEN "[OK] Loaded RLE image: %u "
                             "bytes\n" COLOR_RESET,
                 loaded);
          goto end_of_image;
        }
        while (count--) {
          if (sdram_offset >= sdram_size) {
            printf(COLOR_RED "[ERR] Image exceeds "
                             "SDRAM size at %u "
                             "bytes\n" COLOR_RESET,
                   sdram_offset);
            goto end_of_image;
          }
          sdram_ptr[sdram_offset++] = value;
        }
      }

      /* progress every MiB, overwrite in-place */
      if ((loaded % (1024u * 1024u)) == 0) {
        printf("\033[s");
        printf(COLOR_GREEN "Loaded %u MiB" COLOR_RESET, loaded / (1024u * 1024u));
        printf("\033[u");
      }

    } else {
      printf(COLOR_RED "[SD] Read error at LBA %d\n" COLOR_RESET,
             SD_BLOCK_OFFSET + block_index - 1);
      break;
    }
  }

end_of_image:
  printf(COLOR_GREEN "[OK] Decompressed RLE image size: %u bytes\n" COLOR_RESET, sdram_offset);

  icache_sync_range((void*) SDRAM_START, sdram_offset);

  func_ptr entry = (func_ptr) SDRAM_START;
  printf(COLOR_BOLD "\n[EXEC] control -> entry=" PTR_FMT " (a0=0, a1=0)\n\n" COLOR_RESET,
         PTR_ARG(entry));
  entry(0, 0);
}
