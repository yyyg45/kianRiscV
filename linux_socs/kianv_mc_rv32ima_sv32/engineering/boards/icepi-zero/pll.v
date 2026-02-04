/*
 * - No defparam
 * - generate-if condition is CONSTANT (localparam)
 *
 * FPGA kind      : ECP5
 * Input frequency: 25 MHz
 */

/* verilator lint_off TIMESCALEMOD */
module pll #(
        parameter integer freq = 25
    ) (
        input  wire clki,
        output wire clko,
        output wire locked
    );

    // ------------------------------------------------------------------------
    // Frequency -> divider table
    // ------------------------------------------------------------------------
    function integer f_clki_div;
        input integer f;
        begin
            case (f)
                16:  f_clki_div = 8;
                20:  f_clki_div = 5;
                24:  f_clki_div = 1;
                25:  f_clki_div = 1;
                30:  f_clki_div = 5;
                35:  f_clki_div = 5;
                40:  f_clki_div = 5;
                45:  f_clki_div = 5;
                48:  f_clki_div = 8;
                50:  f_clki_div = 1;
                55:  f_clki_div = 5;
                60:  f_clki_div = 5;
                65:  f_clki_div = 5;
                66:  f_clki_div = 8;
                70:  f_clki_div = 5;
                75:  f_clki_div = 1;
                80:  f_clki_div = 5;
                85:  f_clki_div = 5;
                90:  f_clki_div = 5;
                95:  f_clki_div = 5;
                100: f_clki_div = 1;
                105: f_clki_div = 5;
                110: f_clki_div = 5;
                115: f_clki_div = 5;
                120: f_clki_div = 5;
                125: f_clki_div = 1;
                130: f_clki_div = 5;
                135: f_clki_div = 5;
                140: f_clki_div = 5;
                default: f_clki_div = -1;
            endcase
        end
    endfunction

    function integer f_clkop_div;
        input integer f;
        begin
            case (f)
                16:  f_clkop_div = 38;
                20:  f_clkop_div = 30;
                24:  f_clkop_div = 24;
                25:  f_clkop_div = 24;
                30:  f_clkop_div = 20;
                35:  f_clkop_div = 17;
                40:  f_clkop_div = 15;
                45:  f_clkop_div = 13;
                48:  f_clkop_div = 13;
                50:  f_clkop_div = 12;
                55:  f_clkop_div = 11;
                60:  f_clkop_div = 10;
                65:  f_clkop_div = 9;
                66:  f_clkop_div = 9;
                70:  f_clkop_div = 9;
                75:  f_clkop_div = 8;
                80:  f_clkop_div = 7;
                85:  f_clkop_div = 7;
                90:  f_clkop_div = 7;
                95:  f_clkop_div = 6;
                100: f_clkop_div = 6;
                105: f_clkop_div = 6;
                110: f_clkop_div = 5;
                115: f_clkop_div = 5;
                120: f_clkop_div = 5;
                125: f_clkop_div = 5;
                130: f_clkop_div = 5;
                135: f_clkop_div = 4;
                140: f_clkop_div = 4;
                default: f_clkop_div = -1;
            endcase
        end
    endfunction

    function integer f_clkop_cphase;
        input integer f;
        begin
            case (f)
                16:  f_clkop_cphase = 18;
                20:  f_clkop_cphase = 15;
                24:  f_clkop_cphase = 11;
                25:  f_clkop_cphase = 11;
                30:  f_clkop_cphase = 9;
                35:  f_clkop_cphase = 8;
                40:  f_clkop_cphase = 7;
                45:  f_clkop_cphase = 6;
                48:  f_clkop_cphase = 6;
                50:  f_clkop_cphase = 5;
                55:  f_clkop_cphase = 5;
                60:  f_clkop_cphase = 4;
                65:  f_clkop_cphase = 4;
                66:  f_clkop_cphase = 4;
                70:  f_clkop_cphase = 4;
                75:  f_clkop_cphase = 4;
                80:  f_clkop_cphase = 3;
                85:  f_clkop_cphase = 3;
                90:  f_clkop_cphase = 3;
                95:  f_clkop_cphase = 3;
                100: f_clkop_cphase = 2;
                105: f_clkop_cphase = 2;
                110: f_clkop_cphase = 2;
                115: f_clkop_cphase = 2;
                120: f_clkop_cphase = 2;
                125: f_clkop_cphase = 2;
                130: f_clkop_cphase = 2;
                135: f_clkop_cphase = 2;
                140: f_clkop_cphase = 1;
                default: f_clkop_cphase = -1;
            endcase
        end
    endfunction

    function integer f_clkfb_div;
        input integer f;
        begin
            case (f)
                16:  f_clkfb_div = 5;
                20:  f_clkfb_div = 4;
                24:  f_clkfb_div = 1;
                25:  f_clkfb_div = 1;
                30:  f_clkfb_div = 6;
                35:  f_clkfb_div = 7;
                40:  f_clkfb_div = 8;
                45:  f_clkfb_div = 9;
                48:  f_clkfb_div = 15;
                50:  f_clkfb_div = 2;
                55:  f_clkfb_div = 11;
                60:  f_clkfb_div = 12;
                65:  f_clkfb_div = 13;
                66:  f_clkfb_div = 21;
                70:  f_clkfb_div = 14;
                75:  f_clkfb_div = 3;
                80:  f_clkfb_div = 16;
                85:  f_clkfb_div = 17;
                90:  f_clkfb_div = 18;
                95:  f_clkfb_div = 19;
                100: f_clkfb_div = 4;
                105: f_clkfb_div = 21;
                110: f_clkfb_div = 22;
                115: f_clkfb_div = 23;
                120: f_clkfb_div = 24;
                125: f_clkfb_div = 5;
                130: f_clkfb_div = 26;
                135: f_clkfb_div = 27;
                140: f_clkfb_div = 28;
                default: f_clkfb_div = -1;
            endcase
        end
    endfunction

    localparam integer CLKI_DIV     = f_clki_div(freq);
    localparam integer CLKOP_DIV    = f_clkop_div(freq);
    localparam integer CLKOP_CPHASE = f_clkop_cphase(freq);
    localparam integer CLKFB_DIV    = f_clkfb_div(freq);

    // CONSTANT condition (required by Yosys for generate-if)
    localparam integer SUPPORTED =
        (CLKI_DIV >= 0) && (CLKOP_DIV >= 0) && (CLKOP_CPHASE >= 0) && (CLKFB_DIV >= 0);

`ifndef SYNTHESIS
    initial begin
        if (!SUPPORTED) begin
            $display("ERROR: pll.v: unknown/unsupported freq=%0d", freq);
            $finish;
        end
    end
`endif

    generate
        if (SUPPORTED) begin : gen_pll
            (* ICP_CURRENT="12" *) (* LPF_RESISTOR="8" *) (* MFG_ENABLE_FILTEROPAMP="1" *) (* MFG_GMCREF_SEL="2" *)
            EHXPLLL #(
                .PLLRST_ENA("DISABLED"),
                .INTFB_WAKE("DISABLED"),
                .STDBY_ENABLE("DISABLED"),
                .DPHASE_SOURCE("DISABLED"),
                .OUTDIVIDER_MUXA("DIVA"),
                .OUTDIVIDER_MUXB("DIVB"),
                .OUTDIVIDER_MUXC("DIVC"),
                .OUTDIVIDER_MUXD("DIVD"),
                .CLKOP_ENABLE("ENABLED"),
                .CLKOP_FPHASE(0),
                .FEEDBK_PATH("CLKOP"),

                .CLKI_DIV(CLKI_DIV),
                .CLKOP_DIV(CLKOP_DIV),
                .CLKOP_CPHASE(CLKOP_CPHASE),
                .CLKFB_DIV(CLKFB_DIV)
            ) pll_i (
                .RST(1'b0),
                .STDBY(1'b0),
                .CLKI(clki),
                .CLKOP(clko),
                .CLKFB(clko),
                .CLKINTFB(),
                .PHASESEL0(1'b0),
                .PHASESEL1(1'b0),
                .PHASEDIR(1'b1),
                .PHASESTEP(1'b1),
                .PHASELOADREG(1'b1),
                .PLLWAKESYNC(1'b0),
                .ENCLKOP(1'b0),
                .LOCK(locked)
            );
        end else begin : gen_unknown
            UNKNOWN_FREQUENCY unknown_frequency();
            assign clko   = 1'b0;
            assign locked = 1'b0;
        end
    endgenerate

endmodule
/* verilator lint_on TIMESCALEMOD */
