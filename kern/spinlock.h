#ifndef JOS_INC_SPINLOCK_H
#define JOS_INC_SPINLOCK_H

#include <inc/types.h>

// Comment this to disable spinlock debugging
#define DEBUG_SPINLOCK
#define USE_FINE_GRAINED_LOCK

// Mutual exclusion lock.
struct spinlock {
	unsigned locked;       // Is the lock held?

#ifdef DEBUG_SPINLOCK
	// For debugging:
	char *name;            // Name of lock.
	struct CpuInfo *cpu;   // The CPU holding the lock.
	uintptr_t pcs[10];     // The call stack (an array of program counters)
	                       // that locked the lock.
#endif
};

#ifdef USE_FINE_GRAINED_LOCK
extern struct spinlock console_lock;
// Guard both `page_free_list` and each individual pages.
extern struct spinlock page_lock;
extern struct spinlock env_list_lock;
extern struct spinlock sched_lock;
#endif

void __spin_initlock(struct spinlock *lk, char *name);
void __spin_lock(struct spinlock *lk, const char *filename, int line, bool is_env);
void __spin_unlock(struct spinlock *lk, const char *filename, int line, bool is_env);
bool holding(struct spinlock *lk);

#define spin_initlock(lock)             __spin_initlock(lock, #lock)

#define spin_lock(lock)                 __spin_lock(lock, __FILE__, __LINE__, false)
#define spin_lock__do_print(lock)       __spin_lock(lock, __FILE__, __LINE__, true)
#define spin_unlock(lock)               __spin_unlock(lock, __FILE__, __LINE__, false)
#define spin_unlock__do_print(lock)     __spin_unlock(lock, __FILE__, __LINE__, true)

extern struct spinlock kernel_lock;

static inline void
lock_kernel(void)
{
	spin_lock(&kernel_lock);
}

static inline void
unlock_kernel(void)
{
	spin_unlock(&kernel_lock);

	// Normally we wouldn't need to do this, but QEMU only runs
	// one CPU at a time and has a long time-slice.  Without the
	// pause, this CPU is likely to reacquire the lock before
	// another CPU has even been given a chance to acquire it.
	asm volatile("pause");
}

#endif
