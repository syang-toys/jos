/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */

#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

#define TRAPHANDLER_SUPER(name, num, ec, perm)						\
.text;                                          \
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
    .if ec == 0;                              \
        pushl $0;                           \
    .endif;                                \
	pushl $(num);							\
	jmp _alltraps;                          \
.data;                                       \
    .long num, name, perm
.data 
    .globl trapEntry
    trapEntry:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
	TRAPHANDLER_SUPER(t_divide, T_DIVIDE, 0, 0);
	TRAPHANDLER_SUPER(t_debug, T_DEBUG, 0, 0);
	TRAPHANDLER_SUPER(t_nmi, T_NMI, 0, 0);
	TRAPHANDLER_SUPER(t_brkpt, T_BRKPT, 0, 3);
	TRAPHANDLER_SUPER(t_oflow, T_OFLOW, 0 ,0);
	TRAPHANDLER_SUPER(t_bound, T_BOUND, 0 ,0);
	TRAPHANDLER_SUPER(t_illop, T_ILLOP, 0 ,0);
	TRAPHANDLER_SUPER(t_device, T_DEVICE, 0 ,0);
	TRAPHANDLER_SUPER(t_dblflt, T_DBLFLT, 1, 0);
	TRAPHANDLER_SUPER(t_tss, T_TSS, 1, 0);
	TRAPHANDLER_SUPER(t_segnp, T_SEGNP, 1, 0);
	TRAPHANDLER_SUPER(t_stack, T_STACK, 1, 0);
	TRAPHANDLER_SUPER(t_gpflt, T_GPFLT, 1, 0);
	TRAPHANDLER_SUPER(t_pgflt, T_PGFLT, 1, 0);
	TRAPHANDLER_SUPER(t_fperr, T_FPERR, 0, 0);
	TRAPHANDLER_SUPER(t_align, T_ALIGN, 1, 0);
	TRAPHANDLER_SUPER(t_mchk, T_MCHK, 0, 0);
	TRAPHANDLER_SUPER(t_simderr, T_SIMDERR, 0, 0);
	TRAPHANDLER_SUPER(t_syscall, T_SYSCALL, 0 ,3);
.data
	.long 0, 0, 0

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl %ds
    pushl %es
    pushal
    movw $GD_KD, %ax
    movw %ax, %ds
    movw %ax, %es
    pushl %esp
    call trap

