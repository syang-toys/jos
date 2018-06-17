// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	 if (!((err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW))) {
		panic("no write permission and not a COW page!\n");
		return;
	}
	
	addr = ROUNDDOWN(addr, PGSIZE);
	envid_t envid = sys_getenvid();
	if((r=sys_page_alloc(envid, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0) {
		panic("syscall fault: page alloc %e\n", r);
	}
	memcpy(PFTEMP, addr, PGSIZE);
	if ((r = sys_page_map(envid, PFTEMP, envid, addr, PTE_P | PTE_U | PTE_W)) < 0) {
        panic("syscall fault: page map %e\n", r);
	}
    if ((r = sys_page_unmap(envid, PFTEMP)) < 0) {
        panic("syscall fault: page unmap %e\n", r);
	}
	return;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
    void *vaddr = (void *)(pn * PGSIZE);
	// LAB 4: Your code here.
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
        if ((r = sys_page_map(0, vaddr, envid, vaddr, PTE_P | PTE_U | PTE_COW)) < 0) {
            return r;
		}
        if ((r = sys_page_map(0, vaddr, 0, vaddr, PTE_P | PTE_U | PTE_COW)) < 0) {
            return r;
		}
    } else if ((r = sys_page_map(0, vaddr, envid, vaddr, PTE_P | PTE_U)) < 0) {
        return r;
	}
    return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
    int pn;	
	int r;
	// page fault handler
	set_pgfault_handler(pgfault);
	// fork
	envid = sys_exofork();
	
	if (envid > 0) {
		// copy
		for (pn = PGNUM(UTEXT); pn<PGNUM(USTACKTOP); pn++) {
            if ((uvpd[pn >> 10] & PTE_P) && (uvpt[pn] & PTE_P)) {
                if ((r = duppage(envid, pn)) < 0) {
					panic("duppage: %e\n", r);
				}
			}
		}
		if ((r=sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0) {
			panic("syscall page alloc failed\n: %e\n", r);
		}
		// copy page fault handler
		extern void _pgfault_upcall(void);
		if ((r=sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0) {
			panic("syscall env set pgfault upcall failed: %e\n", r);
		}
		// runnable
		if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0) {
			panic("syscall env set status failed: %e\n", r);			
		}
	} else if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
	}
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
