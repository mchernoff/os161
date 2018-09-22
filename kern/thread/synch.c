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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

		lock->lk_wchan = wchan_create(lock->lk_name);	//Initialize a wait channel
		if (lock->lk_wchan == NULL) {
			kfree(lock->lk_name);
			kfree(lock);
			return NULL;
		}

		lock->lk_owner = NULL;						//No thread is holding the lock at that moment
		spinlock_init(&(lock->lk_spinlock));		//Initialize a spinlock.

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

		//Free the memory that is being allocated
		spinlock_cleanup(&lock->lk_spinlock);
		wchan_destroy(lock->lk_wchan);
		kfree(lock->lk_owner);

        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{		
		KASSERT(lock != NULL);								//check lock has been initialized
		KASSERT(curthread->t_in_interrupt == false);		// do not block in an interrupt handler
		KASSERT(!lock_do_i_hold(lock));				//Make sure the current thread does not hold the lock 

		spinlock_acquire(&lock->lk_spinlock);		//Get the lock, spinning as necessary, disables interrupts

		while (lock->lk_owner != NULL) {						//While no thread is holding the lock
			wchan_sleep(lock->lk_wchan, &(lock->lk_spinlock));		//set the waiting channel to sleeping mode
		}

		lock->lk_owner = curthread;				//the lock is now holded by the current thread
		spinlock_release(&lock->lk_spinlock);	//now unlock the spin lock
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);					//check lock has been initialized
	spinlock_acquire(&lock->lk_spinlock);	
	if (lock_do_i_hold(lock))		//only when the current thread hold a lock
	{
		lock->lk_owner = NULL;					//the lock is not holded by anyone
		wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);			//next that is thread sleeping on a wait channel will be waked up
	}
	spinlock_release(&lock->lk_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	if (lock->lk_owner == curthread)		//Return true if the current thread holds the lock, false otherwise.
		return true;
	else
		return false;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
	
		cv->cv_wchan = wchan_create(cv->cv_name);		//Initialize a wait channel
		if (cv->cv_wchan == NULL) {
			kfree(cv->cv_name);
			kfree(cv);
			return NULL;
		}

		spinlock_init(&cv->cv_spinlock);
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

		spinlock_cleanup(&cv->cv_spinlock);
		wchan_destroy(cv->cv_wchan);		//deallocate memory for wait channel
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
//Release the supplied lock, go to sleep, and, after waking up again, 
//reacquire the lock.
{
	KASSERT(lock != NULL);				//always check to make sure lock was initialized
	KASSERT(cv != NULL);				//always check to make sure cv was initialized		

	
	spinlock_acquire(&(cv->cv_spinlock));
	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &(cv->cv_spinlock));			//turn wait channel back into sleeping mode
	spinlock_release(&(cv->cv_spinlock));
	lock_acquire(lock);
	
}

void
cv_signal(struct cv *cv, struct lock *lock)
//Wake up one thread that's sleeping on this CV and allowed to resume execution
{
	KASSERT(lock != NULL);				//always check to make sure lock was initialized
	KASSERT(cv != NULL);				//always check to make sure cv was initialized
	KASSERT(lock_do_i_hold(lock));		//make sure currrent thread HOLD the lock

	spinlock_acquire(&(cv->cv_spinlock));
	wchan_wakeone(cv->cv_wchan, &(cv->cv_spinlock));		 //if there is at least one thread enqueued then one such thread is dequeued and allowed to resume execution
	spinlock_release(&(cv->cv_spinlock));
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(lock != NULL);				//always check to make sure lock was initialized
	KASSERT(cv != NULL);				//always check to make sure cv was initialized
	KASSERT(lock_do_i_hold(lock));		//make sure currrent thread HOLD the lock

	spinlock_acquire(&(cv->cv_spinlock));
	wchan_wakeall(cv->cv_wchan, &(cv->cv_spinlock));		//if there are any threads enqueued then all such threads are allowed to resume execution
	spinlock_release(&(cv->cv_spinlock));
}
