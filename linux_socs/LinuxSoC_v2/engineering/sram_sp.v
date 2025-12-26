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

module sram_sp #(
    parameter integer DEPTH = 512,
    parameter integer WIDTH = 56
) (
    input  wire                     clk,
    input  wire                     we,
    input  wire [$clog2(DEPTH)-1:0] addr,
    input  wire [        WIDTH-1:0] din,
    output reg  [        WIDTH-1:0] dout
);

  (* ram_style = "block" *) reg [WIDTH-1:0] mem[0:DEPTH-1];

  always @(posedge clk) begin
    if (we) mem[addr] <= din;

    dout <= mem[addr];
  end
endmodule

`default_nettype wire

