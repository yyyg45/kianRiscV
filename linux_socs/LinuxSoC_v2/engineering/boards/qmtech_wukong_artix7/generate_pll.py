#!/usr/bin/env python3

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

import sys

if len(sys.argv) != 2:
    print("Usage: generate_pll.py <target_frequency>")
    sys.exit(1)

# Target frequency from command line
target_freq = int(sys.argv[1])
input_freq = 50_000_000  # 50 MHz input clock

# Calculate PLL parameters
# VCO must be between 800 MHz and 1600 MHz for PLLE2_ADV
best_mult = 0
best_div = 0
best_error = float('inf')

for mult in range(2, 64):  # CLKFBOUT_MULT range
    vco_freq = input_freq * mult
    if vco_freq < 800_000_000 or vco_freq > 1_600_000_000:
        continue

    for div in range(1, 128):  # CLKOUT0_DIVIDE range
        output_freq = vco_freq / div
        error = abs(output_freq - target_freq)

        if error < best_error:
            best_error = error
            best_mult = mult
            best_div = div

actual_freq = (input_freq * best_mult) / best_div
vco_freq = input_freq * best_mult

print(f"""// Generated PLL for {actual_freq/1e6:.2f} MHz (target: {target_freq/1e6:.2f} MHz)
// VCO frequency: {vco_freq/1e6:.2f} MHz
// Error: {best_error/1e3:.3f} kHz
// Input frequency: 50 MHz
`default_nettype none
`timescale 1ps / 1ps

module pll (
    output wire clk_out1,
    output wire locked,
    input  wire clk_in1
);

  wire clk_in1_pll;

  IBUF clkin1_ibufg (
      .O(clk_in1_pll),
      .I(clk_in1)
  );

  wire        clk_out1_pll;
  wire [15:0] do_unused;
  wire        drdy_unused;
  wire        locked_int;
  wire        clkfbout_pll;
  wire        clkfbout_buf_pll;
  wire        clkout1_unused;
  wire        clkout2_unused;
  wire        clkout3_unused;
  wire        clkout4_unused;
  wire        clkout5_unused;

  PLLE2_ADV #(
      .BANDWIDTH         ("OPTIMIZED"),
      .COMPENSATION      ("ZHOLD"),
      .STARTUP_WAIT      ("FALSE"),
      .DIVCLK_DIVIDE     (1),
      .CLKFBOUT_MULT     ({best_mult}),
      .CLKFBOUT_PHASE    (0.000),
      .CLKOUT0_DIVIDE    ({best_div}),
      .CLKOUT0_PHASE     (0.000),
      .CLKOUT0_DUTY_CYCLE(0.500),
      .CLKIN1_PERIOD     (20.000)
  ) plle2_adv_inst (
      .CLKFBOUT(clkfbout_pll),
      .CLKOUT0 (clk_out1_pll),
      .CLKOUT1 (clkout1_unused),
      .CLKOUT2 (clkout2_unused),
      .CLKOUT3 (clkout3_unused),
      .CLKOUT4 (clkout4_unused),
      .CLKOUT5 (clkout5_unused),
      .CLKFBIN (clkfbout_buf_pll),
      .CLKIN1  (clk_in1_pll),
      .CLKIN2  (1'b0),
      .CLKINSEL(1'b1),
      .DADDR   (7'h0),
      .DCLK    (1'b0),
      .DEN     (1'b0),
      .DI      (16'h0),
      .DO      (do_unused),
      .DRDY    (drdy_unused),
      .DWE     (1'b0),
      .LOCKED  (locked_int),
      .PWRDWN  (1'b0),
      .RST     (1'b0)
  );

  assign locked = locked_int;

  BUFG clkf_buf (
      .O(clkfbout_buf_pll),
      .I(clkfbout_pll)
  );

  BUFG clkout1_buf (
      .O(clk_out1),
      .I(clk_out1_pll)
  );

endmodule
`default_nettype wire
""")
