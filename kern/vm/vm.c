
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <mainbus.h>
#include <synch.h>

#define FRAME(x) (firstpaddr + (x * PAGE_SIZE))
#define UNUSED(x) (void)(x)

#define VPN_MAX 452
#define TLB_SIZE 12 //change this later

vaddr_t firstfree;
static paddr_t initialpaddr;  /* address of first free physical page */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

/* PTE Flags 
5 - Valid
4 - Modified/dirty
3 - Reference (has been read or written)
2 - Readable
1 - Writable
0 - Executable
0-19 - Page frame number
*/	

static struct tlb_entry tlb[TLB_SIZE];

void vm_bootstrap(void){
	size_t ramsize;
	UNUSED(tlb);
	ramsize = mainbus_ramsize();
	if (ramsize > 512*1024*1024) {
		ramsize = 512*1024*1024;
	}
	initialpaddr = firstfree - MIPS_KSEG0;
	
}
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	/* TODO */
	UNUSED(faulttype);
	UNUSED(faultaddress);
	panic("fault");
	return EFAULT;
}


static
vaddr_t
getvpages(unsigned long npages)
{

	spinlock_acquire(&stealmem_lock);
	
	vaddr_t va = (ram_stealpages(npages));
	spinlock_release(&stealmem_lock);
	
	return va;
}

vaddr_t alloc_kpages(unsigned npages){
	//paddr_t pa = getppages(npages);
	//vaddr_t va = PADDR_TO_KVADDR(pa);
	vaddr_t va = getvpages(npages);
	return va;
}

void free_kpages(vaddr_t vaddr){
	
	spinlock_acquire(&stealmem_lock);
	
	ram_returnpage(vaddr);
	
	spinlock_release(&stealmem_lock);
}

void vm_tlbshootdown_all(void){
	/* TODO */
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void vm_tlbshootdown(const struct tlbshootdown * shoot){
	/* TODO */
	UNUSED(shoot);
	panic("dumbvm tried to do tlb shootdown?!\n");
}