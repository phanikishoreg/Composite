/* Use the passed in stack in arg4 */

#define RET_CAP (1 << 16)

.text
.globl __inv_vkern_fn
.type  __inv_vkern_fn, @function
__inv_vkern_fn:
	movl %ebp, %esp
	xor %ebp, %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        call vkern_api_handler 
        addl $12, %esp
        movl %eax, %ecx
        movl $RET_CAP, %eax
        sysenter;

.globl __inv_vkern_thd_alloc
.type  __inv_vkern_thd_alloc, @function
__inv_vkern_thd_alloc:
	movl %ebp, %esp
	xor %ebp, %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        call __vcos_thd_alloc
        addl $12, %esp
        movl %eax, %ecx
        movl $RET_CAP, %eax
        sysenter;
