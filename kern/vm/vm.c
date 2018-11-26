
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

#define UNUSED(x) (void)(x)

#define VPN_MAX 1048575

vaddr_t firstfree;
static paddr_t firstpaddr;  /* address of first free physical page */
static paddr_t lastpaddr;   /* one past end of last free physical page */
static struct spinlock allocmem_lock = SPINLOCK_INITIALIZER;

void vm_bootstrap(void){
	size_t ramsize;
	ramsize = mainbus_ramsize();
	if (ramsize > 512*1024*1024) {
		ramsize = 512*1024*1024;
	}
	lastpaddr = ramsize;
	firstpaddr = firstfree - MIPS_KSEG0;
}

int vm_fault(int faulttype, vaddr_t faultaddress){
	UNUSED(faulttype);
	UNUSED(faultaddress);
	return 0;
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;
	UNUSED(addr);
	UNUSED(npages);

	spinlock_acquire(&allocmem_lock);

	//allocate memory with page table

	/*size_t size = npages * PAGE_SIZE;

	if (firstpaddr + size > lastpaddr) {
		return 0;
	}

	paddr = firstpaddr;
	firstpaddr += size;*/

	spinlock_release(&allocmem_lock);
	//return addr;
	return 0;
}

vaddr_t alloc_kpages(unsigned npages){
	UNUSED(npages);
	paddr_t pa;
	pa = getppages(npages);
	
	UNUSED(pa);
	return (vaddr_t)0;
}

void free_kpages(vaddr_t addr){
	UNUSED(addr);
	
}

void vm_tlbshootdown_all(void){
	
}

void vm_tlbshootdown(const struct tlbshootdown * shoot){
	UNUSED(shoot);
	
}