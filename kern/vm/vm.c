
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
#include <machine/vm.h>
#include <mainbus.h>
#include <synch.h>
#include <mips/tlb.h>

#define FRAME(x) (firstpaddr + (x * PAGE_SIZE))
#define UNUSED(x) (void)(x)

#define VPN_MAX 452
#define VM_STACKPAGES	256
#define USER_STACK_LIMIT (0x80000000 - (VM_STACKPAGES * PAGE_SIZE)) 

vaddr_t firstfree;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock tlb_lock = SPINLOCK_INITIALIZER;
struct lock *pagetable_lock = NULL;

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

//Inserts new entry into page table tree
struct pte*
pagetable_insert(struct pte* table, vaddr_t vaddr, paddr_t paddr, int npages, uint8_t flags){
	if(table == NULL){
		table = kmalloc(sizeof(struct pte));
		table->vaddr = vaddr;
		table->paddr = paddr;
		table->npages = npages;
		table->flags = flags;
		table->right = NULL;
		table->left = NULL;
	}
	else if(vaddr > table->vaddr){
		table->right = pagetable_insert(table->right, vaddr, paddr, npages, flags);
	}
	else if(vaddr < table->vaddr){
		table->left = pagetable_insert(table->left, vaddr, paddr, npages, flags);
	}
	return table;
}

//Searches recursively for page table entry
struct pte*
pagetable_find(struct pte* table, vaddr_t vaddr){
	if(table == NULL){
		return NULL;
	}
	if(table->vaddr == vaddr){
		return table;
	}
	else if(vaddr > table->vaddr){
		return pagetable_find(table->right,vaddr);
	}
	else{
		return pagetable_find(table->left,vaddr);
	}
	return NULL;
}

//Copies tree structure of page table
struct pte*
pagetable_copy(struct pte* oldtable){
	if(oldtable == NULL){
		return NULL;
	}
	struct pte* newtable = kmalloc(sizeof(struct pte));
	newtable->vaddr = oldtable->vaddr;
	newtable->paddr = oldtable->paddr;
	newtable->npages = oldtable->npages;
	newtable->flags = oldtable->flags;
	newtable->right = pagetable_copy(oldtable->right);
	newtable->left = pagetable_copy(oldtable->left);
	return newtable;
}

//Finds entry to replace node being deleted
static struct pte*
replacementEntry(struct pte* table){
	if(table->right == NULL){
		return table->left;
	}
	struct pte* current = table->right;
	while(current->left != NULL){
		current = current->left;
	}
	return current;
}

//Removes page table entry from tree
struct pte* 
pagetable_delete(struct pte* table, vaddr_t vaddr){
	if(table == NULL){
		return table;
	}
	if(table->vaddr > vaddr){
		table->right = pagetable_delete(table->right,vaddr);
	}
	else if(table->vaddr > vaddr){
		table->left = pagetable_delete(table->left,vaddr);
	}
	else{
		struct pte *tmp;
		if (table->left == NULL) 
        { 
            tmp = table->right; 
            kfree(table); 
            return tmp; 
        } 
        else if (table->right == NULL) 
        { 
            tmp = table->left; 
            kfree(table); 
            return tmp; 
        } 
		tmp = replacementEntry(table);
		table->vaddr = tmp->vaddr;
		table->paddr = tmp->paddr;
		table->npages = tmp->npages;
		table->flags = tmp->flags;
		
		table->right = pagetable_delete(table->right, tmp->vaddr);
	}
	return table;
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	vaddr_t fulladdress = faultaddress;
	faultaddress &= PAGE_FRAME;

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

	switch (faulttype) {
	    case VM_FAULT_READONLY:
			return EFAULT;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
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

	//paddr = faultaddress;
	
	if(faultaddress >= MIPS_KSEG1){
		panic("shouldn't be in this address space");
	}
	else if(faultaddress >= MIPS_KSEG0){
		paddr = KVADDR_TO_PADDR(faultaddress);
	}
	else{
		
		struct pte* entry = pagetable_find(as->pagetable, faultaddress);
		if(entry == NULL){
			//Process not set up yet
			if(as->stack == 0xdeadbeef){
				paddr = ram_stealpages(1);
			}
			else if(faultaddress < as->stack && faultaddress > USER_STACK_LIMIT)
			{
				as->stack -= PAGE_SIZE;
				paddr = ram_stealpages(1);
			}
			else if(fulladdress < as->heap_end && fulladdress >= as->heap_start)
			{
				paddr = ram_stealpages(1);
			}
			else if(faultaddress < as->heap_start && faultaddress >= as->static_start)
			{
				paddr = ram_stealpages(1);
			}
			else{
				return EFAULT;
			}
			uint8_t flags = PTE_CANREAD_FLAG | PTE_CANWRITE_FLAG | PTE_VALID_FLAG;
			as->pagetable = pagetable_insert(as->pagetable, faultaddress, paddr, 1, flags);
		}
		else{
			paddr = entry->paddr;
		}
	}
	
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);
	
	
	
	spinlock_acquire(&tlb_lock);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if((ehi & TLBHI_VPAGE) == faultaddress){
			spinlock_release(&tlb_lock);
			splx(spl);
			return 0;
		}
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

	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_random(ehi, elo);
	spinlock_release(&tlb_lock);
	splx(spl);
	return 0;
}

//new physical page allocation method
paddr_t
alloc_ppages(unsigned long npages)
{

	spinlock_acquire(&stealmem_lock);
	
	paddr_t pa = (ram_stealpages(npages));
	spinlock_release(&stealmem_lock);
	
	return pa;
}

//old dumbvm physical page allocation method
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
	vaddr_t va;
	
	if(!initialized || curproc->p_addrspace == NULL){
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		va = PADDR_TO_KVADDR(pa);
	}
	else{
		pa = alloc_ppages(npages);
		va = PADDR_TO_KVADDR(pa);
	}
	
	return va;
	
	return ENOMEM;
}

void free_kpages(vaddr_t vaddr){
	struct addrspace *as;
	
	as = proc_getas();
	
	if(as != NULL){
		as->pagetable = pagetable_delete(as->pagetable, vaddr);
	}
	

	uint32_t ehi, elo;
	int i, spl;
	spinlock_acquire(&tlb_lock);
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if ((ehi & TLBHI_VPAGE) == vaddr) {
			tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
			break;
		}
	}
	spinlock_release(&tlb_lock);
	splx(spl);
	
	if(vaddr >= MIPS_KSEG0){
		spinlock_acquire(&stealmem_lock);
		paddr_t paddr = KVADDR_TO_PADDR(vaddr);
		ram_returnpage(paddr);
		spinlock_release(&stealmem_lock);
	}
	else{
		panic("failed returning mem");
	}
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
