// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace about the kernel", mon_backtrace },
  { "showmappings", "Display information about physical page mappings", mon_showmappings },
  { "setpermissions", "Set, clear, or change the permissions of any mapping", mon_setpermissions },
  { "dumpmemory", "Dump the memory by given virtual or physical address", mon_dumpmemory },
  { "step", "step into", mon_step },
  { "continue", "continue exec", mon_continue },	
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{	
	cprintf("Stack backtrace:\n");
  uint32_t *ebp = (uint32_t *)read_ebp();
	uint32_t *eip = NULL;
	while(ebp) {
		eip = (uint32_t *)*(ebp + 1);
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp, eip, *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5), *(ebp + 6));
		struct Eipdebuginfo info;
		debuginfo_eip((uintptr_t)eip, &info);
		cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = (uint32_t *)*(ebp);
	}
	return 0;
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
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
	if(lower_addr != ROUNDDOWN(lower_addr, PGSIZE) || upper_addr != ROUNDUP(upper_addr, PGSIZE)) {
		lower_addr = ROUNDDOWN(lower_addr, PGSIZE);
		upper_addr = ROUNDUP(upper_addr, PGSIZE);
		cprintf("The address is formatted: %08x, %08x\n", lower_addr, upper_addr);
	}
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
  return 0;
}

int mon_setpermissions(int argc, char **argv, struct Trapframe *tf)
{
  /* argc must equals to 3 or more */
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
  if(bits > 7) {
    bits &= 7;
    cprintf("bits are formatted: %03b\n", bits);
  }
  pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, 1);
  *pte = (*pte&0xFFFFFFF8) + bits;
  return 0;
}

int mon_dumpmemory(int argc, char **argv, struct Trapframe *tf)
{
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
	if(flag == 2){
		if(PGNUM(upper_addr) >= npages){
			panic("Wrong!");
		}
		lower_addr = (uintptr_t)KADDR((physaddr_t)lower_addr);
		upper_addr = (uintptr_t)KADDR((physaddr_t)upper_addr);
	}
	for(;lower_addr < upper_addr; lower_addr++) {
		cprintf("address: %08x, value: %02x\n", lower_addr, *((char *)lower_addr)&0xFF);
	}
  return 0;
}

/* lab3 step and continue */

int mon_step(int argc, char **argv, struct Trapframe *tf)
{
		if (tf == NULL || !(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG)) {
      	return 0;
		}   
  	tf->tf_eflags |= FL_TF;
		cprintf("eip at\t%08x\n", tf->tf_eip);
    return 0;
}

int mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	 	if (tf == NULL || !(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG)) {
        return 0;   
	 	}
    tf->tf_eflags &= ~FL_TF;
		return 0;
}
/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("%Cs %Cs %Cs %Cs %Cs %Cs!\n",2, "Welcome", 8, "to", 11, "the", 4, "JOS", 15, "kernel", 6, "monitor");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
