.section .init
.global _entry
_entry:
    /* Stack initialisieren */
    la sp, _stack_top
    addi sp, sp, -16

    /* Global Pointer setzen (für C-ABI / .sdata-Zugriffe) */
    //la gp, __global_pointer$

    /* .text von Flash nach RAM kopieren */
    la t0, _text_load
    la t1, _text_start
    la t2, _text_end
1:
    beq t1, t2, 2f
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j 1b

2:  /* .data von Flash nach RAM kopieren */
    la t0, _data_load
    la t1, _data_start
    la t2, _edata
3:
    beq t1, t2, 4f
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j 3b

4:  /* .bss nullen */
    la t1, _bss_start
    la t2, _ebss
5:
    beq t1, t2, 6f
    sw zero, 0(t1)
    addi t1, t1, 4
    j 5b

6:
    /* Cache & Pipeline synchronisieren */
    fence.i
    fence

    /* Optional: Interruptvektoren setzen, falls verwendet */
    /* la t0, _vectors
       csrw mtvec, t0 */

    /* main() aufrufen */
    la t0, main
    jalr zero, t0, 0

halt:
    j halt

