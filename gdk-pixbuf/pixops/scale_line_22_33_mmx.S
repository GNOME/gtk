	.file	"scale_line_22_33_mmx.S"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16

#ifndef __MINGW32__
	
.globl pixops_scale_line_22_33_mmx
	.type	 pixops_scale_line_22_33_mmx,@function
pixops_scale_line_22_33_mmx:
	
#else
	
.globl _pixops_scale_line_22_33_mmx
_pixops_scale_line_22_33_mmx:
	
#endif
/*
 * Arguments
 *		
 * weights:	 8(%ebp)
 * p:	        12(%ebp)	%esi
 * q1:	        16(%ebp)	
 * q2:	        20(%ebp)	
 * xstep:       24(%ebp)	
 * p_end:       28(%ebp)
 * xinit:       32(%ebp)
 *	
*/

/*
 * Function call entry
 */
	pushl %ebp
	movl %esp,%ebp
	subl $28,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
/* Locals:	
 * int x                      %ebx
 * int x_scaled             -24(%ebp)
 */

/*
 * Setup
 */
/* Initialize variables */	
	movl 32(%ebp),%ebx
	movl 32(%ebp),%edx
	sarl $16,%edx
	movl 12(%ebp),%esi

	cmpl %esi,28(%ebp)
	je   .out

/* Load initial values into %mm1, %mm3 */
	leal (%edx,%edx,2),%edx  # Multiply by 3

	movl 16(%ebp),%edi
	pxor %mm4, %mm4
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm1
	punpcklbw %mm4, %mm1

	movl 20(%ebp),%edi
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm3
	punpcklbw %mm4, %mm3

	addl $65536,%ebx
	movl %ebx,%edx
	sarl $16,%edx

	jmp .newx
	.p2align 4,,7
.loop:
/* int x_index = (x & 0xf000) >> 12 */
	movl %ebx,%eax
	andl $0xf000,%eax
	shrl $7,%eax

	movq (%edi,%eax),%mm4
	pmullw %mm0,%mm4
	movq 8(%edi,%eax),%mm5
	pmullw %mm1,%mm5
	movq 16(%edi,%eax),%mm6
	movq 24(%edi,%eax),%mm7
	pmullw %mm2,%mm6
	pmullw %mm3,%mm7
	paddw %mm4, %mm5
	paddw %mm6, %mm7
	paddw %mm5, %mm7

	psrlw $8, %mm7
	packuswb %mm7, %mm7
	movd %mm7, %eax
	
	movb %al, (%esi)
	shrl $8, %eax
	movw %ax, 1(%esi)
	addl $3, %esi
		
	cmpl %esi,28(%ebp)
	je   .out

/* x += x_step; */
	addl 24(%ebp),%ebx
/* x_scale = x >> 16; */
	movl %ebx,%edx
	sarl $16,%edx

	cmpl %edx,-24(%ebp)
	je   .loop

.newx:
	movl %edx,-24(%ebp)
/*
 * Load the two new values into %mm1, %mm3, move old values into %mm0, %mm2
 */
	movq %mm1, %mm0
	movq %mm3, %mm2
	
	leal (%edx,%edx,2),%edx  # Multiply by 3

	movl 16(%ebp),%edi
	pxor %mm4, %mm4
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm1
	punpcklbw %mm4, %mm1

	movl 20(%ebp),%edi
	movzbl 2(%edi,%edx),%ecx
	shll $16,%ecx
	movzwl (%edi,%edx),%eax
	orl %eax,%ecx
	movd %ecx, %mm3
	punpcklbw %mm4, %mm3
	
	movl 8(%ebp),%edi
	
	jmp .loop

.out:
	movl %esi,%eax
	emms
	leal -40(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
