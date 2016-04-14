#ifndef VKERNEL_ASSEMBLY
#define VKERNEL_ASSEMBLY

#define vkern_stub(name, fn) \
.globl name##_inv ;                     \
.type  name##_inv, @function ;          \
name##_inv:                             \
	movl %ebp, %esp;                \
	xor %ebp, %ebp;                 \
        pushl %edi;                     \
        pushl %esi;                     \
        pushl %ebx;                     \
        call fn ; 		        \
        addl $12, %esp;                 \
        movl %eax, %ecx;                \
        movl $RET_CAP, %eax;            \
        sysenter;			\

#endif
