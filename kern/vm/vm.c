
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
#include <mips/tlb.h>

#define FRAME(x) (firstpaddr + (x * PAGE_SIZE))
#define UNUSED(x) (void)(x)

#define VPN_MAX 452

vaddr_t firstfree;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock tlb_lock = SPINLOCK_INITIALIZER;

uint8_t num_shootdowns;
bool initialized = false;

/* PTE Flags 
5 - Valid
4 - Modified/dirty
3 - Reference (has been read or written)
2 - Readable
1 - Writable
0 - Executable
0-19 - Page frame number
*/	

void vm_bootstrap(void){
	num_shootdowns = 0;
	initialized = true;
}
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/*vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - VPN_MAX * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}*/

	paddr = faultaddress;
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);
	
	
	
	spinlock_acquire(&tlb_lock);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		tlb_write(ehi, elo, i);
		splx(spl);
		spinlock_release(&tlb_lock);
		return 0;
	}

	panic("Ran out of TLB entries - cannot handle page fault\n");
	spinlock_release(&tlb_lock);
	splx(spl);
	return EFAULT;
}


static
paddr_t
alloc_ppages(unsigned long npages)
{

	spinlock_acquire(&stealmem_lock);
	
	paddr_t pa = (ram_stealpages(npages));
	spinlock_release(&stealmem_lock);
	
	return pa;
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealpages(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

vaddr_t alloc_kpages(unsigned npages){
	paddr_t pa;
	struct addrspace* as;
	
	if(!initialized || curproc->p_addrspace == NULL){
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}
	
	as = proc_getas();
	
	pa = alloc_ppages(npages);
	
	unsigned i,j;
	unsigned found = 0;
	for(i = 0; i < PAGE_TABLE_SIZE - npages; i++){
		for(j = 0; j < npages; j++){
			//searches for npages-length block of virtual memory
			if(PTE_VALID(as->pagetable[i+j])){
				break;
			}
			found++;
		}
		if(j == npages){
			//large enough block found -> fills page table
			for(j = 0; j < npages; j++){
				as->pagetable[i+j].flags |= PTE_VALID_FLAG;
				as->pagetable[i+j].pframe |= pa + j*PAGE_SIZE;
			}
			as->pagetable[i].npages = npages;
			return as->pagetable[i].vpage;
		}
	}
	return ENOMEM;
}

void free_kpages(vaddr_t vaddr){
	
	unsigned i;
	unsigned index = VA_TO_PT_INDEX(vaddr);
	KASSERT(index < PAGE_TABLE_SIZE);
	
	if(!initialized || curproc->p_addrspace == NULL){
		paddr_t paddr = KVADDR_TO_PADDR(vaddr);
		ram_returnpage(paddr);
		return;
	}
	
	struct addrspace* as = proc_getas();
	
	spinlock_acquire(&stealmem_lock);
	for(i = 0; i < as->pagetable[index].npages; i++){
		ram_returnpage(vaddr + i*PAGE_SIZE);
	}
	spinlock_release(&stealmem_lock);
	
	lock_acquire(as->pt_lock);
	for(i = 0; i < as->pagetable[index].npages; i++){
		as->pagetable[index+i].flags &= !PTE_VALID_FLAG;
		as->pagetable[index+i].pframe = 0;
	}
	as->pagetable[index].npages = 0;
	lock_release(as->pt_lock);
}

void vm_tlbshootdown_all(void){
	int i;
	uint32_t elo, ehi;
	elo = 0;
	ehi = 0;
	for(i = 0; i < NUM_TLB; i++){
		tlb_write(ehi, elo, i);
	}
}

void vm_tlbshootdown(const struct tlbshootdown * sd){
	int i;
	uint32_t elo, ehi;
	vaddr_t vaddr;
	
	if(num_shootdowns == 16){
		num_shootdowns = 0;
		vm_tlbshootdown_all();
	}
	
	for(i = 0; i < NUM_TLB; i++){
		tlb_read(&elo, &ehi, i);
		vaddr = ehi & TLBHI_VPAGE;
		if(sd->vaddr == vaddr){
			elo &= !TLBLO_VALID;
			tlb_write(ehi, elo, i);
			return;
		}
	}
	panic("tlb shootdown failed with vaddr %x\n", sd->vaddr);
}
