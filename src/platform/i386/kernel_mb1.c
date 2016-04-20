/* only one VGA driver at a time.. */
#undef ENABLE_VGA
#define ENABLE_SCREEN

#define ENABLE_SERIAL
#define ENABLE_TIMER

#include "assert.h"
#include "kernel.h"
#include "multiboot.h"
#include "string.h"
#include "boot_comp.h"
#include "mem_layout.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>
#include <inv.h>

struct mem_layout glb_memlayout;
#define FOREVER	while(1)

#ifdef ENABLE_VGA
#define printcv(x,y,format, ...) do { settextcolor(x,y); printv(format, ##__VA_ARGS__); } while(0)
#else 
#ifdef ENABLE_SCREEN
#define printv(format, ...) puts((unsigned char *)(format))
#define printcv(x,y,format, ...) do { settextcolor(x,y); puts((unsigned char *)(format)); } while(0)
#else
#define printv(format, ...)
#define printcv(x,y,format, ...)
#endif
#endif

static int
xdtoi(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return 0;
}

static u32_t
hextol(const char *s)
{
	int i, r = 0;

	for (i = 0 ; i < 8 ; i++) {
		r = (r * 0x10) + xdtoi(s[i]);
	}

	return r;
}

extern u8_t end; 		/* from the linker script */

void
kern_memory_setup(struct multiboot *mb, u32_t mboot_magic)
{
	struct multiboot_mod_list *mods;
	struct multiboot_mem_list *mems;
	unsigned int i, wastage = 0;
	glb_memlayout.allocs_avail = 1;


	if (mboot_magic != MULTIBOOT_EAX_MAGIC) {
		printcv(4, 15, "Not started from a multiboot loader! Act:%x - Req:%x\n", mboot_magic, MULTIBOOT_EAX_MAGIC);
		FOREVER ;
		die("Not started from a multiboot loader! Act:%x - Req:%x\n", mboot_magic, MULTIBOOT_EAX_MAGIC);
	}
	if ((mb->flags & MULTIBOOT_FLAGS_REQUIRED) != MULTIBOOT_FLAGS_REQUIRED) {
		printcv(1, 15, "Multiboot flags include %x but are missing one of %x\n", 
		    mb->flags, MULTIBOOT_FLAGS_REQUIRED);
		FOREVER ;

		die("Multiboot flags include %x but are missing one of %x\n", 
		    mb->flags, MULTIBOOT_FLAGS_REQUIRED);
	}


	mods = (struct multiboot_mod_list *)mb->mods_addr;
	mems = (struct multiboot_mem_list *)mb->mmap_addr;
	if (mb->mods_count != 1) {
		printcv(2, 15, "Boot failure: expecting a single module to load, received %d instead.\n", mb->mods_count);
		FOREVER ;

		die("Boot failure: expecting a single module to load, received %d instead.\n", mb->mods_count);
	}


	glb_memlayout.kern_end = &end + PAGE_SIZE;
	assert((unsigned int)&end % RETYPE_MEM_NPAGES*PAGE_SIZE == 0);
	printcv(3, 15, "System memory info from multiboot (end 0x%x):\n", &end);
	printk("System memory info from multiboot (end 0x%x):\n", &end);
	printk("\tModules:\n");
	for (i = 0 ; i < mb->mods_count ; i++) {
		struct multiboot_mod_list *mod = &mods[i];
		
		printk("\t- %d: [%08x, %08x)", i, mod->mod_start, mod->mod_end);
		/* These values have to be higher-half addresses */
		glb_memlayout.mod_start = chal_pa2va((paddr_t)mod->mod_start);
		glb_memlayout.mod_end   = chal_pa2va((paddr_t)mod->mod_end);

		glb_memlayout.bootc_vaddr = (void*)hextol((char*)mod->cmdline);
		assert(((char*)mod->cmdline)[8] == '-');
		glb_memlayout.bootc_entry = (void*)hextol(&(((char*)mod->cmdline)[9]));
		printk(" @ virtual address %p, _start = %p.\n", 
		       glb_memlayout.bootc_vaddr, glb_memlayout.bootc_entry);
	}
	glb_memlayout.kern_boot_heap = mem_boot_start();
	printcv(0, 15, "\tMemory regions:\n");
	printk("\tMemory regions:\n");
	for (i = 0 ; i < mb->mmap_length/sizeof(struct multiboot_mem_list) ; i++) {
		struct multiboot_mem_list *mem = &mems[i];
		
		printk("\t- %d (%s): [%08llx, %08llx)\n", i, 
		       mem->type == 1 ? "Available" : "Reserved ", mem->addr, mem->addr + mem->len);

		printv("\t- %d (%s): [%08llx, %08llx)\n", i, 
		       mem->type == 1 ? "Available" : "Reserved ", mem->addr, mem->addr + mem->len);
	}
	/* FIXME: check memory layout vs. the multiboot memory regions... */

	/* Validate the memory layout. */
	assert(mem_kern_end()   == mem_bootc_start());
	assert(mem_bootc_end()  <= mem_boot_start());
	assert(mem_boot_start() >= mem_kmem_start());
	assert(mem_kmem_start() == mem_bootc_start());
	assert(mem_kmem_end()   >= mem_boot_end());
	assert(mem_kmem_end()   <= mem_usermem_start());
	assert(mem_bootc_entry() - mem_bootc_vaddr() <= mem_bootc_end() - mem_bootc_start());

	wastage += mem_boot_start()    - mem_bootc_end();
	wastage += mem_usermem_start() - mem_kmem_end();

	printk("\tAmount of wasted memory due to layout is %u MB + 0x%x B\n", 
	       wastage>>20, wastage & ((1<<20)-1));

	printv("\tAmount of wasted memory due to layout is %u MB + 0x%x B\n", 
	       wastage>>20, wastage & ((1<<20)-1));

	assert(STK_INFO_SZ == sizeof(struct cos_cpu_local_info));
}

void 
kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)
{
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
	unsigned long max;

	tss_init();
	gdt_init();
	idt_init();
#ifdef ENABLE_SERIAL
	serial_init();
#endif
#ifdef ENABLE_CONSOLE
	console_init();
#endif
#ifdef ENABLE_VGA
	vga_init();
#else
#ifdef ENABLE_SCREEN
	init_video();
#endif
#endif
	printcv(8, 15, "In Composite kernel.. (kmain) - header:%x, magic: %x, esp: %x\n", mboot, mboot_magic, esp);
	max = MAX((unsigned long)mboot->mods_addr, 
		  MAX((unsigned long)mboot->mmap_addr, (unsigned long)(chal_va2pa(&end))));
	kern_paging_map_init((void*)(max + PGD_SIZE));
	kern_memory_setup(mboot, mboot_magic);
	printcv(9, 15, "About to go deep..\n");

	chal_init();
	cap_init();
       	ltbl_init();
       	retype_tbl_init();
       	comp_init();
       	thd_init();
       	inv_init();
	paging_init();

	kern_boot_comp();
#ifdef ENABLE_TIMER
	timer_init(100);
#endif
	FOREVER ;
	kern_boot_upcall();

	/* should not get here... */
	khalt(); 
}

void
khalt(void)
{
	printk("Shutting down...\n");
	asm("mov $0x53,%ah");
	asm("mov $0x07,%al");
	asm("mov $0x001,%bx");
	asm("mov $0x03,%cx");
	asm("int $0x15");
}
