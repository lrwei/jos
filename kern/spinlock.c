// Mutual exclusion spin locks.

#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <inc/env.h>

#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/kdebug.h>

// The big kernel lock
struct spinlock kernel_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "kernel_lock"
#endif
};

#ifdef USE_FINE_GRAINED_LOCK
struct spinlock console_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "console_lock"
#endif
};

struct spinlock page_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "page_lock"
#endif
};

struct spinlock env_list_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "env_list_lock"
#endif
};

struct spinlock sched_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "sched_lock"
#endif
};
#endif

#ifdef DEBUG_SPINLOCK
// Record the current call stack in pcs[] by following the %ebp chain.
static void
get_caller_pcs(uint32_t pcs[])
{
	uint32_t *ebp;
	int i;

	ebp = (uint32_t *)read_ebp();
	for (i = 0; i < 10; i++){
		if (ebp == 0 || ebp < (uint32_t *)ULIM)
			break;
		pcs[i] = ebp[1];          // saved %eip
		ebp = (uint32_t *)ebp[0]; // saved %ebp
	}
	for (; i < 10; i++)
		pcs[i] = 0;
}
#endif

// Check whether this CPU is holding the lock.
bool
holding(struct spinlock *lock)
{
	return lock->locked
#ifdef DEBUG_SPINLOCK
            && lock->cpu == thiscpu
#endif
            ;
}

void
__spin_initlock(struct spinlock *lk, char *name)
{
	lk->locked = 0;
#ifdef DEBUG_SPINLOCK
	lk->name = name;
	lk->cpu = 0;
#endif
}

static void
print_lock(const char *s, struct spinlock *lk)
{
#if defined(USE_FINE_GRAINED_LOCK) && defined(DEBUG_SPINLOCK)
    struct Env *e;

    if (lk->name[0] != '&') {
        cprintf("CPU %d: %s lock %s\n", cpunum(), s, lk->name);
    } else {
        e = (struct Env *) ((void *) lk - offsetof(struct Env, env_lock));
        cprintf("CPU %d: %s lock of env [%08x]\n", cpunum(), s, e->env_id);
    }
#endif
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
__spin_lock(struct spinlock *lk, const char *filename, int line, bool print)
{
#ifdef DEBUG_SPINLOCK
	if (holding(lk)) {
            cprintf("at %s: %d\n", filename, line);
            panic("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
        }

        if (print) {
            print_lock("acquiring", lk);
        }
#endif

	// The xchg is atomic.
	// It also serializes, so that reads after acquire are not
	// reordered before it. 
	while (xchg(&lk->locked, 1) != 0)
		asm volatile ("pause");

	// Record info about lock acquisition for debugging.
#ifdef DEBUG_SPINLOCK
        if (print) {
            print_lock("got", lk);
        }

	lk->cpu = thiscpu;
	get_caller_pcs(lk->pcs);
#endif
}

// Release the lock.
void
__spin_unlock(struct spinlock *lk, const char *filename, int line, bool print)
{
#ifdef DEBUG_SPINLOCK
	if (!holding(lk)) {
		int i;
		uint32_t pcs[10];
		// Nab the acquiring EIP chain before it gets released
		memmove(pcs, lk->pcs, sizeof pcs);
		cprintf("CPU %d cannot release %s: held by CPU %d\nAcquired at:", 
			cpunum(), lk->name, lk->cpu->cpu_id);
		for (i = 0; i < 10 && pcs[i]; i++) {
			struct Eipdebuginfo info;
			if (debuginfo_eip(pcs[i], &info) >= 0)
				cprintf("  %08x %s:%d: %.*s+%x\n", pcs[i],
					info.eip_file, info.eip_line,
					info.eip_fn_namelen, info.eip_fn_name,
					pcs[i] - info.eip_fn_addr);
			else
				cprintf("  %08x\n", pcs[i]);
		}
		panic("spin_unlock");
	}

	lk->pcs[0] = 0;
	lk->cpu = 0;

        if (print) {
            print_lock("releasing", lk);
        }
#endif

	// The xchg instruction is atomic (i.e. uses the "lock" prefix) with
	// respect to any other instruction which references the same memory.
	// x86 CPUs will not reorder loads/stores across locked instructions
	// (vol 3, 8.2.2). Because xchg() is implemented using asm volatile,
	// gcc will not reorder C statements across the xchg.
	xchg(&lk->locked, 0);
}
