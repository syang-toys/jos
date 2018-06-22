
## Question1

> Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

Since `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel. It's in a true mode now and the address in `mpentry.S` need to changed to low address in order to make it run successfully. And `MPBOOTPHYS` actually do that.

## Question2

> It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

Although there is only one CPU can run at a time, each CPU needs a separate stack to store trap frames. If the kernel stack is shared, when an interupt occurs, the hardware pushes the trap frames, while other process enter and then it will mess up.

## Question3

> In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

Because we map the `envs` in `mem_init()`, and each `env`'s pgdir is a copy of `kern_pgdir`. so it's right to addressing swtich.

## Question4

> Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

It's necessary tp save the environment otherwise if the process enters again, it doesn't know the environments before and where to exectue the code next.

And we can find the code is in `trap.c`: `curenv->env_tf = *tf;`

## Description

The jos lab4 is a complex lab that we need to finish three parts. In Part A, we should support multi CPUs and `Round Robin` schedule. Then in Part B, we should implement `Copy-On-Write Fork`, the basic function of an os. Finally we need to implement preemptive multitasking and basic IPC in Part C.

### Part A

First implementing `mmio_map_region` to allow CPU access `LAPIC`, then modify our implementation of `page_init` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list.

Second, modify `mem_init_mp` to map per-CPU stacks starting at `KSTACKTOP`:

```C
static void
mem_init_mp(void)
{
	uintptr_t kstacktop_i;
    for(int i = 0; i < NCPU; i++){
        kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
        boot_map_region(kern_pgdir, kstacktop_i - KSTKSIZE, KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
    }
}
```

The next task is implementing `trap_init_percpu()` to initialize the TSS and TSS descriptor for the BSP, and add `lock_kernel()` and `unlock_kernel()` in a appropriate position.

And we should implement `Round Robin` schedule, search from current process and find out the first process whose status is `ENV_RUNNABLE` and run it.

```C
void
sched_yield(void)
{
	struct Env *idle = curenv;
    size_t idx = idle != NULL ? ENVX(idle->env_id) : 0;
	size_t i = 0;
    for (; i < NENV; i++, idx=(idx+1)%NENV) {
        if (envs[idx].env_status == ENV_RUNNABLE) {
            env_run(&envs[idx]);
            return;
        }
    }
    if (idle && idle->env_status == ENV_RUNNING) {
        env_run(idle);
        return;
    }
	sched_halt();
}
```

The last task in Part A is implementing the system calls described above in [`kern/syscall.c`](./kern/syscall.c): `sys_exofork` / `sys_env_set_status` / `sys_page_alloc` / `sys_page_map` / `sys_page_unmap`


### Part B

In this part, we should implement `COW Fork`, but what we do before is deal with `page fault` in user environment.

First, implement `sys_env_set_pgfault_upcall`:

```C
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *env;
    int ret = envid2env(envid, &env, 1);
    if(ret != 0) {
        return ret;
	}
    env->env_pgfault_upcall = func;
    return 0;
}
```

Second, implement the code in `page_fault_handler` in `kern/trap.c` required to dispatch page faults to the user-mode handler:

```C
    ...
    if(curenv->env_pgfault_upcall!=NULL) {
		struct UTrapframe *utf;
		uintptr_t utf_addr;
		if(tf->tf_esp >= UXSTACKTOP - PGSIZE && tf->tf_esp < UXSTACKTOP) {
			utf_addr = tf->tf_esp - sizeof(struct UTrapframe) - 4;
		}
		else{
			utf_addr = UXSTACKTOP - sizeof(struct UTrapframe);
		}

		user_mem_assert(curenv, (void *) utf_addr, sizeof(struct UTrapframe), PTE_W);

		utf = (struct UTrapframe *) utf_addr;
		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_err;
		utf->utf_regs = tf->tf_regs;
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;
		tf->tf_eip = (uintptr_t) curenv->env_pgfault_upcall;
		tf->tf_esp = utf_addr;
		
		env_run(curenv);
	} else {
    ...
    }
```

Third, implement the `_pgfault_upcall routine` in `lib/pfentry.S`:

```asm
_pgfault_upcall:
    pushl %esp
    movl _pgfault_handler, %eax
    call *%eax
    addl $4, %esp
    addl $8, %esp
    movl 0x20(%esp), %eax
    subl $4, 0x28(%esp)
    movl 0x28(%esp), %edx
    movl %eax, (%edx)
    popal
    addl $0x4, %esp
    popfl
    popl %esp
    ret
```

Finish `set_pgfault_handler()` in `lib/pgfault.c`:

```C
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;
	if (_pgfault_handler == 0) {
		if(sys_page_alloc(thisenv->env_id, (void *) (UXSTACKTOP - PGSIZE), PTE_U|PTE_P|PTE_W) != 0){
            panic("set_pgfault_handler: sys_page_alloc failed");
        }
        if(sys_env_set_pgfault_upcall(thisenv->env_id, _pgfault_upcall) != 0){
            panic("set_pgfault_handler: sys_env_set_pgfault_upcall failed");
        }
	}

	_pgfault_handler = handler;
}
```

After finish those, we can finally try to implement code about `COW Fork`, and the what we need to implement is `fork`, `duppage` and `pgfault`:

- [fork.c](lib/fork.c)


### Part C

First we should register IPC gate in `trapentry.S` and change `env_alloc`

```asm
    TRAPHANDLER_SUPER(irq_offset_0, IRQ_OFFSET, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_1, IRQ_OFFSET + 1, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_2, IRQ_OFFSET + 2, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_3, IRQ_OFFSET + 3, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_4, IRQ_OFFSET + 4, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_5, IRQ_OFFSET + 5, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_6, IRQ_OFFSET + 6, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_7, IRQ_OFFSET + 7, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_8, IRQ_OFFSET + 8, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_9, IRQ_OFFSET + 9, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_10, IRQ_OFFSET + 10, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_11, IRQ_OFFSET + 11, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_12, IRQ_OFFSET + 12, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_13, IRQ_OFFSET + 13, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_14, IRQ_OFFSET + 14, 0, 0);
	TRAPHANDLER_SUPER(irq_offset_15, IRQ_OFFSET + 15, 0, 0);
```


```C
	e->env_tf.tf_eflags |= FL_IF;
```

Then, we should modify `trap_dispatch()`:

```C
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
       lapic_eoi();
       sched_yield();
       return;
    }
```

Finally, we should implement `sys_ipc_recv` and `sys_ipc_try_send` in `kern/syscall.c` and `ipc_recv` and `ipc_send` functions in `lib/ipc.c`, the file is below:

- [syscall.c](./kern/syscall.c)
- [ipc.c](./lib/ipc.c) 