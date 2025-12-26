// SPDX-License-Identifier: Apache-2.0
/*
 * KianV RISC-V Linux/XV6 SoC
 * RISC-V SoC/ASIC Design
 *
 * Copyright (c) 2025 Hirosh Dabui <hirosh@dabui.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

`default_nettype none

module cache_sram_D$ #(
    parameter integer NUM_SET    = 512,
    parameter integer LINE_BYTES = 4,
    parameter integer ADDR_WIDTH = 32,
    parameter integer DATA_WIDTH = 32
) (
    input wire clk,
    input wire resetn,
`ifdef USE_POWER_PINS
    inout      VDD,
    inout      VSS,
`endif
    input wire flush,

    input wire [$clog2(NUM_SET)-1:0] idx,
    input wire [ADDR_WIDTH-$clog2(NUM_SET)-$clog2(LINE_BYTES)-1:0] tag,

    input wire                  we,
    input wire                  re,
    input wire [DATA_WIDTH-1:0] wdata,

    output reg [DATA_WIDTH-1:0] rdata,
    output reg                  hit
);

  localparam integer OFFSET_BITS = $clog2(LINE_BYTES);
  localparam integer IDX_BITS = $clog2(NUM_SET);
  localparam integer TAG_BITS = ADDR_WIDTH - OFFSET_BITS - IDX_BITS;

  localparam integer SUM_BITS = TAG_BITS + DATA_WIDTH;
  localparam integer PACK_BYTES = (SUM_BITS + 7) / 8;
  localparam integer PACK_BITS = 8 * PACK_BYTES;
  localparam integer PAD_BITS = PACK_BITS - SUM_BITS;

  function automatic integer is_pow2(input integer x);
    begin
      is_pow2 = (x > 0) && ((x & (x - 1)) == 0);
    end
  endfunction

  initial begin
    if (!is_pow2(NUM_SET))
      $fatal(1, "cache_sram_D$: NUM_SET (%0d) must be a power-of-two.", NUM_SET);

    if (NUM_SET < 2) $fatal(1, "cache_sram_D$: NUM_SET (%0d) must be >= 2.", NUM_SET);

    if (!is_pow2(LINE_BYTES))
      $fatal(1, "cache_sram_D$: LINE_BYTES (%0d) must be a power-of-two.", LINE_BYTES);

    if ((LINE_BYTES * 8) != DATA_WIDTH)
      $fatal(
          1,
          "cache_sram_D$: LINE_BYTES*8 (%0d) must equal DATA_WIDTH (%0d).",
          LINE_BYTES * 8,
          DATA_WIDTH
      );
  end

  (* keep *) reg [NUM_SET-1:0] valid0, valid1, valid2, valid3;
  (* keep *) reg [3*NUM_SET-1:0] plru;

  wire [2:0] plru_bits = plru[idx*3+:3];
  wire b0 = plru_bits[0];
  wire b1 = plru_bits[1];
  wire b2 = plru_bits[2];

  wire [PACK_BITS-1:0] packed_out0, packed_out1, packed_out2, packed_out3;

  reg  [1:0] hit_way;
  reg  [1:0] victim_way;
  wire [1:0] target_way;
  wire       hit_comb;

  wire write_sel_way0, write_sel_way1, write_sel_way2, write_sel_way3;

  sram_sp #(
      .DEPTH(NUM_SET),
      .WIDTH(PACK_BITS)
  ) u_mem_way0 (
      .clk (clk),
      .we  (write_sel_way0),
      .addr(idx),
      .din ({{PAD_BITS{1'b0}}, tag, wdata}),
      .dout(packed_out0)
  );

  sram_sp #(
      .DEPTH(NUM_SET),
      .WIDTH(PACK_BITS)
  ) u_mem_way1 (
      .clk (clk),
      .we  (write_sel_way1),
      .addr(idx),
      .din ({{PAD_BITS{1'b0}}, tag, wdata}),
      .dout(packed_out1)
  );

  sram_sp #(
      .DEPTH(NUM_SET),
      .WIDTH(PACK_BITS)
  ) u_mem_way2 (
      .clk (clk),
      .we  (write_sel_way2),
      .addr(idx),
      .din ({{PAD_BITS{1'b0}}, tag, wdata}),
      .dout(packed_out2)
  );

  sram_sp #(
      .DEPTH(NUM_SET),
      .WIDTH(PACK_BITS)
  ) u_mem_way3 (
      .clk (clk),
      .we  (write_sel_way3),
      .addr(idx),
      .din ({{PAD_BITS{1'b0}}, tag, wdata}),
      .dout(packed_out3)
  );

  localparam integer DATA_LO = 0;
  localparam integer DATA_HI = DATA_LO + DATA_WIDTH - 1;
  localparam integer TAG_LO = DATA_HI + 1;
  localparam integer TAG_HI = TAG_LO + TAG_BITS - 1;

  wire [DATA_WIDTH-1:0] data0 = packed_out0[DATA_HI:DATA_LO];
  wire [DATA_WIDTH-1:0] data1 = packed_out1[DATA_HI:DATA_LO];
  wire [DATA_WIDTH-1:0] data2 = packed_out2[DATA_HI:DATA_LO];
  wire [DATA_WIDTH-1:0] data3 = packed_out3[DATA_HI:DATA_LO];

  wire [TAG_BITS-1:0] tag0 = packed_out0[TAG_HI:TAG_LO];
  wire [TAG_BITS-1:0] tag1 = packed_out1[TAG_HI:TAG_LO];
  wire [TAG_BITS-1:0] tag2 = packed_out2[TAG_HI:TAG_LO];
  wire [TAG_BITS-1:0] tag3 = packed_out3[TAG_HI:TAG_LO];

  wire way0_match = valid0[idx] && (tag0 == tag);
  wire way1_match = valid1[idx] && (tag1 == tag);
  wire way2_match = valid2[idx] && (tag2 == tag);
  wire way3_match = valid3[idx] && (tag3 == tag);

  assign hit_comb = way0_match | way1_match | way2_match | way3_match;

  always @* begin
    if (way0_match) hit_way = 2'd0;
    else if (way1_match) hit_way = 2'd1;
    else if (way2_match) hit_way = 2'd2;
    else if (way3_match) hit_way = 2'd3;
    else hit_way = 2'd0;
  end

  always @* begin
    if (!valid0[idx]) victim_way = 2'd0;
    else if (!valid1[idx]) victim_way = 2'd1;
    else if (!valid2[idx]) victim_way = 2'd2;
    else if (!valid3[idx]) victim_way = 2'd3;
    else begin
      if (b0 == 1'b0) victim_way = (b1 == 1'b0) ? 2'd0 : 2'd1;
      else victim_way = (b2 == 1'b0) ? 2'd2 : 2'd3;
    end
  end

  assign target_way = hit_comb ? hit_way : victim_way;

  assign write_sel_way0 = we && (target_way == 2'd0);
  assign write_sel_way1 = we && (target_way == 2'd1);
  assign write_sel_way2 = we && (target_way == 2'd2);
  assign write_sel_way3 = we && (target_way == 2'd3);

  always @* begin
    hit = hit_comb;
    if (way0_match) rdata = data0;
    else if (way1_match) rdata = data1;
    else if (way2_match) rdata = data2;
    else if (way3_match) rdata = data3;
    else rdata = {DATA_WIDTH{1'b0}};
  end

  function automatic [2:0] plru_update(input [2:0] cur, input [1:0] used_way);
    reg nb0, nb1, nb2;
    begin
      nb0 = cur[0];
      nb1 = cur[1];
      nb2 = cur[2];
      case (used_way)
        2'd0: begin
          nb0 = 1'b1;
          nb1 = 1'b1;
        end
        2'd1: begin
          nb0 = 1'b1;
          nb1 = 1'b0;
        end
        2'd2: begin
          nb0 = 1'b0;
          nb2 = 1'b1;
        end
        2'd3: begin
          nb0 = 1'b0;
          nb2 = 1'b0;
        end
        default: ;
      endcase
      plru_update = {nb2, nb1, nb0};
    end
  endfunction

  always @(posedge clk) begin
    if (!resetn) begin
      valid0 <= {NUM_SET{1'b0}};
      valid1 <= {NUM_SET{1'b0}};
      valid2 <= {NUM_SET{1'b0}};
      valid3 <= {NUM_SET{1'b0}};
      plru   <= {(3 * NUM_SET) {1'b0}};
    end else if (flush) begin
      valid0 <= {NUM_SET{1'b0}};
      valid1 <= {NUM_SET{1'b0}};
      valid2 <= {NUM_SET{1'b0}};
      valid3 <= {NUM_SET{1'b0}};
      plru   <= {(3 * NUM_SET) {1'b0}};
    end else begin
      if (write_sel_way0) valid0[idx] <= 1'b1;
      if (write_sel_way1) valid1[idx] <= 1'b1;
      if (write_sel_way2) valid2[idx] <= 1'b1;
      if (write_sel_way3) valid3[idx] <= 1'b1;

      if (re && hit_comb) begin
        plru[idx*3+:3] <= plru_update(plru_bits, hit_way);
      end else if (we) begin
        plru[idx*3+:3] <= plru_update(plru_bits, target_way);
      end
    end
  end

endmodule

`default_nettype wire
