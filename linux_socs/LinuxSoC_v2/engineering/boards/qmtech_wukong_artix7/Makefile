###################################################################
# KianV SoC - Vivado Non-Project build + GUI that supports '$' files
###################################################################

NUM_CPUS := $(shell nproc)
PROJ := soc_fpga_top
PART := xc7a100tfgg676-1
CONSTRAINTS := wukong_v3.xdc

# SoC clock in Hz (used for SYSTEM_CLK define + PLL generation)
SYSTEM_FREQUENCY ?= 80000000

# Vivado binary
VIVADO ?= vivado

# Sources
PROJECT_DIR := $(shell pwd)
SOC_DEFINES_VH := $(PROJECT_DIR)/../../defines_soc.vh
include ../sources.mk

# Verilog defines (same as your working batch flow)
DEFINES := -verilog_define SOC_IS_ARTIX7 \
           -verilog_define SDRAM_SIZE=$$((1024*1024*32)) \
           -verilog_define NUM_ENTRIES_ITLB=64 \
           -verilog_define NUM_ENTRIES_DTLB=64 \
           -verilog_define ICACHE_SET=512 \
           -verilog_define DCACHE_SET=512 \
           -verilog_define SYSTEM_CLK=$(SYSTEM_FREQUENCY)

# ================================================================
OPT_STRATEGY ?= PERFORMANCE

ifeq ($(OPT_STRATEGY),PERFORMANCE)
    SYNTH_STRATEGY := Flow_PerfOptimized_high
    IMPL_STRATEGY  := Performance_ExploreWithRemap
    SYNTH_DIRECTIVES := -directive PerformanceOptimized
    OPT_DIRECTIVES   := -directive Explore
    PLACE_DIRECTIVES := -directive ExtraNetDelay_high
    ROUTE_DIRECTIVES := -directive AggressiveExplore
    SYNTH_EXTRAS := -flatten_hierarchy rebuilt -retiming
    OPT_EXTRAS   :=
    PLACE_EXTRAS :=
    ROUTE_EXTRAS :=
else ifeq ($(OPT_STRATEGY),BALANCED)
    SYNTH_STRATEGY := Flow_AreaOptimized_high
    IMPL_STRATEGY  := Performance_Explore
    SYNTH_DIRECTIVES := -directive Default
    OPT_DIRECTIVES   := -directive Default
    PLACE_DIRECTIVES := -directive Default
    ROUTE_DIRECTIVES := -directive Explore
    SYNTH_EXTRAS := -flatten_hierarchy rebuilt -retiming
    OPT_EXTRAS   :=
    PLACE_EXTRAS :=
    ROUTE_EXTRAS :=
else
    SYNTH_STRATEGY := Flow_AreaOptimized_high
    IMPL_STRATEGY  := Area_Explore
    SYNTH_DIRECTIVES := -directive AreaOptimized_high
    OPT_DIRECTIVES   := -directive Default
    PLACE_DIRECTIVES := -directive Default
    ROUTE_DIRECTIVES := -directive Default
    SYNTH_EXTRAS := -flatten_hierarchy full -no_retiming
    OPT_EXTRAS   :=
    PLACE_EXTRAS :=
    ROUTE_EXTRAS :=
endif

.PHONY: all bit gui clean pll
.PHONY: perf balanced area

all: bit
bit: $(PROJ).bit

perf:
	$(MAKE) OPT_STRATEGY=PERFORMANCE bit

balanced:
	$(MAKE) OPT_STRATEGY=BALANCED bit

area:
	$(MAKE) OPT_STRATEGY=AREA bit

# --- PLL generation ---
pll.v: Makefile generate_pll.py
	@echo "Generating PLL for $(SYSTEM_FREQUENCY) Hz..."
	@chmod +x generate_pll.py
	@./generate_pll.py $(SYSTEM_FREQUENCY) > pll.v
	@echo "PLL generated: pll.v"

# --- Batch build (non-project) ---
$(PROJ).bit: project.tcl pll.v $(SRCS) $(CONSTRAINTS)
	$(VIVADO) -mode batch -source project.tcl

# --- GUI open (non-project, works with '$' filenames) ---
gui: gui.tcl pll.v $(SRCS) $(CONSTRAINTS)
	$(VIVADO) -mode gui -source gui.tcl &

# --- Generate batch build script ---
project.tcl: Makefile
	@echo "set_param general.maxThreads $(NUM_CPUS)" > project.tcl
	@echo "read_verilog -sv {$(SOC_DEFINES_VH)}" >> project.tcl
	@echo "read_verilog -sv {pll.v}" >> project.tcl
	@echo "read_verilog -sv {./soc_fpga_top.v}" >> project.tcl
	@echo "read_verilog -sv {$(SRCS)}" >> project.tcl
	@echo "read_xdc {$(CONSTRAINTS)}" >> project.tcl
	@echo "" >> project.tcl
	@echo "puts \"OPT_STRATEGY=$(OPT_STRATEGY) SYNTH=$(SYNTH_STRATEGY) IMPL=$(IMPL_STRATEGY)\"" >> project.tcl
	@echo "synth_design -top $(PROJ) -part $(PART) $(DEFINES) $(SYNTH_DIRECTIVES) $(SYNTH_EXTRAS)" >> project.tcl
	@echo "opt_design $(OPT_DIRECTIVES) $(OPT_EXTRAS)" >> project.tcl
	@echo "place_design $(PLACE_DIRECTIVES) $(PLACE_EXTRAS)" >> project.tcl
	@echo "route_design $(ROUTE_DIRECTIVES) $(ROUTE_EXTRAS)" >> project.tcl
	@echo "report_timing_summary -file timing_summary.rpt" >> project.tcl
	@echo "report_utilization -file utilization.rpt" >> project.tcl
	@echo "set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]" >> project.tcl
	@echo "write_bitstream -force $(PROJ).bit" >> project.tcl
	@echo "exit" >> project.tcl

# --- Generate GUI script (elaborate RTL with the SAME defines) ---
gui.tcl: Makefile
	@echo "set_param general.maxThreads $(NUM_CPUS)" > gui.tcl
	@echo "read_verilog -sv {$(SOC_DEFINES_VH)}" >> gui.tcl
	@echo "read_verilog -sv {pll.v}" >> gui.tcl
	@echo "read_verilog -sv {./soc_fpga_top.v}" >> gui.tcl
	@echo "read_verilog -sv {$(SRCS)}" >> gui.tcl
	@echo "read_xdc {$(CONSTRAINTS)}" >> gui.tcl
	@echo "" >> gui.tcl
	@echo "# RTL elaboration so hierarchy resolves with macros" >> gui.tcl
	@echo "synth_design -rtl -top $(PROJ) -part $(PART) $(DEFINES) $(SYNTH_DIRECTIVES) $(SYNTH_EXTRAS) -name rtl_1" >> gui.tcl
	@echo "puts \"GUI ready (Non-Project). RTL elaborated with defines.\" " >> gui.tcl
	@echo "" >> gui.tcl
	@echo "# --- keep GUI alive ---" >> gui.tcl
	@echo "start_gui" >> gui.tcl
	@echo "wait_on_gui" >> gui.tcl

clean:
	rm -rf *.bit *.bin *.jou *.log *.str *.dcp *.rpt .Xil pll.v project.tcl gui.tcl vivado*
