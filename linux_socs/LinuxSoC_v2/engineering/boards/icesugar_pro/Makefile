###################################################################
# Makefile for Kianv RISC-V SoC on Lattice ECP5 (IceSugar-Pro)
#
# This Makefile automates the FPGA build process for the Kianv RISC-V
# SoC on a Lattice ECP5 platform using open-source tools: Yosys for
# synthesis, NextPNR for place-and-route, and EcpPack for bitstream
# generation.
#
# Key Variables:
# - PROJ: Project name (default: soc)
# - SYSTEM_FREQUENCY: Target system clock frequency in Hz (default: 50 MHz)
# - DEFINES: Defines for SoC configuration, including memory and I/O features
# - SRCS: List of Verilog source files for the SoC
#
# Key Targets:
# - `all`: Runs the entire build process, generating a .bit file
# - `clean`: Removes generated files to reset the build environment
#
###################################################################

PROJ=soc_fpga_top
SYSTEM_FREQUENCY ?= 60000000
DEFINES += -DNUM_ENTRIES_ITLB=16 -DNUM_ENTRIES_DTLB=32 -DICACHE_SET=64 -DDCACHE_SET=64
INCDIRS = -I. -I../.. -I../../include

RM             = rm -rf

LPF_FILE = ./icesugar-pro.lpf
include ../sources.mk

all: ${PROJ}.bit

${PROJ}.json: $(SRCS)
	yosys $(DEFINES) -DSYSTEM_CLK=${SYSTEM_FREQUENCY} -p \
		'read_verilog -sv $(INCDIRS) $^; synth_ecp5 -abc9 -json $@ -top ${PROJ}'

${PROJ}_out.config: ${PROJ}.json ${LPF_FILE}
	nextpnr-ecp5 --freq 350 --timing-allow-fail --router router2 \
		--json ${PROJ}.json --textcfg ${PROJ}_out.config --25k --speed 6 \
		--package CABGA256 --lpf ${LPF_FILE}

${PROJ}.bit: ${PROJ}_out.config
	ecppack --compress --input ${PROJ}_out.config --bit ${PROJ}.bit

clean:
	$(RM) -f ${PROJ}.bit ${PROJ}_out.config ${PROJ}.json

.PHONY: clean

