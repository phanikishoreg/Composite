/* Use the passed in stack in arg4 */
#include "vkernel_assembly.h"

#define RET_CAP (1 << 16)

.text
vkern_stub(thd_alloc, __vcos_thd_alloc)
//vkern_stub(pgtblcap_t vmpt;
//arcvcap_t vmrcv;
//tcap_t vmtcp;
//sinvcap_t invthd;
//sinvcap_t invinv;
//sinvcap_t invrcv;
//sinvcap_t invsnd;
//sinvcap_t invcomp;
//sinvcap_t invcaptbl;
//sinvcap_t invpgtbl;

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

//.globl __inv_vkern_thd_alloc
//.type  __inv_vkern_thd_alloc, @function
//__inv_vkern_thd_alloc:
//	movl %ebp, %esp
//	xor %ebp, %ebp
//        pushl %edi
//        pushl %esi
//        pushl %ebx
//        call __vcos_thd_alloc
//        addl $12, %esp
//        movl %eax, %ecx
//        movl $RET_CAP, %eax
//        sysenter;
