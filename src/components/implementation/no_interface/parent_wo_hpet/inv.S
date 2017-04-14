/* Use the passed in stack in arg4 */

#include <cos_asm_simple_stacks.h>

#define RET_CAP (1 << 16)
.text
.globl __inv_hierchild_serverfn 
.type  __inv_hierchild_serverfn, @function
__inv_hierchild_serverfn:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
	call hierchild_serverfn
        movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;
