
#define EMIF_START  0x02000000
#define EMIF_SIZE   0x02000000
#define IRAM_START  0x00000000
#define DRAM_START  0x00010000
#define RAM_SIZE    0x00008000

.globl main
.globl _start
.globl __bss_start
.globl __bss_end

.text

_start:
        .org 0

        b  begin

        nop
        nop
        nop
        nop
        nop
        nop
        nop

        .org 0x20
begin:
        /* Enable ITCM */
        mrc  p15,#0,r0,c9,c1,#1
        mov  r0, #0x1
        mcr  p15,#0,r0,c9,c1,#1

        /* Enable DTCM */
        mrc  p15,#0,r0,c9,c1,#0
        ldr  r0, =DRAM_START
        orr  r0, r0, #0x1D
        mcr  p15,#0,r0,c9,c1,#0

        nop
        mrs  r0, cpsr
        bic  r0, r0, #0x1F /* CLEAR MODES */
        orr  r0, r0, #0x13 /* SET SUPERVISOR mode */
        orr  r0, r0, #0xC0 /* Disable FIQ and IRQ */
        msr  cpsr, r0
        nop

        /* Set the IVT to low memory, leave MMU & caches disabled */
        mrc  p15,#0,r0,c1,c0,#0
        bic  r0,r0,#0x00002300
        bic  r0,r0,#0x00000087
        orr  r0,r0,#0x00000002
        orr  r0,r0,#0x00001000
        mcr  p15,#0,r0,c1,c0,#0
        nop

        mov  r0, #0
        ldr  r1, =__bss_start
        ldr  r2, =__bss_end
clear_bss_loop:
        str  r0, [r1], #4
        cmp  r2, r1
        bne  clear_bss_loop

        ldr  sp, =(DRAM_START + RAM_SIZE - 8) /* 8-byte alignment for ABI compliance */

reper:
		mov  r1, pc
        ldr  r2, =reper
        ldr  r0, =main
        add  r0, r0, r1
        sub  r0, r0, r2
        mov  pc, r0
		b .
