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

#define UNUSED(x) (void)(x)

int sys_open(const char *filename, int flags);
int sys_write(int fd, const void *buf, size_t nbytes);


int
sys_open(const char *filename, int flags)
{
	UNUSED(filename);
	UNUSED(flags);
	return 0;
}
int
sys_write(int fd, const void *buf, size_t nbytes)
{
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(nbytes);
	return 0;
}
