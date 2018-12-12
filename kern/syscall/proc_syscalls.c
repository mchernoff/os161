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

/*
 * Process-related syscalls.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <lib.h>
#include <machine/trapframe.h>
#include <clock.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <pid.h>
#include <syscall.h>
#include <addrspace.h>
#include <vfs.h>
#include <limits.h>
#include <kern/fcntl.h>

/*
 * sys_getpid
 * love easy syscalls. :)
 */
int
sys_getpid(pid_t *retval)
{
	*retval = curproc->p_pid;
	return 0;
}

/*
 * sys__exit()
 *
 * The process-level work (exit status, waking up waiters, etc.)
 * happens in proc_exit(). Then call thread_exit() to make our thread
 * go away too.
 */
__DEAD
void
sys__exit(int status)
{
	proc_exit(_MKWAIT_EXIT(status));
	thread_exit();
}

/*
 * sys_fork
 *
 * create a new process, which begins executing in fork_newthread().
 */

static
void
fork_newthread(void *vtf, unsigned long junk)
{
	struct trapframe mytf;
	struct trapframe *ntf = vtf;

	(void)junk;

	/*
	 * Now copy the trapframe to our stack, so we can free the one
	 * that was malloced and use the one on our stack for going to
	 * userspace.
	 */

	mytf = *ntf;
	kfree(ntf);

	enter_forked_process(&mytf);
}

int
sys_fork(struct trapframe *tf, pid_t *retval)
{
	struct trapframe *ntf;
	int result;
	struct proc *newproc;

	/*
	 * Copy the trapframe to the heap, because we might return to
	 * userlevel and make another syscall (changing the trapframe)
	 * before the child runs. The child will free the copy.
	 */

	ntf = kmalloc(sizeof(struct trapframe));
	if (ntf==NULL) {
		return ENOMEM;
	}
	*ntf = *tf;

	result = proc_fork(&newproc);
	if (result) {
		kfree(ntf);
		return result;
	}
	*retval = newproc->p_pid;

	result = thread_fork(curthread->t_name, newproc,
			     fork_newthread, ntf, 0);
	if (result) {
		proc_unfork(newproc);
		kfree(ntf);
		return result;
	}

	return 0;
}

//Frees a string array
static void kfree_all(char *argv[])
{
	int i;
	for (i=0; argv[i]; i++)
		kfree(argv[i]);
}

//Replaces the current executing program with a newly loaded program image
int 
sys_execv(char* path, char* args[])
{
	struct addrspace *as;
	struct vnode *v;
	int result, argc;
	int *length;
	size_t buflen;
	int i = 0;
	vaddr_t entrypoint, stackptr;
	char *progname = (char*)kmalloc(PATH_MAX); 
	copyinstr((userptr_t)path,progname,PATH_MAX,&buflen);
	char **argv= (char **)kmalloc(sizeof(char*));
	
    if(progname == NULL){
		kfree(argv);
		kfree(progname);
		return ENOMEM;
    }

    while (args[i]!=NULL){
		i++;
    }
    argc = i;
	//Too many arguments
	if(argc > ARG_MAX){	
		kfree(argv);
		kfree(progname);
		return E2BIG;
	}
	length = kmalloc(sizeof(int)*argc);

    if(argv == NULL){
		kfree(length);
		kfree(argv);
		kfree(progname);
		return ENOMEM;
    }
    

    for (i=0; i<argc; i++){
        length[i] = strlen(args[i]);
        length[i]=length[i]+1;
    }
    for (i=0; i<=argc; i++){
		if(i<argc){  
			argv[i]=(char*)kmalloc(length[i]+1);
			if(argv[i]==NULL) {
				
				kfree(length);
				kfree_all(argv);
				kfree(argv);
				kfree(progname);
				return ENOMEM;
			}
            copyinstr((userptr_t)args[i], argv[i], length[i], &buflen);
		}
		else{
			argv[i] = NULL;
		}
	}


	int arglength[argc];
	int arg_pointer[argc];
	int offset=0;
	

	int count;
	for(count = argc-1; count >= 0; count--) {
		arglength[count] = strlen(argv[count])+1;
	}

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		
		kfree(length);
		kfree_all(argv);
		kfree(argv);
		kfree(progname);
		return result;
	}

	/* Create new address space */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		
		kfree(length);
		kfree_all(argv);
		kfree(argv);
		kfree(progname);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

    /* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		kfree(length);
		kfree_all(argv);
		kfree(argv);
		kfree(progname);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

  /* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
	/* thread_exit destroys curthread->t_vmspace */
	
		kfree(length);
		kfree_all(argv);
		kfree(argv);
		kfree(progname);
		return result;
	}

  
    //push arguments onto stack
    for(i = argc-1; i >= 0; i--) {
		offset=(arglength[i] + (4-(arglength[i]%4)));
		stackptr = stackptr - offset;
		copyoutstr(argv[i], (userptr_t)stackptr, (size_t)arglength[i], &buflen);
		arg_pointer[i] = stackptr;
	}

	//allocate memory in stack;
	arg_pointer[argc] = (int)NULL;
	i = argc;
	while(i>=0){
		stackptr = stackptr - 4;
		copyout(&arg_pointer[i] ,(userptr_t)stackptr, sizeof(arg_pointer[i]));
		i--;
	}
	
	kfree(length);
	kfree_all(argv);
	kfree(argv);
	kfree(progname);
	// Warp to user mode.

	enter_new_process(argc, (userptr_t)stackptr,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);
	panic("enter_new_process should not return\n");
	return EINVAL;
}

/*
 * sys_waitpid
 * just pass off the work to the pid code.
 */
int
sys_waitpid(pid_t pid, userptr_t retstatus, int flags, pid_t *retval)
{
	int status;
	int result;

	result = pid_wait(pid, &status, flags, retval);
	if (result) {
		return result;
	}

	if (retstatus != NULL) {
		result = copyout(&status, retstatus, sizeof(int));
	}
	return result;
}
