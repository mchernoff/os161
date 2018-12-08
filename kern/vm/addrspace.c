/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <machine/vm.h>
#include <vm.h>
#include <spl.h>
#include <mips/tlb.h>
#include <proc.h>
#include <spinlock.h>
#include <synch.h>

#define UNUSED(x) (void)(x)

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

/*	Create a new empty address space. You need to make
 *  sure this gets called in all the right places. You
 *  may find you want to change the argument list. May
 *  return NULL on out-of-memory error.
 */
struct addrspace *
as_create(void)
{
	kprintf("start calling as_create \n");
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	unsigned i;
	for(i = 0; i < PAGE_TABLE_SIZE; i++){
		as->pagetable[i].vpage = firstfree + i*PAGE_SIZE;
		as->pagetable[i].flags = 0;
	}

	as->pt_lock = lock_create("page table lock");
	as->static_start = 0x0;					//initialize Static Segment Start to 0
	as->is_loading_done = false;		//allow load_elf to access address space while calling as_create

	kprintf("finish calling as_create \n");

	return as;
}

/*Create a new address space that is an exact copy of
 *    an old one. Probably calls as_create to get a new
 *    empty address space and fill it in, but that's up to
 *    you.
*/
int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	kprintf("start calling as_copy \n");
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	lock_acquire(old->pt_lock);

	for(size_t i = 0; i < PAGE_TABLE_SIZE; i++)
	{
		//this part need to be changed after we change page table
		
		// struct pte *old_entry =  &(old->pagetable[i]);

		// if(old_entry != NULL)
		// {
		// 	struct pte *new_entry = kmalloc(sizeof (struct pte));
		// 	newas->pagetable[i] = new_entry;
		// }
		// else
		// {
		// 	newas->pagetable[i] = NULL;
		// }
	}

	lock_release(old->pt_lock);

	*ret = newas;

	kprintf("finish calling as_copy \n");
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	
	//tlb_random(0x400000,0x450200);

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

void pte_insert(struct addrspace *as, vaddr_t va, paddr_t pa, uint8_t flags){
	size_t index = VA_TO_PT_INDEX(va);
	as->pagetable[index].flags = flags;
	as->pagetable[index].vpage = va;
	as->pagetable[index].pframe = pa;
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, paddr_t paddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;
	
	if(as->static_start == 0x0){
		as->static_start = vaddr & PAGE_FRAME;
	}

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;
	KASSERT(sz%PAGE_SIZE == 0);
	
	vaddr_t seg_end = vaddr + sz;
	KASSERT(seg_end > as->heap_start);
	
	as->heap_start = seg_end + PAGE_SIZE;
	as->heap_end = as->heap_start;
	
	uint8_t flags = 0;
	if(readable){
		flags |= PTE_CANREAD_FLAG;
	}
	if(writeable){
		flags |= PTE_CANWRITE_FLAG;
	}
	if(executable){
		flags |= PTE_CANEXEC_FLAG;
	}
	
	vaddr_t cur_vaddr = vaddr;
	paddr_t cur_paddr = paddr;
	
	size_t i;
	//uint32_t elo, ehi;
	for(i = 0; i < npages; i++){
		//ehi = cur_vaddr;
		//elo = (cur_paddr&TLBLO_PPAGE) | TLBLO_VALID;
		pte_insert(as, cur_vaddr, cur_paddr, flags);
		cur_vaddr += PAGE_SIZE;
		cur_paddr += PAGE_SIZE;
	}
	
	return 0;
}

/*static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}*/

int
as_prepare_load(struct addrspace *as)
{
	as->is_loading_done = false;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	as->is_loading_done = true;
	as_activate();
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	as->stack = (vaddr_t) USERSTACK - PAGE_SIZE;

	*stackptr = USERSTACK;
	return 0;
}



