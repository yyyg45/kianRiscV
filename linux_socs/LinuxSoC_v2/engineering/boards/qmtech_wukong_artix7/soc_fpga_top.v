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
`timescale 1ns / 1ps
`include "defines_soc.vh"

module soc_fpga_top (
    input wire clk_osc,
    input wire ext_resetn,

    output wire uart_tx,
    input  wire uart_rx,

    output wire        sdram_clk,
    output wire        sdram_cke,
    output wire [12:0] sdram_addr,
    output wire [ 1:0] sdram_ba,
    output wire [ 1:0] sdram_dqm,
    output wire        sdram_csn,
    output wire        sdram_wen,
    output wire        sdram_rasn,
    output wire        sdram_casn,
    inout  wire [15:0] sdram_dq,

    output wire spi_cen0,
    output wire spi_sclk0,
    input  wire spi_sio1_so_miso0,
    output wire spi_sio0_si_mosi0,

    output wire spi_cen1,
    output wire spi_sclk1,
    input  wire spi_sio1_so_miso1,
    output wire spi_sio0_si_mosi1,

    output wire flash_csn,
    input  wire flash_miso,
    output wire flash_mosi,

    inout wire [9:0] gpio
);

  wire [15:0] sdram_dq_out;
  wire [15:0] sdram_dq_in;
  wire        sdram_dq_oe;

  assign sdram_dq    = sdram_dq_oe ? sdram_dq_out : 16'hzzzz;
  assign sdram_dq_in = sdram_dq;

  wire clk;
  wire locked;

  pll u_pll (
      .clk_in1 (clk_osc),
      .clk_out1(clk),
      .locked  (locked)
  );

  wire flash_sclk_mux;

  STARTUPE2 #(
      .PROG_USR     ("FALSE"),
      .SIM_CCLK_FREQ(0.0)
  ) u_startupe2 (
      .CFGCLK   (),
      .CFGMCLK  (),
      .EOS      (),
      .PREQ     (),
      .CLK      (1'b0),
      .GSR      (1'b0),
      .GTS      (1'b0),
      .KEYCLEARB(1'b1),
      .PACK     (1'b0),
      .USRCCLKO (flash_sclk_mux),
      .USRCCLKTS(1'b0),
      .USRDONEO (1'b1),
      .USRDONETS(1'b1)
  );

  wire flash_init_done;
  wire fi_csn, fi_sclk, fi_mosi;

  n25q_flash_init #(
      .DIV(120)
  ) u_flash_init (
      .clk  (clk),
      .start(locked),
      .done (flash_init_done),
      .csn  (fi_csn),
      .sclk (fi_sclk),
      .mosi (fi_mosi)
  );

  wire soc_flash_sclk;
  wire soc_flash_csn;
  wire soc_flash_mosi;

  assign flash_csn      = flash_init_done ? soc_flash_csn : fi_csn;
  assign flash_mosi     = flash_init_done ? soc_flash_mosi : fi_mosi;
  assign flash_sclk_mux = flash_init_done ? soc_flash_sclk : fi_sclk;

  wire rst_n_async = ext_resetn & locked & flash_init_done;

  wire resetn_sync;
  async_reset_sync u_reset_sync (
      .clk        (clk),
      .rst_n_async(rst_n_async),
      .rst_n_sync (resetn_sync)
  );

  wire [9:0] gpio_out;
  wire [9:0] gpio_in;
  wire [9:0] gpio_oe;

  genvar i;
  generate
    for (i = 0; i < 10; i = i + 1) begin : GPIO_BUF
      assign gpio[i]    = gpio_oe[i] ? gpio_out[i] : 1'bz;
      assign gpio_in[i] = gpio[i];
    end
  endgenerate

  soc u_soc (
      .clk_osc   (clk),
      .ext_resetn(resetn_sync),

      .uart_tx(uart_tx),
      .uart_rx(uart_rx),

      .flash_sclk(soc_flash_sclk),
      .flash_csn (soc_flash_csn),
      .flash_miso(flash_miso),
      .flash_mosi(soc_flash_mosi),

      .sdram_clk   (sdram_clk),
      .sdram_cke   (sdram_cke),
      .sdram_dqm   (sdram_dqm),
      .sdram_addr  (sdram_addr),
      .sdram_ba    (sdram_ba),
      .sdram_csn   (sdram_csn),
      .sdram_wen   (sdram_wen),
      .sdram_rasn  (sdram_rasn),
      .sdram_casn  (sdram_casn),
      .sdram_dq_in (sdram_dq_in),
      .sdram_dq_out(sdram_dq_out),
      .sdram_dq_oe (sdram_dq_oe),

      .spi_cen0         (spi_cen0),
      .spi_sclk0        (spi_sclk0),
      .spi_sio1_so_miso0(spi_sio1_so_miso0),
      .spi_sio0_si_mosi0(spi_sio0_si_mosi0),

      .spi_cen1         (spi_cen1),
      .spi_sclk1        (spi_sclk1),
      .spi_sio1_so_miso1(spi_sio1_so_miso1),
      .spi_sio0_si_mosi1(spi_sio0_si_mosi1),

      .gpio_in (gpio_in),
      .gpio_out(gpio_out),
      .gpio_oe (gpio_oe)
  );

endmodule

module n25q_flash_init #(
    parameter integer DIV = 120
) (
    input  wire clk,
    input  wire start,
    output reg  done,
    output reg  csn,
    output reg  sclk,
    output reg  mosi
);

  localparam integer DCW = (DIV <= 1) ? 1 : $clog2(DIV);
  reg [DCW-1:0] divcnt;
  wire tick = (divcnt == (DIV - 1));

  localparam [2:0]
    ST_IDLE  = 3'd0,
    ST_CSLOW = 3'd1,
    ST_SHIFT = 3'd2,
    ST_CSHI  = 3'd3,
    ST_GAP   = 3'd4,
    ST_DONE  = 3'd5;

  reg [2:0] state;

  reg [1:0] cmd_idx;
  reg [7:0] shreg;
  reg [3:0] bitcnt;
  reg [7:0] gapcnt;

  reg [7:0] cur_cmd;
  always @(*) begin
    case (cmd_idx)
      2'd0: cur_cmd = 8'hFF;
      2'd1: cur_cmd = 8'h66;
      default: cur_cmd = 8'h99;
    endcase
  end

  always @(posedge clk) begin
    if (!start || done) begin
      divcnt <= {DCW{1'b0}};
    end else if (tick) begin
      divcnt <= {DCW{1'b0}};
    end else begin
      divcnt <= divcnt + {{(DCW - 1) {1'b0}}, 1'b1};
    end
  end

  always @(posedge clk) begin
    if (!start) begin
      state   <= ST_IDLE;
      done    <= 1'b0;
      csn     <= 1'b1;
      sclk    <= 1'b0;
      mosi    <= 1'b0;
      cmd_idx <= 2'd0;
      shreg   <= 8'h00;
      bitcnt  <= 4'd0;
      gapcnt  <= 8'd0;
    end else begin
      case (state)
        ST_IDLE: begin
          done    <= 1'b0;
          csn     <= 1'b1;
          sclk    <= 1'b0;
          mosi    <= 1'b0;
          cmd_idx <= 2'd0;
          state   <= ST_CSLOW;
        end

        ST_CSLOW: begin
          csn    <= 1'b0;
          sclk   <= 1'b0;
          shreg  <= cur_cmd;
          bitcnt <= 4'd8;
          mosi   <= cur_cmd[7];
          state  <= ST_SHIFT;
        end

        ST_SHIFT: begin
          if (tick) begin
            sclk <= ~sclk;

            if (sclk == 1'b1) begin
              shreg  <= {shreg[6:0], 1'b0};
              bitcnt <= bitcnt - 4'd1;
              mosi   <= shreg[6];
              if (bitcnt == 4'd1) begin
                state <= ST_CSHI;
              end
            end
          end
        end

        ST_CSHI: begin
          csn    <= 1'b1;
          sclk   <= 1'b0;
          mosi   <= 1'b0;
          gapcnt <= 8'd20;
          state  <= ST_GAP;
        end

        ST_GAP: begin
          if (tick) begin
            if (gapcnt != 8'd0) begin
              gapcnt <= gapcnt - 8'd1;
            end else begin
              if (cmd_idx == 2'd2) begin
                state <= ST_DONE;
              end else begin
                cmd_idx <= cmd_idx + 2'd1;
                state   <= ST_CSLOW;
              end
            end
          end
        end

        ST_DONE: begin
          done  <= 1'b1;
          csn   <= 1'b1;
          sclk  <= 1'b0;
          mosi  <= 1'b0;
          state <= ST_DONE;
        end

        default: state <= ST_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire

