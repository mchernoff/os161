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
#include <synch.h>

//initialize the file descriptor 0, 1, 2
int file_descriptor_init(struct proc *process)
{
	//create vnodes for file descriptor 0, 1, 2
	struct vnode *vn_stdin = NULL;
	struct vnode *vn_stdout = NULL;
	struct vnode *vn_stderr = NULL;

	//files opened from the path "con:"
	char *stdin = kstrdup("con:");
	char *stdout = kstrdup("con:");
	char *stderr = kstrdup("con:");

	//if fail to open or create a file, then file_descriptor_init fail, 
	//return 0 and free memory
	if (vfs_open(stdin, O_RDONLY, 0, &vn_stdin) != 0 || vn_stdin == NULL)
	{
		kfree(stdin);
		return 0;
	}

	if (vfs_open(stdout, O_WRONLY, 0, &vn_stdout) != 0 || vn_stdout == NULL)
	{
		kfree(stdin);
		kfree(stdout);
		return 0;
	}

	if (vfs_open(stderr, O_RDWR, 0, &vn_stderr) != 0 || vn_stderr == NULL)
	{
		kfree(stdin);
		kfree(stdout);
		kfree(stderr);
		return 0;
	}

	//set all descriptor in file table to null
	for (int i = 0; i < OPEN_MAX; i++)
	{
		process->p_fd[i] = NULL;
	}

	//initialize stdin
	process->p_fd[0] = (struct file_descriptor *)kmalloc(sizeof(struct file_descriptor));		//allocate memory for file struct
	strcpy(process->p_fd[0]->fname, stdin);		
	process->p_fd[0]->fvnode = vn_stdin;		
	process->p_fd[0]->mode = O_RDONLY;
	process->p_fd[0]->flags = O_RDONLY;					
	process->p_fd[0]->offset = 0;
	process->p_fd[0]->file_lock = lock_create(stdin);
	process->p_fd[0]->vnode_reference = 1;

	//initialize stdout
	process->p_fd[1] = (struct file_descriptor *)kmalloc(sizeof(struct file_descriptor));
	strcpy(process->p_fd[1]->fname, stdout);
	process->p_fd[1]->fvnode = vn_stdout;
	process->p_fd[1]->mode = O_WRONLY;
	process->p_fd[1]->flags = O_WRONLY;
	process->p_fd[1]->offset = 0;
	process->p_fd[1]->file_lock = lock_create(stdout);
	process->p_fd[0]->vnode_reference = 1;

	//initialize stderr
	process->p_fd[2] = (struct file_descriptor *)kmalloc(sizeof(struct file_descriptor));
	strcpy(process->p_fd[2]->fname, stderr);
	process->p_fd[2]->fvnode = vn_stderr;
	process->p_fd[2]->mode = O_RDWR;
	process->p_fd[2]->flags = O_RDWR;
	process->p_fd[2]->offset = 0;
	process->p_fd[2]->file_lock = lock_create(stderr);
	process->p_fd[0]->vnode_reference = 1;

	return 1;
}