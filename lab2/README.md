# Lab2

## Question1
> Assuming that the following JOS kernel code is correct, what type should variable `x` have, `uintptr_t` or `physaddr_t`?

```C
    mystery_t x;
    char* value = return_a_pointer();
    *value = 10;
    x = (mystery_t) value;
```

The type of `x` is `uintptr_t`, since all data in kernel using virtual address.

## Question2

> What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

| Entry | Base Virtual Address | Points to (logically) |
|---|---|---|
| 1023 | 0xffc00000 | Page table for top 4MB of phys memory|
| ... | ... | ... |
| 960 | 0xf0000000 | Page table for [0, 4MB) of phys memory |
| 959 | 0xefc00000 | Kernel Stack |
| 958 | 0xef800000 | ? |
| 957 | 0xef400000 | Cur. Page Table |
| 956 | 0xef000000 | User pages |
| ... | ... | ... |
| 2 | 0x00800000 | ? |
| 1 | 0x00400000 | ? |
| 0 | 0x00000000 | ? |

## Question3

> We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

Since a user program shouldn't access kernel' s memory, we use a special mechanism to protect the kernel memory. Here, we use `PTE_U` to set the permission whether user program can access this part of memory.

## Question4

> What is the maximum amount of physical memory that this operating system can support? Why?

Since `sizeof(struct PageInfo) == 8` is true, and as we known, the `pages` is 4MB, so the number of pages is no more than 512K, so the maximum amount of physical memory is `512K * 4KB = 2GB`.

## Question5

> How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

If we actually had the maximum amount of physical memory, we have 512K ptes and 1024 pdes, so the overhead is `512K * 4Bytes + 1024 * 4Bytes + 4MB = 6145KB`. If we want to break down the overhead, we can increase the size of the page. So as the same physical memory, we need less ptes and pdes, so the overhead is broken down.

## Question6

> Revisit the page table setup in `kern/entry.S` and `kern/entrypgdir.c`. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

In `entry.S`, we can find:

```asm
    # Now paging is enabled, but we're still running at a low EIP
    # (why is this okay?).  Jump up above KERNBASE before entering
    # C code.
    mov	$relocated, %eax
    jmp	*%eax
```

The jmp instruction makes the transition to running at an EIP above KERNBASE.

And in `entrypgdir.c`:
```C
__attribute__((__aligned__(PGSIZE)))
pde_t entry_pgdir[NPDENTRIES] = {
	// Map VA's [0, 4MB) to PA's [0, 4MB)
	[0]
		= ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P,
	// Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
	[KERNBASE>>PDXSHIFT]
		= ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P + PTE_W
};
```

As the comments mentioned, we can find that `[0, 4MB)` and `[KERNBASE, KERNBASE + 4MB)` are mapped into the same phys memory `[0, 4MB)`, so it is possible for us to continue executing.

Because BIOS only needs to access to lower address while kernel requires it to access to the higher address memory, this transition is necessary.

## Challenge2

I finish three parts in challenge2:

> Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.

In this part, I implement a function called `mon_showmappings`, the function will show pages according to our given address.

At first, the function will check the arguments:

```C
if(argc < 3) {
    cprintf("Usage: showmappings lower_addr upper_addr\n");
    return 0;
}
char *endptr;
uintptr_t lower_addr = strtol(argv[1], &endptr, 16);
if(*endptr) {
    cprintf("lower_addr format wrong!\n");
    return 0;
}
uintptr_t upper_addr = strtol(argv[2], &endptr, 16);
if(*endptr) {
    cprintf("upper_addr format wrong!\n");
    return 0;
}
if(lower_addr < 0 || upper_addr > 0xFFFFFFFF) {
    cprintf("Error! The address is error!\n");
	return 0;		
}
if(lower_addr > upper_addr){
    cprintf("Error! The lower_addr <= upper_addr doesn't hold!\n");
    return 0;
}
```

We should be sure that all arguments are right and then, we need to align the address if necessary:

```C
if(lower_addr != ROUNDDOWN(lower_addr, PGSIZE) || upper_addr != ROUNDUP(upper_addr, PGSIZE)) {
    lower_addr = ROUNDDOWN(lower_addr, PGSIZE);
	upper_addr = ROUNDUP(upper_addr, PGSIZE);
	cprintf("The address is formatted: %08x, %08x\n", lower_addr, upper_addr);
}
```

And then, we can use `pgdir_walk()` to get the pte, and we can print message by flags in pte.

```C
for(;lower_addr<upper_addr;lower_addr+=PGSIZE){
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)lower_addr, 0);
    cprintf("0x%08x - 0x%08x\t", lower_addr, lower_addr+PGSIZE-1);
    if(pte == NULL || !(*pte & PTE_P)) {
        cprintf("page not mapped!\n");
    } else {
        physaddr_t phys_addr = PTE_ADDR(*pte);
        cprintf("0x%08x - 0x%08x\t", phys_addr, phys_addr+PGSIZE-1);
		if(*pte&PTE_U) {
			cprintf("user\t");			
		} else {
			cprintf("kern\t");
		}
		if(*pte&PTE_W) {
			cprintf("rw\n");
		} else {
			cprintf("ro\n");
        }
    }
}
```

> Explicitly set, clear, or change the permissions of any mapping in the current address space.

In this part, I implement a function called `mon_setpermissions`, the function will set the permission bits by the address and flags we give.

At first, we should check the arguments and align the address the same as what we do in last step:

```C
if(argc < 3) { 
    cprintf("Usage: setpermissions addr permission_bits\n");
    cprintf("permission_bits: U|W|P, from 000 to 111\n");
    return 0;
}
/* read argv */
char *endptr;
uintptr_t addr = strtol(argv[1], &endptr, 16);
if(*endptr) {
    cprintf("address format wrong!\n");
    return 0;
}
int bits = (int)strtol(argv[2], &endptr, 2);
if(*endptr) {
    cprintf("bits format wrong!\n");
    return 0;
}
/* check input and align */
if(addr != ROUNDDOWN(addr, PGSIZE)) {
	addr = ROUNDDOWN(addr, PGSIZE);
	cprintf("The address is formatted: %08x\n", addr);
}
```

Another thing we need to do is checking the permissions bits:

```C
if(bits > 7) {
    bits &= 7;
    cprintf("bits are formatted: %03b\n", bits);
}
```

And then, it's easy to find the pte and set the permissions:

```C
pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, 1);
*pte = (*pte&0xFFFFFFF8) + bits;
```

> Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!

In this part, I implement a function called `mon_dumpmemory`, the function will show value according to our given address.

At first, it's same as last two functions to check the arguments:

```C
if(argc < 4) {
    cprintf("Usage: dumpmemory [V|P]  lower_addr upper_addr\n");
	cprintf("V: virtual address; P: physical address\n");
    return 0;
}
char *endptr;
int flag = 0;	
if(argv[1][0] == 'V'){
    flag = 1;
} else if(argv[1][0] == 'P') {
    flag = 2;
}
if(flag == 0) {
    cprintf("wrong address type\n");
    return 0;
}
uintptr_t lower_addr = strtol(argv[2], &endptr, 16);
if(*endptr) {
    cprintf("lower_addr format wrong!\n");
    return 0;
}
uintptr_t upper_addr = strtol(argv[3], &endptr, 16);
if(*endptr) {
    cprintf("upper_addr format wrong!\n");
    return 0;
}
if(lower_addr < 0 || upper_addr > 0xFFFFFFFF) {
    cprintf("Error! The address is error!\n");
	return 0;
}
```

If the address is physical address, it needs to be converted to virtual address:

```C
if(flag == 2){
	if(PGNUM(upper_addr) >= npages){
		panic("Wrong!");
	}
	lower_addr = (uintptr_t)KADDR((physaddr_t)lower_addr);
	upper_addr = (uintptr_t)KADDR((physaddr_t)upper_addr);
}
```

And then, it's easy to show the value:

```C
for(;lower_addr < upper_addr; lower_addr++) {
	cprintf("address: %08x, value: %02x\n", lower_addr, *((char *)lower_addr)&0xFF);
}
```