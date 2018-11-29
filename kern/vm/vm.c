
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

#define PAGE_SIZE 4096
#define VPN_MAX 452

//PTE helper functions
#define PTE_VALID_FLAG 32
#define PTE_MOD_FLAG 16
#define PTE_REF_FLAG 8
#define PTE_CANREAD_FLAG 4
#define PTE_CANWRITE_FLAG 2
#define PTE_CANEXEC_FLAG 1

#define PTE_VALID(pte) (pte.flags&PTE_VALID_FLAG)
#define PTE_MOD(pte) (pte.flags&PTE_MOD_FLAG)
#define PTE_REF(pte) (pte.flags&PTE_REF_FLAG)
#define PTE_CANREAD(pte) (pte.flags&PTE_CANREAD_FLAG)
#define PTE_CANWRITE(pte) (pte.flags&PTE_CANWRITE_FLAG)
#define PTE_CANEXEC(pte) (pte.flags&PTE_CANEXEC_FLAG)

vaddr_t firstfree;
static paddr_t firstpaddr;  /* address of first free physical page */
static paddr_t lastpaddr;   /* one past end of last free physical page */
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
static struct pte vpt[VPN_MAX]; 

void vm_bootstrap(void){
	size_t ramsize;
	
	ramsize = mainbus_ramsize();
	if (ramsize > 512*1024*1024) {
		ramsize = 512*1024*1024;
	}
	lastpaddr = ramsize;
	firstpaddr = firstfree - MIPS_KSEG0;
	kprintf("last %x first %x\n", (int)lastpaddr, (int)firstpaddr);
	kprintf("ramsize %d\n", (int)(lastpaddr - firstpaddr));
	
	unsigned i;
	for(i = 0; i < VPN_MAX; i++){
		vpt[i].frame = (paddr_t)((i<<12) + firstpaddr);
		vpt[i].flags = 0;
	}
	kprintf("first frame %x\n", vpt[0].frame);
	kprintf("last frame %x\n", vpt[i-1].frame);
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
paddr_t
getppages(unsigned long npages)
{
	
	/* TODO */
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
	/*
	paddr_t addr;
	UNUSED(addr);
	UNUSED(npages);

	
	unsigned i;
	unsigned long found = 0;
	for(i = 0; i < VPN_MAX; i++){
		if(!PTE_VALID(vpt[i])){
			vpt[i].flags = vpt[i].flags | PTE_VALID_FLAG;
			found+=1;
			if(found == 1){
				addr = vpt[i].frame;
			}
			if(npages == found){
				break;
			}
		}
	}
	
	if(npages != found){
		return (vaddr_t)0;
	}
	
	return PADDR_TO_KVADDR(addr);*/
}

vaddr_t alloc_kpages(unsigned npages){
	paddr_t pa;
	pa = getppages(npages);
	
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr){
	/* TODO */
	UNUSED(addr);
	
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