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

#define UNUSED(x) (void)(x)

/*
	sys_open opens the file, device, or other kernel object named by the pathname filename. 
	The flags argument specifies how to open the file and should consist of one of

		O_RDONLY	Open for reading only.
		O_WRONLY	Open for writing only.
		O_RDWR		Open for reading and writing.

*/
int	sys_open(const char *filename, int flags, mode_t mode, int *retval)
{
	UNUSED(retval);

	//check if filename is valid input
	if (filename == NULL) {
		return EFAULT;
	}

	//check if flags is valid input
	if (flags != O_RDONLY || flags != O_WRONLY || flags != O_RDWR) {
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

	curproc->p_fd[fd]->file_lock = lock_create(filename);
	lock_acquire(curproc->p_fd[fd]->file_lock);
	//allocate memory block for new entry in file table
	curproc->p_fd[fd] = (struct file_descriptor *)kmalloc(sizeof(struct file_descriptor*));

	//allocates a memory block in kernel for filename and copy that from user-level 
	//address to kernel space
	char* kfname = kmalloc(NAME_MAX * sizeof(char*));
	size_t fnamelength;
	int result;
	result = copyinstr((const_userptr_t)filename, kfname, NAME_MAX, &fnamelength);

	if (result == 1) {
		kfree(kfname);
		return result;
	}

	//virtual file system open or create a file
	struct vnode *filevnode = NULL;
	result = vfs_open((char*)filename, flags, mode, &filevnode);
	curproc->p_fd[fd]->fvnode = filevnode;

	if (result == 1) {
		kfree(kfname);
		return result;
	}

	//set up contents for file descriptor 
	curproc->p_fd[fd]->flags = flags;
	curproc->p_fd[fd]->offset = 0;
	*retval = fd;
	lock_release(curproc->p_fd[fd]->file_lock);
	kfree(kfname);

	return 0;
}


/*
	sys_write writes up to buflen bytes to the file specified by fd, at the location in the 
	file specified by the current seek position of the file, taking the data from the space 
	pointed to by buf. The file must be open for writing.
*/
int sys_write(int fd, const void *buf, size_t nbytes, int *retval)
{
	//fd is not a valid file descriptor
	if (fd < 0 ) {
		return EBADF;
	}

	//buf is invalid
	if (buf == NULL) {
		return EFAULT;
	}

	//if flag is incorrect
	//if (curproc->p_fd[fd]->flags != O_WRONLY || curproc->p_fd[fd]->flags != O_RDWR)
	//{
	//	*retval = -1;
	//	return EINVAL;
	//}

	lock_acquire(curproc->p_fd[fd]->file_lock);		//acquire the lock to the file

	struct uio uio;				//used to manage blocks of data moved around by the kernel
	struct iovec iovec;			//read I/O calls	

	char *buffer = (char*)kmalloc(nbytes);
	uio_kinit(&iovec, &uio, (void*) buf, nbytes, curproc->p_fd[fd]->offset, UIO_WRITE);
	iovec.iov_ubase = (userptr_t)buf;			//user-supplied pointer
	iovec.iov_len = nbytes;						//length of data

	int result = VOP_WRITE(curproc->p_fd[fd]->fvnode, &uio);		//write data from uio to file at offset
	if (result == 1) {
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
sys_read(int fd, void *buf, size_t nbytes)
{
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(nbytes);
	return 0;
}

int
sys_close(int fd)
{
	UNUSED(fd);
	return 0;
}

int
sys_lseek(int fd, off_t pos, int whence)
{
	UNUSED(fd);
	UNUSED(pos);
	UNUSED(whence);
	return 0;
}

int
sys_chdir(const char *pathname)
{
	UNUSED(pathname);
	return 0;
}

int
sys_dup2(int oldfd, int newfd)
{
	UNUSED(oldfd);
	UNUSED(newfd);
	return 0;
}

int
sys_getcwd(char *buf, size_t buflen)
{
	UNUSED(buf);
	UNUSED(buflen);
	return 0;
}