include Makefile.src

CC := gcc -m32
LD := ld -m elf_i386
AS := as --32 -g

INCPATH := ../../kernel/include
CFLAGS := -g3 -Werror -ffreestanding -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -nostdlib -mno-red-zone -I$(INCPATH)
LDFLAGS := -nostdlib -fno-builtin -nostartfiles -nostdinc -nodefaultlibs

KERNEL := kernel.img

WARNINGS += -Wall
WARNINGS += -Wcast-align
WARNINGS += -Wformat=2
WARNINGS += -Winit-self
#WARNINGS += -Wmissing-declarations
#WARNINGS += -Wmissing-prototypes
WARNINGS += -Wnested-externs
WARNINGS += -Wno-system-headers
WARNINGS += -Wold-style-definition
WARNINGS += -Wredundant-decls
WARNINGS += -Wsign-compare
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wundef
WARNINGS += -Wvolatile-register-var
WARNINGS += -Wwrite-strings

CFLAGS += $(WARNINGS)

OBJS += kernel.o
OBJS += gdt.o
OBJS += isr.o
OBJS += idt.o
OBJS += vm.o
OBJS += printk.o
OBJS += string.o
OBJS += vtxprintf.o
OBJS += tss.o
OBJS += user.o
OBJS += serial.o
OBJS += timer.o
OBJS += chal.o
OBJS += boot_comp.o
#OBJS += screen.o
#OBJS += console.o
OBJS += vga.o

COS_OBJ += pgtbl.o
COS_OBJ += retype_tbl.o
COS_OBJ += liveness_tbl.o
COS_OBJ += capinv.o
COS_OBJ += measurement.o
COS_OBJ += captbl.o
COS_OBJ += per_cpu.o

LLLLDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

DEPS :=$(patsubst %.o, %.d, $(OBJS))

OBJS += $(COS_OBJ)

all: $(KERNEL) composite_bootsec.img

$(KERNEL): linker.ld $(DEPS) $(OBJS) loader2.o
	$(info |     [LD]   Linking $@)
	$(LD) -T linker.ld loader2.o asm.o entry.o $(OBJS) -o $@

bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LLLLDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
#	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock

composite_bootsec.img: bootblock $(KERNEL)
	dd if=/dev/zero of=xv6.img count=1000
	dd if=bootblock of=xv6.img conv=notrunc
	dd if=$(KERNEL) of=xv6.img seek=1 conv=notrunc

loader2.o: loader2.S interrupt.s entry.S
	$(info |     [AS]   Assembling $@)
	@$(AS) -c -o asm.o interrupt.s
	@$(CC) -c -I$(INCPATH) entry.S
	@$(CC) -c -I$(INCPATH) loader2.S

%.d: %.c
	@$(CC) -M -MT $(patsubst %.d, %.o, $@) $(CFLAGS) $(LDFLAGS) $< -o $@

per_cpu.o: ../../kernel/per_cpu.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

pgtbl.o: ../../kernel/pgtbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

retype_tbl.o: ../../kernel/retype_tbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

liveness_tbl.o: ../../kernel/liveness_tbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

capinv.o: ../../kernel/capinv.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

measurement.o: ../../kernel/measurement.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

captbl.o: ../../kernel/captbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@


%.o: %.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f *.d *.o $(KERNEL)

cp: $(KERNEL) xv6.img
	$(info |     [CP]   Copying native booter to $(TRANS_DIR))
	@cp -f $(KERNEL) .gdbinit qemu.sh qemu2.sh qemu-g.sh ppos.sh xv6.img $(TRANS_DIR)
	@cp runscripts/*.sh $(TRANS_DIR)
