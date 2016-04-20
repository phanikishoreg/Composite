/* boot.S - bootstrap the kernel */
/* Copyright (C) 1999, 2001, 2010  Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define ASM_FILE	1
#include "multiboot2.h"

/* Composite headers */
#include <shared/cos_config.h>
#include "chal_asm_inc.h"

/* C symbol format. HAVE_ASM_USCORE is defined by configure.  */
#ifdef HAVE_ASM_USCORE
# define EXT_C(sym)			_ ## sym
#else
# define EXT_C(sym)			sym
#endif

/* The size of our stack (4KB).  */
#define STACK_SIZE			0x1000

/* The flags for the Multiboot header.  */
#ifdef __ELF__
# define AOUT_KLUDGE 0
#else
# define AOUT_KLUDGE MULTIBOOT_AOUT_KLUDGE
#endif
	
.align 4
.text

.globl	loader, _loader
/*loader = _loader*/
loader = (_loader - COS_MEM_KERN_START_VA)
	jmp	_loader

	/* Align 64 bits boundary.  */
	.align	8
	
	/* Multiboot header.  */
multiboot_header:
	/* magic */
	.long	MULTIBOOT2_HEADER_MAGIC
	/* ISA: i386 */
	.long   MULTIBOOT_ARCHITECTURE_I386
	/* Header length.  */
	.long	multiboot_header_end - multiboot_header
	/* checksum */
	.long	-(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))
	/* request for specific headers..*/
ega_tag_start:
	.short MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long ega_tag_end - ega_tag_start
	.long MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED
	.align 8
ega_tag_end:
	/* MMAP Address Information header */
/*mmap_tag_start:
	.short MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long mmap_tag_end - mmap_tag_start
	.long MULTIBOOT_TAG_TYPE_MMAP
	.align 8
mmap_tag_end:
*/	/* Modules Information header */
/*module_tag_start:
	.short MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
	.short MULTIBOOT_HEADER_TAG_OPTIONAL
	.long module_tag_end - module_tag_start
	.long MULTIBOOT_TAG_TYPE_MODULE
	.align 8
module_tag_end:
*/	/* End of multiboot header */
	.short MULTIBOOT_HEADER_TAG_END
	.short 0
	.long 8
multiboot_header_end:
multiboot_entry:
_loader:
	/* Initialize the stack pointer.  */
	/*movl	$(stack + STACK_SIZE - STK_INFO_OFF), %esp*/
	movl $(stack + STACK_SIZE), %esp

	/* Backup %eax to %ecx */
	movl	%eax, %ecx

	/* Other loader.S stuff. */
	movl	%cr4, %eax
	orl	$((1 << 4) | (1 << 7)), %eax
/*	orl	$(1 << 4), %eax*/
	movl	%eax, %cr4
	/*movl	$(COS_MEM_KERN_START_VA), %eax*/
	movl	$(boot_comp_pgd - COS_MEM_KERN_START_VA), %eax
	movl	%eax, %cr3

	/* Turn on Paging. */
	movl	%cr0, %eax
	orl	$(1 << 31), %eax
	movl	%eax, %cr0
	cli

	/* Reset EFLAGS.  */
	pushl	$0
	popf

	/* Push stack pointer. not used now in kmain */
	pushl	%esp
	/* Push the magic value.  */
	pushl	%ecx
	/* Push the pointer to the Multiboot information structure.  */
	pushl	%ebx

	/*call kmain*/
	/* Empty return value as we're jmping, not calling */
	pushl	$0
	
	/* Jmp to kmain */
	mov	$kmain, %eax
	jmp	*%eax

/* Our stack area.  */
.comm	stack, STACK_SIZE, 32

