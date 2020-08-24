Note for lab1
===

Part 1: PC Bootstrap
---

No code to write in this part, it meanly tells how (farely) modern PC still starts from mimicking its ancestor -- the Intel 8088 (Just like the human embryo!). Two important things to note here:  

1. Intel 8088 uses 20-bit address space, i.e. at most 1 MB of memory. Memory addresses above `0xA0000` (640 KB) are reserved for hardware use. All of these lagacy setup are preserved in 80386.
2. 8088 seems to be a 16-bit machine capable of addressing a 20-bit memory space. This is achieved by its unique address formation procedure: `addr = cs * 16 + ip`.


Part 2: The Bootloader
---

After the necessary initialization procedure, BIOS load OS from disk. In fact, it only loads the bootloader of the OS, which is situated in the first sector (512 B) of the disk. And it's the bootloader itself that eventually load the rest of OS into the memory.  
The 'disk' emulated by Qemu is backed by the file `kernel.img` as illustrated by the following piece of Makefile:

```makefile
QEMUOPTS = -drive file=$(OBJDIR)/kern/kernel.img,index=0,media=disk,format=raw ...
```

which in turn is made using the following command sequence:

```makefile
# How to build the kernel disk image
$(OBJDIR)/kern/kernel.img: $(OBJDIR)/kern/kernel $(OBJDIR)/boot/boot
	@echo + mk $@
	$(V)dd if=/dev/zero of=$(OBJDIR)/kern/kernel.img~ count=10000 2>/dev/null
	$(V)dd if=$(OBJDIR)/boot/boot of=$(OBJDIR)/kern/kernel.img~ conv=notrunc 2>/dev/null
	$(V)dd if=$(OBJDIR)/kern/kernel of=$(OBJDIR)/kern/kernel.img~ seek=1 conv=notrunc 2>/dev/null
	$(V)mv $(OBJDIR)/kern/kernel.img~ $(OBJDIR)/kern/kernel.img
```

The `dd` uses 512 as its default `bs` parameter, which corresponds to the size of a disk sector. Therefore, `dd` is instructed to:
1. Create an empty file with 10000 sectors.
2. Copy the compiled bootloader code into the first sector.
3. Copy the compiled kernel code right after the bootloader (seek=1).
  
So, what does the bootloader actually do? And how does it get compiled?

### What does the bootloader do?

The bootloader is consists of 2 files: `boot/boot.S` and `boot/main.c`, with the former setting up the correct machine state and the latter loading the OS kernel.  

`boot/main.c` is quite straight forward, it first reads the ELF header of the kernel to `0x10000`, and load other parts of the OS according to the information encoded in ELF header, then jump to the entry point of the kernel. We would rather focus on the assembly code from now on.  

The first move the bootloader tries is to disable interrupt enabled by BIOS, interrupt will not be re-enabled until the OS has prepared itself.  

```assembly
.global start
start:
  .code16
  cli
```

When the processor is in the real mode (i.e. 8088 compatible mode), any bits above the 20-th will be discarded in the address calculated by adding `cs` and `ip`. A piece of magic code is used first to get rid of the limitation.  

```assembly
  # Enable A20:
  #   For backwards compatibility with the earliest PCs, physical
  #   address line 20 is tied low, so that addresses higher than
  #   1MB wrap around to zero by default.  This code undoes this.
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60
```

But still, the address is calculated in the awkward `cs * 16 + ip` way. By setting the `PE` bit in `cr0` register, the machine enters the so called 'protected mode', in which the segment register is turned into an index to certain 'Descriptor Table'. The corresponding table entry will decide the base address to add on `ip`, addressing limit and permission bit for current memory access. A simple descriptor table is located at label `gdt:` and its base as well as its length is loaded into gdtr by `lgdt` instruction. The global descriptor table sets up here is simply trivial -- with the base address being 0, logical address is mapped directly to linear address.  

```assembly
  # Switch from real to protected mode, using a bootstrap GDT
  # and segment translation that makes virtual addresses 
  # identical to their physical addresses, so that the 
  # effective memory map does not change during the switch.
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0
```

A small question raises here: For the time being, the processor should have been in protected mode, but the selector `cs` still contains value `0x0000` which points to the first entry in GDT. Won't this breaks any thing?  
It won't, the following words are quoted from the 'Appendix B' of xv6 book:  

>Enabling protected mode does not immediately change how the processor translates logical to physical addresses; it is only when one loads a new value into a segment register that the processor reads the GDT and changes its internal segmentation settings.

The remaining stuff is straight forward: `cs` and the other selectors are loaded with `ljmp` and `mov` respectively, stack pointer is setup for C code, then call into `bootmain` to load the kernel into the memory.  

```assembly
  # Jump to next instruction, but in 32-bit code segment.
  # Switches processor into 32-bit mode.
  ljmp    $PROT_MODE_CSEG, $protcseg

  .code32                     # Assemble for 32-bit mode
protcseg:
  # Set up the protected-mode data segment registers
  movw    $PROT_MODE_DSEG, %ax    # Our data segment selector
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS
  movw    %ax, %ss                # -> SS: Stack Segment
  
  # Set up the stack pointer and call into C.
  movl    $start, %esp
  call bootmain
```

### How does the bootloader get compiled?

Besides the ordinary compilation, the following commands are executed:  

```assembly
$(OBJDIR)/boot/boot: $(BOOT_OBJS)
	@echo + ld boot/boot
	$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $@.out $^
	$(V)$(OBJDUMP) -S $@.out >$@.asm
	$(V)$(OBJCOPY) -S -O binary -j .text $@.out $@
	$(V)perl boot/sign.pl $(OBJDIR)/boot/boot
```

The interesting part is `-Ttext 0x7c00`, which sets the VMA (or link address) of the `text` section. The link address is the memory address the section expects to execute, it may calculate address of labels according to that, and encode them in instructions. If the section ends up not being loaded to that particular address, things might break. Here, the BIOS guarantees that the first sector of code will be loaded to `0x7c00`.  
In this case, label `gdtdesc` and `protcseg` is recalculated during linking, using the supplied link address. To be specific, in `boot.o` we have:  

```
  lgdt    gdtdesc
  1e:	0f 01 16             	lgdtl  (%esi)
  21:	64 00 0f             	add    %cl,%fs:(%edi)
  movl    %cr0, %eax
  24:	20 c0                	and    %al,%al
  orl     $CR0_PE_ON, %eax
  26:	66 83 c8 01          	or     $0x1,%ax
  movl    %eax, %cr0
  2a:	0f 22 c0             	mov    %eax,%cr0
  
  # Jump to next instruction, but in 32-bit code segment.
  # Switches processor into 32-bit mode.
  ljmp    $PROT_MODE_CSEG, $protcseg
  2d:	ea                   	.byte 0xea
  2e:	32 00                	xor    (%eax),%al
  30:	08 00                	or     %al,(%eax)

00000032 <protcseg>:
  ...

00000064 <gdtdesc>:
  ...
```

While in `boot.out` (i.e. the linked version):  

```
  lgdt    gdtdesc
    7c1e:	0f 01 16             	lgdtl  (%esi)
    7c21:	64 7c 0f             	fs jl  7c33 <protcseg+0x1>
  movl    %cr0, %eax
    7c24:	20 c0                	and    %al,%al
  orl     $CR0_PE_ON, %eax
    7c26:	66 83 c8 01          	or     $0x1,%ax
  movl    %eax, %cr0
    7c2a:	0f 22 c0             	mov    %eax,%cr0
  
  # Jump to next instruction, but in 32-bit code segment.
  # Switches processor into 32-bit mode.
  ljmp    $PROT_MODE_CSEG, $protcseg
    7c2d:	ea                   	.byte 0xea
    7c2e:	32 7c 08 00          	xor    0x0(%eax,%ecx,1),%bh

00007c32 <protcseg>:
    ...

00007c64 <gdtdesc>:
    ...
```

Many things changed, of which the most important are the addresses encoded in `lgdt` and `ljmp`. Wrong address would be used by both of these instruction if the load and link address of this section don't match each other. Never the less, those ordinary short `jne` instruction can still work, because they use *relative* offset to decide which address to jump to, this works regardless of the link address.  


Part 3: The Kernel
---

The kernel is linked at `0xF0100000` while loaded to `0x100000`, as is specified by the linker script:  

```
ENTRY(_start)

SECTIONS
{
	/* Link the kernel at this address: "." means the current address */
	. = 0xF0100000;

	/* AT(...) gives the load address of this section, which tells
	   the boot loader where to load the kernel in physical memory */
	.text : AT(0x100000) {
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}

        ...
```

The entry point `_start` is defined in `kern/entry.S`, this assembly thunk is responsible for enabling the paging address translation as well as setting up the kernel stack.  

Before paging is enabled, all use of label address is manually mapped to the corresponding load address with macro `RELOC`. Enabling the paging consists of loading `cr3` with the base address of the page directory and setting the `PG` bit in `cr0`. The page directory (and page tables) used here (i.e. `entry_pgdir`) is statically initialized in `kern/entrypgdir.c`. The page table keeps a map from the low address indentially to itself, so that enabling paging won't break the execution which currently still using a low `eip`.  

```assembly
.globl		_start
_start = RELOC(entry)

.globl entry
entry:
	...

	movl	$(RELOC(entry_pgdir)), %eax
	movl	%eax, %cr3
	# Turn on paging.
	movl	%cr0, %eax
	orl	$(CR0_PE|CR0_PG|CR0_WP), %eax
	movl	%eax, %cr0
```

The very next move after enabling paging is to load the corresponding high address to `eip`, this is done by an indirect jump. Note that ordinary jump cannot be used at here, since the linker encodes such instruction with relative offset, which causes `eip` to retain its low value.  
```assembly
	mov	$relocated, %eax
	jmp	*%eax
relocated:
```

The kernel stack is statically allocated in `.data` section, with its lower end being page-aligned and label `bootstacktop` pointing to its higher end. 

```assembly
.data
###################################################################
# boot stack
###################################################################
	.p2align	PGSHIFT		# force page alignment
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
bootstacktop:
```

The note ends here.
