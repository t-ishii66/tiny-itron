	.file	"video.c"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16
.globl video_init
	.type	 video_init,@function
video_init:
	pushl %ebp
	movl %esp,%ebp
	movw $0,c_y
	movw $0,c_x
	movw $24,c_y_max
	movw $0,scrolltop
	movzwl scrolltop,%eax
	pushl %eax
	pushl $12
	call video_set_6845
	addl $8,%esp
.L1:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe1:
	.size	 video_init,.Lfe1-video_init
.data
	.align 4
	.type	 lock,@object
	.size	 lock,4
lock:
	.long 0
	.local	k.4
	.comm	k.4,4,4
	.align 4
	.type	 i.5,@object
	.size	 i.5,4
i.5:
	.long 0
	.local	j.6
	.comm	j.6,4,4
.section	.rodata
.LC0:
	.string	"abc"
.LC1:
	.string	"                           \n"
.text
	.align 16
.globl video_test1
	.type	 video_test1,@function
video_test1:
	pushl %ebp
	movl %esp,%ebp
	movl $0,i.5
.L3:
	jmp .L5
	.align 16
	jmp .L4
	.align 16
.L5:
	movl $0,j.6
.L6:
	movl j.6,%eax
	cmpl %eax,i.5
	jg .L9
	jmp .L7
	.align 16
.L9:
	pushl $.LC0
	call video_puts
	addl $4,%esp
.L8:
	incl j.6
	jmp .L6
	.align 16
.L7:
	pushl $.LC1
	call video_puts
	addl $4,%esp
	incl i.5
	cmpl $9,i.5
	jle .L10
	movl $0,i.5
.L10:
	nop
	movl $0,k.4
.L11:
	cmpl $649999,k.4
	jle .L14
	jmp .L12
	.align 16
.L14:
.L13:
	incl k.4
	jmp .L11
	.align 16
.L12:
	jmp .L3
	.align 16
.L4:
.L2:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe2:
	.size	 video_test1,.Lfe2-video_test1
.section	.rodata
.LC2:
	.string	"\n\n\n\n\n\n\n\n\nabc"
.text
	.align 16
.globl video_test2
	.type	 video_test2,@function
video_test2:
	pushl %ebp
	movl %esp,%ebp
	subl $8,%esp
	pushl $.LC2
	call video_puts
	addl $4,%esp
.L16:
	jmp .L18
	.align 16
	jmp .L17
	.align 16
.L18:
	movzwl scrolltop,%eax
	pushl %eax
	pushl $12
	call video_set_6845
	addl $8,%esp
	pushl $80
	pushl $12
	call video_set_6845
	addl $8,%esp
	addw $80,scrolltop
	movl $0,-8(%ebp)
.L19:
	cmpl $649999,-8(%ebp)
	jle .L22
	jmp .L20
	.align 16
.L22:
.L21:
	incl -8(%ebp)
	jmp .L19
	.align 16
.L20:
	pushl $160
	pushl $12
	call video_set_6845
	addl $8,%esp
	jmp .L16
	.align 16
.L17:
.L15:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe3:
	.size	 video_test2,.Lfe3-video_test2
	.align 16
.globl video_puts
	.type	 video_puts,@function
video_puts:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	movb $35,-1(%ebp)
.L24:
	movl 8(%ebp),%eax
	cmpb $0,(%eax)
	jne .L26
	jmp .L25
	.align 16
.L26:
	movl 8(%ebp),%eax
	movsbl (%eax),%edx
	pushl %edx
	incl 8(%ebp)
	call video_putc
	addl $4,%esp
	cmpw $20,c_y
	jne .L27
	movsbl -1(%ebp),%eax
	pushl %eax
	call video_putc
	addl $4,%esp
.L27:
	jmp .L24
	.align 16
.L25:
.L23:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe4:
	.size	 video_puts,.Lfe4-video_puts
	.align 16
.globl video_putc
	.type	 video_putc,@function
video_putc:
	pushl %ebp
	movl %esp,%ebp
	subl $8,%esp
	pushl %ebx
	movl 8(%ebp),%ebx
	movb %bl,-1(%ebp)
	movl $753664,-8(%ebp)
	movzwl scrolltop,%eax
	movl %eax,%edx
	addl %edx,%eax
	movzwl c_y,%ecx
	movl %ecx,%edx
	sall $2,%edx
	addl %ecx,%edx
	movl %edx,%ecx
	sall $5,%ecx
	addl %ecx,%eax
	movzwl c_x,%ecx
	movl %ecx,%edx
	addl %edx,%ecx
	addl %ecx,%eax
	addl %eax,-8(%ebp)
	cmpb $10,-1(%ebp)
	jne .L29
	incw c_y
	movw $0,c_x
	jmp .L30
	.align 16
.L29:
	movl -8(%ebp),%eax
	movb -1(%ebp),%dl
	movb %dl,(%eax)
	movl -8(%ebp),%eax
	incl %eax
	movb $2,(%eax)
	incw c_x
	cmpw $79,c_x
	jbe .L31
	movw $0,c_x
	incw c_y
.L31:
.L30:
	cmpw $24,c_y
	jbe .L32
	call video_scroll
	movw $24,c_y
.L32:
.L28:
	movl -12(%ebp),%ebx
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe5:
	.size	 video_putc,.Lfe5-video_putc
	.align 16
	.type	 video_set_6845,@function
video_set_6845:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%ebx
	movl 12(%ebp),%esi
	movw %bx,-2(%ebp)
	movw %si,-4(%ebp)
	call video_wait
	movw -2(%ebp),%ax
	movb $0,%ah
	movzwl %ax,%edx
	pushl %edx
	pushl $980
	call outb
	addl $8,%esp
	movw -4(%ebp),%ax
	shrw $8,%ax
	movl %eax,%edx
	movb $0,%dh
	movzwl %dx,%eax
	pushl %eax
	pushl $981
	call outb
	addl $8,%esp
	movzwl -2(%ebp),%eax
	leal 1(%eax),%edx
	movzbl %dl,%eax
	pushl %eax
	pushl $980
	call outb
	addl $8,%esp
	movw -4(%ebp),%ax
	movb $0,%ah
	movzwl %ax,%edx
	pushl %edx
	pushl $981
	call outb
	addl $8,%esp
.L33:
	leal -12(%ebp),%esp
	popl %ebx
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe6:
	.size	 video_set_6845,.Lfe6-video_set_6845
	.align 16
	.type	 video_scroll,@function
video_scroll:
	pushl %ebp
	movl %esp,%ebp
	addw $80,scrolltop
	incw c_y_max
	movzwl scrolltop,%eax
	pushl %eax
	pushl $12
	call video_set_6845
	addl $8,%esp
	cmpw $2000,scrolltop
	jbe .L35
	movw $0,c_x
	call video_wait
	pushl $4160
	pushl $753664
	pushl $757824
	call video_copy
	addl $12,%esp
	movw $24,c_y
	movw $24,c_y_max
	movw $0,scrolltop
	movzwl scrolltop,%eax
	pushl %eax
	pushl $12
	call video_set_6845
	addl $8,%esp
	movw $0,c_x
.L35:
.L34:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe7:
	.size	 video_scroll,.Lfe7-video_scroll
	.align 16
	.type	 video_cursor,@function
video_cursor:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	movzwl c_y,%edx
	movl %edx,%eax
	sall $2,%eax
	addl %edx,%eax
	movl %eax,%edx
	sall $5,%edx
	movw c_x,%cx
	addw %dx,%cx
	movw %cx,-2(%ebp)
	movzwl -2(%ebp),%eax
	pushl %eax
	pushl $14
	call video_set_6845
	addl $8,%esp
.L36:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe8:
	.size	 video_cursor,.Lfe8-video_cursor
	.align 16
	.type	 video_copy,@function
video_copy:
	pushl %ebp
	movl %esp,%ebp
	subl $12,%esp
	pushl %ebx
	movl 16(%ebp),%eax
	movw %ax,-2(%ebp)
	movl 8(%ebp),%edx
	movl %edx,-8(%ebp)
	movl 12(%ebp),%edx
	movl %edx,-12(%ebp)
	movw $0,-4(%ebp)
.L38:
	movw -4(%ebp),%dx
	cmpw %dx,-2(%ebp)
	ja .L41
	jmp .L39
	.align 16
.L41:
	movl -12(%ebp),%edx
	movl -8(%ebp),%ecx
	movb (%ecx),%bl
	movb %bl,(%edx)
	incl -12(%ebp)
	incl -8(%ebp)
.L40:
	incw -4(%ebp)
	jmp .L38
	.align 16
.L39:
.L37:
	movl -16(%ebp),%ebx
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe9:
	.size	 video_copy,.Lfe9-video_copy
	.align 16
	.type	 video_wait,@function
video_wait:
	pushl %ebp
	movl %esp,%ebp
.L42:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe10:
	.size	 video_wait,.Lfe10-video_wait
	.local	c_x
	.comm	c_x,2,2
	.local	c_y
	.comm	c_y,2,2
	.local	c_y_max
	.comm	c_y_max,2,2
	.local	scrolltop
	.comm	scrolltop,2,2
	.ident	"GCC: (GNU) 2.7.2.3"
