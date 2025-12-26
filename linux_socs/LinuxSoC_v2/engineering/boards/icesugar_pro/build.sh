time (make -f Makefile clean && make -f Makefile && icesprog soc.bit)
icesprog -o $((1024 * 1024 * 6)) ../../bootloader/firmware.bin
