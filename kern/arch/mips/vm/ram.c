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
#include <lib.h>
#include <vm.h>
#include <mainbus.h>

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
struct pte pagetable[VPN_MAX];

vaddr_t firstfree;   /* first free virtual address; set by start.S */

static paddr_t firstpaddr;  /* address of first free physical page */
static paddr_t lastpaddr;   /* one past end of last free physical page */

static paddr_t firstvaddr;  /* address of first free physical page */


/*
 * Called very early in system boot to figure out how much physical
 * RAM is available.
 */
void
ram_bootstrap(void)
{
	size_t ramsize;

	/* Get size of RAM. */
	ramsize = mainbus_ramsize();

	/*
	 * This is the same as the last physical address, as long as
	 * we have less than 512 megabytes of memory. If we had more,
	 * we wouldn't be able to access it all through kseg0 and
	 * everything would get a lot more complicated. This is not a
	 * case we are going to worry about.
	 */
	if (ramsize > 512*1024*1024) {
		ramsize = 512*1024*1024;
	}

	lastpaddr = ramsize;

	/*
	 * Get first free virtual address from where start.S saved it.
	 * Convert to physical address.
	 */
	firstpaddr = firstfree - MIPS_KSEG0;
	firstvaddr = firstfree;
	
	unsigned i;
	for(i = 0; i < VPN_MAX; i++){
		pagetable[i].pframe = ((i*PAGE_SIZE) + firstpaddr);
		pagetable[i].vpage = 0;
		pagetable[i].flags = 0;
	}

	kprintf("first address %x\n", firstpaddr);
	kprintf("%uk physical memory available\n",
		(lastpaddr-firstpaddr)/1024);
}

/*
 * This function is for allocating physical memory prior to VM
 * initialization.
 *
 * The pages it hands back will not be reported to the VM system when
 * the VM system calls ram_getsize(). If it's desired to free up these
 * pages later on after bootup is complete, some mechanism for adding
 * them to the VM system's page management must be implemented.
 * Alternatively, one can do enough VM initialization early so that
 * this function is never needed.
 *
 * Note: while the error return value of 0 is a legal physical address,
 * it's not a legal *allocatable* physical address, because it's the
 * page with the exception handlers on it.
 *
 * This function should not be called once the VM system is initialized,
 * so it is not synchronized.
 */
paddr_t
ram_stealmem(unsigned long npages)
{
	size_t size;
	paddr_t paddr;

	size = npages * PAGE_SIZE;

	if (firstpaddr + size > lastpaddr) {
		return 0;
	}

	paddr = firstpaddr;
	firstpaddr += size;

	return paddr;
}

vaddr_t
ram_stealpages(unsigned long npages)
{
	size_t size;

	size = npages * PAGE_SIZE;
	
	unsigned i,j;
	unsigned long found = 0;
	for(i = 0; i < VPN_MAX; i++){
		for(j = 0; j < npages; j++){
			//searches for npages-length block of virtual memory
			if(PTE_VALID(pagetable[i+j])){
				break;
			}
			found++;
		}
		if(j == npages){
			//large enough block found -> fills page table
			for(j = 0; j < npages; j++){
				pagetable[i+j].flags = PTE_VALID_FLAG | pagetable[i].flags;
				pagetable[i+j].vpage = firstvaddr + j*PAGE_SIZE;
			}
			firstvaddr += size;
			return pagetable[i].vpage;
		}
	}
	return 0;
}

void 
ram_returnpage(vaddr_t vaddr)
{
	unsigned i;
	for(i = 0; i < VPN_MAX; i++){
		if(pagetable[i].vpage == vaddr){
			pagetable[i].flags = 0;
			pagetable[i].vpage = 0;
			firstvaddr = vaddr;
		}
	}
	panic("Freeing memory failed");
}

/*
 * This function is intended to be called by the VM system when it
 * initializes in order to find out what memory it has available to
 * manage. Physical memory begins at physical address 0 and ends with
 * the address returned by this function. We assume that physical
 * memory is contiguous. This is not universally true, but is true on
 * the MIPS platforms we intend to run on.
 *
 * lastpaddr is constant once set by ram_bootstrap(), so this function
 * need not be synchronized.
 *
 * It is recommended, however, that this function be used only to
 * initialize the VM system, after which the VM system should take
 * charge of knowing what memory exists.
 */
paddr_t
ram_getsize(void)
{
	return lastpaddr;
}

/*
 * This function is intended to be called by the VM system when it
 * initializes in order to find out what memory it has available to
 * manage.
 *
 * It can only be called once, and once called ram_stealmem() will
 * no longer work, as that would invalidate the result it returned
 * and lead to multiple things using the same memory.
 *
 * This function should not be called once the VM system is initialized,
 * so it is not synchronized.
 */
paddr_t
ram_getfirstfree(void)
{
	paddr_t ret;

	ret = firstpaddr;
	firstpaddr = lastpaddr = 0;
	return ret;
}
