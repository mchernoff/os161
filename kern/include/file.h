#ifndef _FILE_H_
#define _FILE_H_

#include <limits.h>
#include <proc.h>

struct file_descriptor				//define a file descriptor that has a file structure and a lock
{
	char fname[NAME_MAX];
	struct vnode *fvnode;		//each file should have a vnode that contains representation of a file
	int flags;					//O_RDONLY, O_WRONLY or O_RDWR
	mode_t mode;				//may use or not, its an optional arguement from OS/161 Reference Manual
	off_t offset;				//the position of entry in file table
	struct lock *file_lock;
};

#endif