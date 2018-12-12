#include <types.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <lib.h>
#include <machine/trapframe.h>
#include <clock.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <pid.h>
#include <syscall.h>
#include <addrspace.h>
#include <vfs.h>
#include <limits.h>
#include <kern/fcntl.h>
#include <vm.h>


/*
    The sbrk() system call is needed to support dynamic memory allocation in 
    user programs.
*/
int	
sys_sbrk(intptr_t amount, void **ptr) 
{
    struct addrspace *as;
    //vaddr_t	heap_start;
	vaddr_t	heap_end;

    as = proc_getas();
    heap_end = as->heap_end;
	*ptr = (void*)heap_end;
	
	heap_end += amount;

	as->heap_end = heap_end;

    //more to add

    return 0;
}