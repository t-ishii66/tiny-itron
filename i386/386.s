	.file	"386.c"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16
.globl bootstrap
	.type	 bootstrap,@function
bootstrap:
	pushl %ebp
	movl %esp,%ebp
#	call hello
	movb 0x0e, %ah
	movb 0x39, %al
	movb 0x02, %bl
	int	0x10	
.L1:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe1:
	.size	 bootstrap,.Lfe1-bootstrap
	.ident	"GCC: (GNU) 2.7.2.3"
