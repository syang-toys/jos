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