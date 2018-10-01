/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NROPES 16
static int ropes_left = NROPES;



// Data structures for rope mappings

// Use a bool array to represent ropes. The rope index will be 0 to NROPES - 1.
// False(0) if ropes are tied with stake, true(1) if ropes are cutted.
int ropes[NROPES];

// Use to count how many threads have been finished. 
// It will be 3 if Dandelion, Marigold and Lord FlowerKiller threads are finished 
// so Balloon thread can exit.
// It will be 4 if Dandelion, Marigold, Lord FlowerKiller and Balloon threads are 
// finished so main thread can exit.
int finishedThreads;

// A stake data structure. Each stake has an array to indicate which rope is 
// tied with that stake.
struct stake {
	int stakeRopeMap[NROPES];
};

// An array for stakes. Like an array of stakes represent the rope stake map for all the
// stakes
struct stake* stakeArray[NROPES];

// Synchronization primitives 

// Condition Variable for main threads
struct cv* threadCV;

// Condition Variable lock for main threads
struct lock* CVLock;

// Condition Variable for balloon Thread
struct cv* balloonCV;

// Ballooon CV lock for balloon Thread
struct lock* balloonLock;

// Declear an array of locks so each rope has a look
struct lock* ropelockArray[NROPES];

// Lock for thread counters, acquire the lock before thread counter changes and release after
struct lock* counterLock;

// Rope look, acquire the lock before changeing the number of ropes remain.
static struct lock *ropeLock;


/*
 * Describe your design and any invariants or locking protocols 
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?  
 */

/* For this task, we have ropes and stakes. Base on requirement each rope can only 
	point to one stakes but one stake can possible point to multiple ropes.	

	Therefore I declear 
					int ropes[NROPES];
	A array of ints to represent the state of each ropes:		
		when int ropes[i] = 1, this rope i has been cut, 
		when int ropes[i] = 0, this rope i has not been cut yet.

	Also I declear 
		struct stake {
			int stakeRopeMap[NROPES];
		};

	A stake data structure that contain an int array. This array represent which
	rope is tied with the stake since mutiple rope can tie to the same stake at the same
	time. 		
		when int stakeRopeMap[i] = 1, rope i is tied to this stake,
		when int stakeRopeMap[i] = 0, there is no rope tied to this stake.
	
	And struct stake* stakeArray[NROPES]; to declear all the stakes(same amount with ropes).

	I have int finishedThreads to count how many threads have been finished. 
		It will be 3 if Dandelion, Marigold and Lord FlowerKiller threads are finished 
		so Balloon thread can exit.
		It will be 4 if Dandelion, Marigold, Lord FlowerKiller and Balloon threads are 
		finished so main thread can exit.

	For Synchronization primitives, I declear a Condiction Variable and a Condition Variable 
	lock for both balloon Thread(balloonCV, balloonLock) and main threads(threadCV, CVLock).

	Base on the requirement, this program will first call main thread. Main thread calls
	balloon, dandelion, marigold and flowerkiller thread and will be sleeping until balloon 
	thread finish. Balloon thread will be sleeping until dandelion, marigold and flowerkiller 
	thread finish. Therefore I use condiction variable and cv_signal() function to wake up
	and resume a sleeping thread.

	In addition, I declear three more locks: 

		counterLock acquire before editing finishedThreads and release after editing 
		finishedThreads to make sure there is no conflict such as 2 thread finish at the
		same time and both want to do finishedThreads++;

		ropelockArray[NROPES] is an array of locks to represent each lock for each rope.
		This is to make sure only one thread can cut one rope at the same time.

		ropeLock acquire before editing how many rope are remain tied and release after 
		editing that to make sure dandelion and marigold wont do ropes_left-- at the same 
		and cause conflict time


	dandelion, marigold and flowerkiller thread exit when there is no uncut rope remain(ropes_left = 0)
	Balloon thread exit if Dandelion, Marigold and Lord FlowerKiller threads are all finished.
	Main thread exit if Balloon, Dandelion, Marigold and Lord FlowerKiller threads are all finished.

	There are more comments for each function to explain how my implementation will work.
*/

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;			//surpress compile error 
	(void)arg;			//surpress compile error

	kprintf("Dandelion thread starting\n");

	int ramdomNum;					// declear to use later to store ramdom generated int

	while (ropes_left != 0)			// Dandelion cuts ropes while there are ropes left
	{
		ramdomNum = (random() % NROPES);			// Generate a ramdom number between 0 and NROPES - 1
		lock_acquire(ropelockArray[ramdomNum]);		// Acquire a lock on that single ramdom rope

		if (ropes[ramdomNum] == 0)
		{
			
			ropes[ramdomNum] = 1;				// Cut the ropes if ropes are tied	
			lock_acquire(ropeLock);				// Acquire the lock on ropes before performing on ropes
			ropes_left--;						
			kprintf("Dandelion severed rope %d\n", ramdomNum);
			lock_release(ropeLock);				// Release the lock on ropes after performing on ropes
		}

		lock_release(ropelockArray[ramdomNum]);		// Release a lock on that single ramdom rope
		thread_yield();								// Switch thread to ready
	}

	// No ropes left now
	kprintf("Dandelion thread done");
	lock_acquire(counterLock);						// Lock finishedThreads
	finishedThreads = finishedThreads + 1;			// One more thread(Dandelion) finished


	if (finishedThreads == 3) {						// All threads finished
		lock_acquire(balloonLock);
		cv_signal(balloonCV, balloonLock);			// Wake up balloon thread and allowed to resume execution
		lock_release(balloonLock);
	}

	lock_release(counterLock);
	thread_exit();

}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;			//surpress compile error 
	(void)arg;			//surpress compile error

	kprintf("Marigold thread starting\n");

	//Marigold selects ropes by generating a random ground stake index and
	//then unties the ropes from the ground stakes

	int ramdomNum;					// declear to use later to store ramdom generated int
	struct stake* stakes;			// declear a stake data structure to indicate which ropes are tied to which stakes

	while (ropes_left != 0)			// Marigold cuts ropes while there are ropes left
	{
		ramdomNum = (random() % NROPES);			// Generate a ramdom number between 0 and NROPES - 1
		stakes = stakeArray[ramdomNum];				// Get the stake with the same index

		for (int i = 0; i < NROPES; i++)
		{
			if (stakes->stakeRopeMap[i] == 1 && ropes[i] == 0)		//if there is rope tied with stake that has not been cut yet
			{
				lock_acquire(ropelockArray[i]);				// Acquire the lock on ropes before performing on ropes
				stakes->stakeRopeMap[i] = 0;				// -1 represent cut the ropes if ropes are tied		
				ropes[i] = 1;								// Rope is cut
				lock_release(ropelockArray[i]);

				lock_acquire(ropeLock);				// Acquire the lock on ropes before performing on ropes
				ropes_left--;
				kprintf("Marigold severed rope %d\n from stake %d\n", i, ramdomNum);
				lock_release(ropeLock);				// Release the lock on ropes after performing on ropes
				break;								// break the for loop after Marigold cut the only rope from the stake
			}
			thread_yield();
		}
	}

	// No ropes left now
	kprintf("Marigold thread done");
	lock_acquire(counterLock);						// Lock finishedThreads
	finishedThreads = finishedThreads + 1;			// One more thread(Marigold) finished


	if (finishedThreads == 3) {						// All threads finished
		lock_acquire(balloonLock);
		cv_signal(balloonCV, balloonLock);			// Wake up balloon thread and allowed to resume execution
		lock_release(balloonLock);
	}

	lock_release(counterLock);
	thread_exit();
}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;			//surpress compile error 
	(void)arg;			//surpress compile error

	kprintf("Lord FlowerKiller thread starting\n");

	// declear int later used for generating random value
	int random1;
	int random2;

	//Declear stake for switching ropes
	struct stake* Stake1;
	struct stake* Stake2;

	while (ropes_left != 0)
	{
		random1 = random() % 16;
		random2 = random() % 16;

		while (random1 == random2)				// make sure the index of first stake does not equal to second stake
		{
			random1 = random() % 16;
			random2 = random() % 16;
		}

		Stake1 = stakeArray[random1];			// choose a random stake out of all stakes
		Stake2 = stakeArray[random2];			// choose a different random stake out of all stakes

		for (int i = 0; i < NROPES; i++) {

			if (Stake1->stakeRopeMap[i] == 1 && ropes[i] == 0)		// if there is a rope on stake1 that has not been cut yet
			{
				lock_acquire(ropelockArray[i]);				// lock that rope
				Stake1->stakeRopeMap[i] = 0;				// remvoe that rope from stake1
				Stake2->stakeRopeMap[i] = 1;				// apply that rope from stake2
				lock_release(ropelockArray[i]);				// unlock that rope
				kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", i, random1, random2);
			}

			thread_yield();
		}

	}

	kprintf("Lord FlowerKiller thread done\n");
	lock_acquire(counterLock);						// Lock finishedThreads
	finishedThreads = finishedThreads + 1;			// One more thread(flowerkiller) finished

	if (finishedThreads == 3) {						// All threads finished
		lock_acquire(balloonLock);
		cv_signal(balloonCV, balloonLock);			// Wake up balloon thread and allowed to resume execution
		lock_release(balloonLock);
	}

	lock_release(counterLock);
	thread_exit();
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;			//surpress compile error 
	(void)arg;			//surpress compile error
	
	kprintf("Balloon thread starting\n");

	while (ropes_left != 0)			//balloon thread should wait until all ropes has been cut
	{

	}

	// Once Dandelion, Marigold and Lord FlowerKiller threads are finished
	// Wake up balloon thread and allowed to resume execution
	lock_acquire(balloonLock);
	cv_signal(balloonCV, balloonLock);			
	lock_release(balloonLock);

	lock_acquire(counterLock);						// Lock finishedThreads
	finishedThreads = finishedThreads + 1;			// One more thread(balloon) finished

	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	lock_release(counterLock);

	if (finishedThreads == 4) {					// All threads plus balloon threads are finished
		lock_acquire(CVLock);
		cv_signal(threadCV, CVLock);			// Wake up main thread and allowed to resume execution
		lock_release(CVLock);
	}

	thread_exit();

}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{
	(void)nargs;			//surpress compile error
	(void)args;				//surpress compile error

	int err = 0;

	ropes_left = NROPES;					//initialize ropes_left
	finishedThreads = 0;

	// Initialize Condiction Variables and Locks
	threadCV = cv_create("thread");
	counterLock = lock_create("counterLock");
	CVLock = lock_create("CVLock");
	balloonCV = cv_create("balloon");
	balloonLock = lock_create("balloon");
	ropeLock = lock_create("ropeLock");
	
	
	for (int i = 0; i < NROPES; i++) {
		stakeArray[i] = kmalloc(sizeof(struct stake));			// Allocate memory and initialize stakeArray
		ropelockArray[i] = lock_create("lock");						// Initialize locks for each ropes
		ropes[i] = 0;

	}

	// Initialize empty stakes
	for (int i = 0; i < NROPES; i++)		//loop through each stakes
	{
		for (int j = 0; j < NROPES; j++)	//loop through each rope position on one stakes
		{
			stakeArray[i]->stakeRopeMap[j] = 0;		// stakes and ropes are not tied together yet
		}
	}

	// Set one rope per stakes
	for (int i = 0; i < NROPES; i++) {
		stakeArray[i]->stakeRopeMap[i] = 1;
	}


	// error asserts of each threads
	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Lord FlowerKiller Thread",
			  NULL, flowerkiller, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;
	
	//If all threads are work and finished, then resume main thread after Balloon thread is finished
	lock_acquire(CVLock);
	cv_signal(threadCV, CVLock);			// Wake up main thread and allowed to resume execution
	lock_release(CVLock);

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));
	
done:

	// deallocate all the memory allocated eariler
	lock_destroy(ropeLock);
	lock_destroy(CVLock);
	lock_destroy(balloonLock);
	lock_destroy(counterLock);
	cv_destroy(balloonCV);
	cv_destroy(threadCV);

	kprintf("Main thread done\n");
	return 0;
}
