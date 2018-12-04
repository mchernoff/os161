
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

/* Fault handling function called by trap code */
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	UNUSED(faulttype);
	UNUSED(faultaddress);
	return 0;

	//copy most of the implementation from DUMBVM
	// vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	// paddr_t paddr;
	// int i;
	// uint32_t ehi, elo;
	// struct addrspace *as;
	// int spl;

	// faultaddress &= PAGE_FRAME;

	// DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	// switch (faulttype) {
	//     case VM_FAULT_READONLY:
	// 	/* We always create pages read-write, so we can't get this */
	// 	panic("dumbvm: got VM_FAULT_READONLY\n");
	//     case VM_FAULT_READ:
	//     case VM_FAULT_WRITE:
	// 	break;
	//     default:
	// 	return EINVAL;
	// }

	// if (curproc == NULL) {
	// 	/*
	// 	 * No process. This is probably a kernel fault early
	// 	 * in boot. Return EFAULT so as to panic instead of
	// 	 * getting into an infinite faulting loop.
	// 	 */
	// 	return EFAULT;
	// }

	// as = proc_getas();
	// if (as == NULL) {
	// 	/*
	// 	 * No address space set up. This is probably also a
	// 	 * kernel fault early in boot.
	// 	 */
	// 	return EFAULT;
	// }

	// /* Assert that the address space has been set up properly. */
	// KASSERT(as->as_vbase1 != 0);
	// KASSERT(as->as_pbase1 != 0);
	// KASSERT(as->as_npages1 != 0);
	// KASSERT(as->as_vbase2 != 0);
	// KASSERT(as->as_pbase2 != 0);
	// KASSERT(as->as_npages2 != 0);
	// KASSERT(as->as_stackpbase != 0);
	// KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	// KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	// KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	// KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	// KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	// vbase1 = as->as_vbase1;
	// vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	// vbase2 = as->as_vbase2;
	// vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	// stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	// stacktop = USERSTACK;

	// if (faultaddress >= vbase1 && faultaddress < vtop1) {
	// 	paddr = (faultaddress - vbase1) + as->as_pbase1;
	// }
	// else if (faultaddress >= vbase2 && faultaddress < vtop2) {
	// 	paddr = (faultaddress - vbase2) + as->as_pbase2;
	// }
	// else if (faultaddress >= stackbase && faultaddress < stacktop) {
	// 	paddr = (faultaddress - stackbase) + as->as_stackpbase;
	// }
	// else {
	// 	return EFAULT;
	// }

	// /* make sure it's page-aligned */
	// KASSERT((paddr & PAGE_FRAME) == paddr);

	// /* Disable interrupts on this CPU while frobbing the TLB. */
	// spl = splhigh();

	// for (i=0; i<NUM_TLB; i++) {
	// 	tlb_read(&ehi, &elo, i);
	// 	if (elo & TLBLO_VALID) {
	// 		continue;
	// 	}
	// 	ehi = faultaddress;
	// 	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	// 	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	// 	tlb_write(ehi, elo, i);
	// 	splx(spl);
	// 	return 0;
	// }

	// kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	// splx(spl);
	// return EFAULT;

	// //within user mem address
	// if(faultaddress <= MIPS_KSEG0)
	// {
	// 	paddr = find_pgtbl(faultaddress & PAGE_FRAME);

	// 	if(!ISVALID(paddr)) 
	// 	{
	// 		panic("VM: invalid page table fault 0x%x",vir_addr);
	// 		return 0;
	// 	}

	// }
	// }
	// else
	// {

	// }
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
