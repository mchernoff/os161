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

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

#include <machine/vm.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

#define VPN_MAX 452

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void);
void vm_tlbshootdown(const struct tlbshootdown *);

// Lock helpers
int ptlock_acquire(void);
void ptlock_release(void);
int ptlock_do_i_hold(void);
//

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

struct pte {
	vaddr_t vpage;
	paddr_t pframe;
	unsigned npages;
	uint8_t flags;
};

struct fte {
	paddr_t pframe;
	uint8_t flags;
};

#endif /* _VM_H_ */
