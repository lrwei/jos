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
.text;									\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps;							\
.data;									\
	.long name

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.text;									\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps;							\
.data;									\
	.long name

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

.data
.align 2
.global _vectors
_vectors:

TRAPHANDLER_NOEC(_vector_0, T_DIVIDE)
TRAPHANDLER_NOEC(_vector_1, T_DEBUG)
TRAPHANDLER_NOEC(_vector_2, T_NMI)
TRAPHANDLER_NOEC(_vector_3, T_BRKPT)
TRAPHANDLER_NOEC(_vector_4, T_OFLOW)
TRAPHANDLER_NOEC(_vector_5, T_BOUND)
TRAPHANDLER_NOEC(_vector_6, T_ILLOP)
TRAPHANDLER_NOEC(_vector_7, T_DEVICE)
     TRAPHANDLER(_vector_8, T_DBLFLT)
TRAPHANDLER_NOEC(_vector_9, 9 /* T_COPROC */)
     TRAPHANDLER(_vector_10, T_TSS)
     TRAPHANDLER(_vector_11, T_SEGNP)
     TRAPHANDLER(_vector_12, T_STACK)
     TRAPHANDLER(_vector_13, T_GPFLT)
     TRAPHANDLER(_vector_14, T_PGFLT)
TRAPHANDLER_NOEC(_vector_15, 15 /* T_RES */)
TRAPHANDLER_NOEC(_vector_16, T_FPERR)
TRAPHANDLER_NOEC(_vector_17, T_ALIGN)
     TRAPHANDLER(_vector_18, T_MCHK)
     TRAPHANDLER(_vector_19, T_SIMDERR)

.space	GAPSIZE

TRAPHANDLER_NOEC(_vector_48, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */

.text
_alltraps:
	# Construct trapframe on kernel stack. Use *pushl* for
	# both %ds, %es to fit data layout of struct Trapframe.
	pushl	%ds
	pushl	%es
	pushal

	# Set registers not automatically loaded from IDT.
	movw	$GD_KD, %ax
	movw	%ax, %ds
	movw	%ax, %es

	# Pass parameter and call trap().
	pushl	%esp
	call	trap

	# So that return address still belongs to `kern/trapentry.S`.
spin:	jmp	spin

.global sysenter_handler
.type sysenter_handler, @function
sysenter_handler:
	pushl	%edi
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%eax
	call	syscall

	# Both %esi and %ebp are callee-saved register.
	# SYSEXIT will load %eip and %esp from %edx and %ecx respectively.
	movl	%esi, %edx
	movl	%ebp, %ecx
	sysexit
