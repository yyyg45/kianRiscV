time (make -f Makefile clean && make -f Makefile && openFPGALoader -b icepi-zero soc.bit)
openFPGALoader -b icepi-zero -f -o $((1024 * 1024 * 6)) ../../bootloader/firmware.bin
