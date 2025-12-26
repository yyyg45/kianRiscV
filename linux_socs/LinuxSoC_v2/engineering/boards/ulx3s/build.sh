#!/bin/bash
DEVICE=${1:-85k}
make -f Makefile clean
make -f Makefile DEVICE=$DEVICE
openFPGALoader -f --board=ulx3s soc_fpga_top.bit
openFPGALoader -f --board=ulx3s -o $((1024 * 1024 * 6)) ../../bootloader/firmware.bin
