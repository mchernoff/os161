
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

#define KVADDR_TO_PADDR(kvaddr) ((kvaddr)-MIPS_KSEG0)

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
static struct pte vpt[VPN_MAX]; 
static struct tlb_entry tlb[TLB_SIZE];

void vm_bootstrap(void){
	size_t ramsize;
	UNUSED(tlb);
	
	ramsize = mainbus_ramsize();
	if (ramsize > 512*1024*1024) {
		ramsize = 512*1024*1024;
	}
	initialpaddr = firstfree - MIPS_KSEG0;
	
	unsigned i;
	for(i = 0; i < VPN_MAX; i++){
		vpt[i].vpage = ((i<<12) + initialpaddr + MIPS_KSEG0);
		vpt[i].pframe = 0;
		vpt[i].flags = 0;
	}
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
	
	//unsigned i,j;
	//unsigned long found = 0;
	
	/*for(i = 0; i < VPN_MAX; i++){
		for(j = 0; j < npages; j++){
			//searches for npages-length block of virtual memory
			if(PTE_VALID(vpt[i+j])){
				break;
			}
			found++;
		}
		if(j == npages){
			//large enough block found -> fills page table
			for(j = 0; j < npages; j++){
				vpt[i+j].flags = vpt[i+j].flags | PTE_VALID_FLAG;
				vpt[i+j].pframe = ram_stealmem(1);
			}
			spinlock_release(&stealmem_lock);
			kprintf("Allocating memory va: %x pa: %x idx: %d\n", vpt[i].vpage, vpt[i].pframe, i);
			return PADDR_TO_KVADDR(vpt[i].pframe);
		}
	}*/
	vaddr_t va = (ram_stealpages(npages));
	spinlock_release(&stealmem_lock);
	
	return va;
	
	//panic("Ran out of memory");
	//return PADDR_TO_KVADDR(0);
}

/*
static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}*/

vaddr_t alloc_kpages(unsigned npages){
	//paddr_t pa = getppages(npages);
	//vaddr_t va = PADDR_TO_KVADDR(pa);
	vaddr_t va = getvpages(npages);
	return va;
}

void free_kpages(vaddr_t vaddr){
	UNUSED(vaddr);/*
	paddr_t addr = KVADDR_TO_PADDR(vaddr);
	kprintf("Freeing memory %x %x\n", vaddr, addr);
	
	spinlock_acquire(&stealmem_lock);
	
	unsigned index = (vaddr - MIPS_KSEG0 - initialpaddr)>>12;
	kprintf("Freeing memory va: %x pa: %x idx: %d\n", vpt[index].vpage, vpt[index].pframe, index);
	if(index >= VPN_MAX){
		panic("Page table index out of range %d\n", index);
	}
	if(vpt[index].flags == 0){
		panic("Empty page table entry %x %d\n", addr, index);
	}
	
	ram_returnpage(vpt[index].pframe);
	vpt[index].flags = 0;
	vpt[index].pframe = 0;
	
	
	spinlock_release(&stealmem_lock);*/
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