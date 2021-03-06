#include <types.h>
#include <kern/errno.h>
#include <kern/reboot.h>
#include <kern/unistd.h>
#include <lib.h>
#include <spl.h>
#include <clock.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vm.h>
#include <mainbus.h>
#include <vfs.h>
#include <device.h>
#include <syscall.h>
#include <test.h>
#include <version.h>
#include "autoconf.h" 
#include <uio.h>
#include <limits.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <file.h>
#include <copyinout.h>
#include <stat.h>
#include <kern/seek.h>
#include <addrspace.h>
#include <kern/wait.h>

#define UNUSED(x) (void)(x)
void wait_for_parent_process_to_exit(struct proc *proc);
/*
	Opens the file, device, or other kernel object named by the pathname filename. 
	The flags argument specifies how to open the file and should consist of one of

		O_RDONLY	Open for reading only.
		O_WRONLY	Open for writing only.
		O_RDWR		Open for reading and writing.

*/
int	sys_open(const char *filename, int flags, mode_t mode, int *retval)
{

	//check if filename is valid input
	if (filename == NULL) {
		return EFAULT;
	}

	//separate the permission flag from optional flags
	//int extra_flags = flags&252;
	int wr_flags = flags & 3;
	
	//check if flags is valid input
	if (wr_flags != O_RDONLY && wr_flags != O_WRONLY && wr_flags != O_RDWR) {
		return EINVAL;
	}

	//find a space in file table, noticed that 0, 1, 2 are reserved in the file 
	//descriptor table
	int fd;
	for (fd = 3; fd < OPEN_MAX; fd++)
	{
		if (curproc->p_fd[fd] == NULL)
		{
			break;
		}
	}

	//the system file table is full
	if (fd == OPEN_MAX)
	{
		return ENFILE;
	}

	
	//allocates a memory block in kernel for filename and copy that from user-level 
	//address to kernel space
	int result;
	/*char* kfname = kmalloc(NAME_MAX * sizeof(char));
	size_t fnamelength;
	result = copyinstr((const_userptr_t)filename, kfname, NAME_MAX, &fnamelength);

	if (result == 1) {
		lock_release(curproc->p_fd[fd]->file_lock);
		lock_destroy(curproc->p_fd[fd]->file_lock);
		kfree(curproc->p_fd[fd]);
		kfree(kfname);
		return result;
	}*/
	
	//virtual file system open or create a file
	struct vnode *filevnode = NULL;
	result = vfs_open((char*)filename, flags, mode, &filevnode);
	
	if (result != 0 || filevnode == NULL) {
		//kfree(kfname);
		kfree(filevnode);
		return EMFILE;
	}
	
	//allocate memory block for new entry in file table
	curproc->p_fd[fd] = kmalloc(sizeof(struct file_descriptor));
	curproc->p_fd[fd]->file_lock = lock_create(filename);
	lock_acquire(curproc->p_fd[fd]->file_lock);
	
	//set up contents for file descriptor 
	curproc->p_fd[fd]->fvnode = filevnode;
	curproc->p_fd[fd]->flags = flags;
	curproc->p_fd[fd]->offset = 0;
	strcpy(curproc->p_fd[fd]->fname, filename);	
	lock_release(curproc->p_fd[fd]->file_lock);
	//kfree(kfname);
	
	*retval = fd;
	return 0;
}


/*
	Writes up to buflen bytes to the file specified by fd, at the location in the 
	file specified by the current seek position of the file, taking the data from the space 
	pointed to by buf. The file must be open for writing.
*/
int sys_write(int fd, const void *buf, size_t nbytes, int *retval)
{
	//fd is not a valid file descriptor
	if (fd < 0 || curproc->p_fd[fd] == NULL) {
		return EBADF;
	}

	//buf is invalid
	if (buf == NULL) {
		return EFAULT;
	}
	
	//separate the permission flag from optional flags
	//int extra_flags = flags&252;
	int wr_flags = (curproc->p_fd[fd]->flags) & 3;
	
	//if flag is incorrect
	if (wr_flags != O_WRONLY && wr_flags != O_RDWR)
	{
		return EINVAL;
	}

	lock_acquire(curproc->p_fd[fd]->file_lock);		//acquire the lock to the file

	struct uio uio;				//used to manage blocks of data moved around by the kernel
	struct iovec iovec;			//read I/O calls	

	char *buffer = (char*)kmalloc(nbytes);
	uio_kinit(&iovec, &uio, (void*) buf, nbytes, curproc->p_fd[fd]->offset, UIO_WRITE);
	iovec.iov_ubase = (userptr_t)buf;			//user-supplied pointer
	iovec.iov_len = nbytes;						//length of data
	
	int result = VOP_WRITE(curproc->p_fd[fd]->fvnode, &uio);		//write data from uio to file at offset
	if (result != 0) {
		kfree(buffer);
		lock_release(curproc->p_fd[fd]->file_lock);
		return result;
	}

	curproc->p_fd[fd]->offset = uio.uio_offset;
	kfree(buffer);
	*retval = nbytes - uio.uio_resid;
	
	lock_release(curproc->p_fd[fd]->file_lock);

	return 0;
}

int
sys_read(int fd, void *buf, size_t buflen, int* retval)
{
	//fd is not a valid file descriptor
	if (fd < 0 || curproc->p_fd[fd] == NULL) {
		return EBADF;
	}

	//buf is invalid
	if (buf == NULL) {
		return EFAULT;
	}
	
	//separate the permission flag from optional flags
	//int extra_flags = flags&252;
	int wr_flags = (curproc->p_fd[fd]->flags) & 3;
	
	//if flag is incorrect
	if (wr_flags != O_RDONLY && wr_flags != O_RDWR)
	{
		return EINVAL;
	}

	lock_acquire(curproc->p_fd[fd]->file_lock);		//acquire the lock to the file
	
	struct uio uio;				//used to manage blocks of data moved around by the kernel
	struct iovec iovec;			//read I/O calls	

	char *buffer = (char*)kmalloc(buflen);
	uio_kinit(&iovec, &uio, (void*) buf, buflen, curproc->p_fd[fd]->offset, UIO_READ);
	iovec.iov_ubase = (userptr_t)buf;			//user-supplied pointer
	iovec.iov_len = buflen;						//length of data
	
	int result = VOP_READ(curproc->p_fd[fd]->fvnode, &uio);		//read data from uio from file at offset
	if (result != 0) {
		kfree(buffer);
		lock_release(curproc->p_fd[fd]->file_lock);
		return EIO;
	}

	kfree(buffer);
	*retval = buflen - uio.uio_resid;
	
	curproc->p_fd[fd]->offset = uio.uio_offset;
	
	lock_release(curproc->p_fd[fd]->file_lock);

	return 0;
}

int
sys_close(int fd)
{
	//check if fd is valid input
	if (curproc->p_fd[fd] == NULL) {
		return EBADF;
	}
	lock_acquire(curproc->p_fd[fd]->file_lock);


	if (curproc->p_fd[fd]->vnode_reference == 1)
	{
		vfs_close(curproc->p_fd[fd]->fvnode);
	}
	else
	{
		curproc->p_fd[fd]->vnode_reference--;
	}

	//if (curproc->p_fd[fd]->fvnode == NULL)
	//{
	//	kprintf("sys_close mark1.7        \n ");
	//}
	//vfs_close(curproc->p_fd[fd]->fvnode);
	//kprintf("sys_close mark2        \n ");

	//free all memory
	lock_release(curproc->p_fd[fd]->file_lock);
	lock_destroy(curproc->p_fd[fd]->file_lock);
	kfree(curproc->p_fd[fd]);
	curproc->p_fd[fd] = NULL;
	return 0;
}

int
sys_lseek(int fd, off_t pos, int whence, off_t* retval)
{
	off_t new_pos;
	struct stat statbuf;
	
	//fd is not a valid file descriptor
	if (fd < 0 || curproc->p_fd[fd] == NULL) {
		return EBADF;
	}
	
	// If file system is null, node is not seekable file
	if(curproc->p_fd[fd]->fvnode->vn_fs == NULL)
	{
		return ESPIPE;
	}
	lock_acquire(curproc->p_fd[fd]->file_lock);
	
	switch(whence){
		case SEEK_SET:
			new_pos = pos;
			break;
			
		case SEEK_CUR:
			new_pos = pos + curproc->p_fd[fd]->offset;
			break;
			
		case SEEK_END:
			VOP_STAT(curproc->p_fd[fd]->fvnode,&statbuf);
			new_pos = statbuf.st_size + pos;
			break;
			
		//whence is not valid
		default:
			lock_release(curproc->p_fd[fd]->file_lock);
			return EINVAL;
	}
	
	//Offset is negative
	if(new_pos < 0 ){
		lock_release(curproc->p_fd[fd]->file_lock);
		return EINVAL;
	}
	
	curproc->p_fd[fd]->offset = new_pos;
	lock_release(curproc->p_fd[fd]->file_lock);
	
	*retval = new_pos;
	return 0;
}

/*
dup2 clones the file handle oldfd onto the file handle newfd. 
If newfd names an already-open file, that file is closed
*/
int
sys_dup2(int oldfd, int newfd, int* retval)
{
	//oldfd is not a valid file handle, 
	//or newfd is a value that cannot be a valid file handle.
	if (oldfd < 0 || newfd < 0 || oldfd >= OPEN_MAX || newfd >= OPEN_MAX) {
		*retval = -1;
		return EBADF;
	}

	if (curproc->p_fd[oldfd] == NULL)
	{
		return EBADF;
	}

	if (newfd != oldfd)
	{
		curproc->p_fd[newfd] = kmalloc(sizeof(struct file_descriptor));
		curproc->p_fd[newfd]->file_lock = lock_create(curproc->p_fd[oldfd]->fname);

		lock_acquire(curproc->p_fd[newfd]->file_lock);
		lock_acquire(curproc->p_fd[oldfd]->file_lock);

		strcpy(curproc->p_fd[newfd]->fname, curproc->p_fd[oldfd]->fname);
		curproc->p_fd[newfd]->flags = curproc->p_fd[oldfd]->flags;
		curproc->p_fd[newfd]->offset = curproc->p_fd[oldfd]->offset;
		curproc->p_fd[newfd]->fvnode = curproc->p_fd[oldfd]->fvnode;

		*retval = newfd;

		lock_release(curproc->p_fd[oldfd]->file_lock);
		lock_release(curproc->p_fd[newfd]->file_lock);

	}
	return 0;
}

/*
The current directory of the current process is set to the directory named by pathname.
*/
int
sys_chdir(const char *pathname, int *retval)
{
	//pathname was an invalid pointer
	if (pathname == NULL)
	{
		return EFAULT;
	}

	size_t length;
	char *kpathname = (char *)kmalloc(NAME_MAX * sizeof(char));
	copyinstr((const_userptr_t)pathname, kpathname, NAME_MAX, &length);
	int result = vfs_chdir(kpathname);

	if (result == 1)
	{
		*retval = 1;
		return result;
	}
	kfree(kpathname);
	*retval = 0;
	return 0;
}

/*
Get name of current working directory
On success, sys_getcwd returns the length of the data returned.
On error, -1 is returned.
*/
int
sys_getcwd(char *buf, size_t buflen, int *retval)
{
	//buf points to an invalid address
	if (buf == NULL)
	{
		return EFAULT;
	}

	struct uio uio;				//used to manage blocks of data moved around by the kernel
	struct iovec iovec;			//read I/O calls	

	uio_kinit(&iovec, &uio, buf, buflen, 0, UIO_READ);

	iovec.iov_ubase = (userptr_t)buf;
	iovec.iov_len = buflen;

	int result = vfs_getcwd(&uio);

	if (result == 1)
	{
		*retval = -1;
		return result;
	}

	*retval = buflen - uio.uio_resid;
	return 0;
}

// Returns the process ID of the currentprocess
int sys_getpid(int* retval)
{
	*retval = (int)curproc->pid;
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
int sys_execv(char* path, char* args[])
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

// fork duplicates the currently running process. The two copies are identical, 
// except that one (the "new" one, or "child"), has a new, unique process id, 
// and in the other (the "parent") the process id is unchanged.
// The process id must be greater than 0. The two processes do not share memory 
// or open file tables; this state is copied into the new process, and subsequent 
// modification in one process does not affect the other. However, the file 
// handle objects the file tables point to are shared, so, for instance, 
// calls to lseek in one process can affect the other.
int sys_fork(struct trapframe *p_tf, int* retval)
{
	int result;

	// Copy parent's trap frame, and pass it to child thread
	struct trapframe *child_proc_tf;
	child_proc_tf = kmalloc(sizeof(struct trapframe));
	//*child_proc_tf = *p_tf;
	memcpy(child_proc_tf,p_tf,sizeof(struct trapframe));
	//memcpy(p_tf, child_proc_tf,sizeof(struct trapframe));

	if(child_proc_tf == NULL)
	{
		kfree(child_proc_tf);
		return ENOMEM; 
	}

	// Copy parent's address space to child address
	struct addrspace *child_proc_ads;
	result = as_copy(curproc->p_addrspace, &child_proc_ads);
	if(result == 1 || child_proc_ads == NULL || curproc->p_addrspace == NULL)
	{
		kfree(child_proc_tf);
		as_destroy(child_proc_ads);
		return result; 
	}
	// Copy parent's file table into child, also acquire the lock while doing 
	// this
	struct proc *child_proc = proc_create_runprogram("Child");
	//child_proc->parent_proc = curproc;
	//child_proc->p_addrspace = child_proc_ads;

	// if (child_proc == NULL) {
	// 	return -1; 
	// }

	for(int i = 0; i <= OPEN_MAX; i++)
	{
		if(curproc->p_fd[i] != NULL)
		{
			//kprintf("loop p_fd at i = %d \n", i);
			lock_acquire(child_proc->p_fd[i]->file_lock);
			//lock_acquire(curproc->p_fd[i]->file_lock);
			//child_proc->p_fd[i] = curproc->p_fd[i];
		
			if(child_proc->p_fd[i])
			{
				child_proc->p_fd[i]->vnode_reference++;
			}

			if(i <= 2)
			{
				child_proc->p_fd[i]->offset = 0;
			}
			//lock_release(curproc->p_fd[i]->file_lock);
			lock_release(child_proc->p_fd[i]->file_lock);
			
		}
		//kprintf("if statement done");
	}

	if (child_proc == NULL) 
	{
		kfree(child_proc_tf);
		as_destroy(child_proc_ads);
		proc_destroy(child_proc);
		return ENOMEM; 
	}

	// adding this child's pid to parent's child process table
	for(int i = PID_MIN; i < PID_MAX; i++)
	{
		//save the child to parent's child process table
		
		if (!curproc->child_proc_table[i])
		{
			lock_acquire(curproc->child_proc_lock);
			curproc->child_proc_table[i] = child_proc;
			child_proc->parent_proc = curproc;
			lock_acquire(child_proc->proc_exit_lock);
			lock_release(curproc->child_proc_lock);
			break;
		}

		// parent's child process already full, then return ENOMEM
		if (curproc->child_proc_table[i] != child_proc && i == PID_MAX - 1)
		{
			kfree(child_proc_tf);
			as_destroy(child_proc_ads);
			proc_destroy(child_proc);
			decrement_vnode_reference();
			return ENOMEM;
		}
	}

	// Add the child process to the process ID table
	int found = 0;
	lock_acquire(process_table_lock);
	for (int i = PID_MIN; i < PID_MAX; i++)
	{
		//save the child to process id table
		if (!process_table[i])
		{
			process_table[i] = child_proc;
			child_proc->pid = i;
			found = 1;
			break;
		}
	}
	lock_release(process_table_lock);
	// process id table already full, then return ENOMEM
	if(!found){
		kfree(child_proc_tf);
		as_destroy(child_proc_ads);
		proc_destroy(child_proc);
		decrement_vnode_reference();
		return ENOMEM;
	}

	// Create child thread (using thread_fork)
	//kprintf("before forking\n");
	//as_activate(child_proc_ads);
	//result = thread_fork(curthread->t_name, child_proc, &enter_forked_process, child_proc_tf, 1);
	result = thread_fork(curthread->t_name, child_proc, &enter_forked_process, child_proc_tf, (unsigned long)child_proc_ads);
	//kprintf("after forking\n");
	//child_proc->p_addrspace = child_proc_ads;
	if (result) {
		kfree(child_proc_tf);
		as_destroy(child_proc_ads);
		decrement_vnode_reference();
		lock_acquire(process_table_lock);
		process_table[child_proc->pid] = NULL;
		lock_release(process_table_lock);
		proc_destroy(child_proc);
		return result; 
	}

	*retval = child_proc->pid;
	
	// Child returns with 0
	return 0;
}

/*
	Wait for the process specified by pid to exit, and return an encoded 
	exit status in the integer pointed to by status. If that process has 
	exited already, waitpid returns immediately. If that process does not 
	exist, waitpid fails.
*/
int sys_waitpid(int pid, int *proc_status, int options, int *retval)
{
	//if status argument was an invalid pointer
	if (proc_status == NULL)
	{
		return EFAULT;
	}

	*retval = -1;

	//The options argument requested invalid or unsupported options.
	if (options != 0)
	{
		return EINVAL;
	}

	//The pid argument named a nonexistent process
	if (pid < PID_MIN || pid > PID_MAX)
	{
		return ESRCH;
	}

	struct proc *child_proc;
	//find the child process that specified by pid to exit from current process 
	/*lock_acquire(curproc->child_proc_lock);	
	int i;
	for(i = 0; i < PID_MAX; i++)
	{
		if (curproc->child_proc_table[i] && curproc->child_proc_table[i]->pid == pid)
		{
			child_proc = curproc->child_proc_table[i];
			break;
		}
	}
	lock_release(curproc->child_proc_lock);*/
	int i;
	for(i = 0; i < PID_MAX; i++)
	{
		if (process_table[i] != NULL && process_table[i]->pid == pid){
			child_proc = process_table[i];
			break;
		}
	}
	// check if pid argument named a process that was not a child of the current process
	// only the parent can wait for the child process
	if(child_proc == NULL)
	{
		return ECHILD;
	}

	//Wait for the process specified by pid to exit
	lock_acquire(child_proc->proc_wait_lock);
	while (child_proc->proc_is_exit == 0)
	{
		cv_wait(child_proc->proc_wait_cv, child_proc->proc_wait_lock);
	}
	lock_release(child_proc->proc_wait_lock);
	//return an encoded exit status in the integer pointed to by status
	int result = copyout((void *)(&(child_proc->exit_code)), (userptr_t)proc_status, sizeof(int));
	memcpy(retval, &pid, sizeof(int));

	if(result == 1)
	{
		return 1;
	}
	*retval = pid;
	return 0;
}

/*
	Cause the current process to exit. The exit code exitcode is reported back to 
	other process(es) via the waitpid() call. The process id of the exiting process 
	should not be reused until all processes interested in collecting the exit code 
	with waitpid have done so. 	
*/
void sys_exit(int exitcode)
{
	struct proc *proc = curproc;

	proc->exit_code = _MKWAIT_EXIT(exitcode);
	proc->proc_is_exit = 1;

	for (int i = PID_MIN; i < PID_MAX; i++)
	{
		if (proc->child_proc_table[i])
		{
			lock_release(proc->child_proc_table[i]->proc_exit_lock);
			proc->child_proc_table[i] = NULL;
		}
	}
	
	cv_broadcast(proc->proc_wait_cv, proc->proc_wait_lock);

	as_deactivate();
	struct addrspace *as = proc_setas(NULL);
	as_destroy(as);

	proc_remthread(curthread);

	wait_for_parent_process_to_exit(proc);
	

	lock_acquire(process_table_lock);	
	process_table[proc->pid]= NULL;
	lock_release(process_table_lock);	

	//lock_destroy(proc->proc_exit_lock);
	//lock_destroy(proc->proc_wait_lock);
	//lock_destroy(proc->child_proc_lock);

	// for(int i = 0; i <= OPEN_MAX; i++)
	// {
	// 	if(proc->p_fd[i] != NULL)
	// 	{
	// 		lock_destroy(proc->p_fd[i]->file_lock);
	// 	}
	// 	//kprintf("if statement done");
	// }
	//kfree(tf);
	//cv_destroy(proc->proc_wait_cv);
	proc_destroy(proc);

	spec_thread_exit();
}

void wait_for_parent_process_to_exit(struct proc *proc)
{
	lock_acquire(proc->proc_exit_lock);
	lock_release(proc->proc_exit_lock);
}

void decrement_vnode_reference(void)
{
	for (int i  = 0; i < OPEN_MAX; i++)
	{
		lock_acquire(curproc->p_fd[i]->file_lock);

		if (curproc->p_fd[i]->fvnode != NULL)
		{
			curproc->p_fd[i]->vnode_reference--;
		}

		lock_release(curproc->p_fd[i]->file_lock);
	}
}