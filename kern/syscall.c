/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/spinlock.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
        user_mem_assert(curenv, s, len, 0);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
        // XXX: Still lack of the concept of "terminal"s.
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0) {
#ifdef USE_FINE_GRAINED_LOCK
            if (e != curenv) {
                spin_unlock(&e->env_lock);
            }
#endif
            return r;
        }
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
        int r;
        struct Env *e;

        r = env_alloc(&e, curenv->env_id);
        if (r < 0) {
            return r;
        }

        e->env_status = ENV_NOT_RUNNABLE;
        e->env_tf = curenv->env_tf;
        e->env_tf.tf_regs.reg_eax = 0;

        return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
        int r;
        struct Env *e;

        r = envid2env(envid, &e, true);
        if (r < 0) {
            goto exit;
        }
        if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
            r = -E_INVAL;
            goto exit;
        }

        barrier();
        e->env_status = status;

exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (e != curenv) {
            spin_unlock(&e->env_lock);
        }
#endif
        return r;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
        int r;
        struct Env *e;

        r = envid2env(envid, &e, true);
        if (r < 0) {
            goto exit;
        }

        e->env_pgfault_upcall = func;

exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (e != curenv) {
            spin_unlock(&e->env_lock);
        }
#endif
        return r;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
        int r;
        struct Env *e;
        struct PageInfo *p;

        r = envid2env(envid, &e, true);
        if (r < 0) {
            goto exit;
        }

        if ((uintptr_t) va >= UTOP || va != ROUNDUP(va, PGSIZE)) {
            r = -E_INVAL;
            goto exit;
        }
        if (!(PTE_P & perm) || !(PTE_U & perm) || perm & ~PTE_SYSCALL) {
            r = -E_INVAL;
            goto exit;
        }

        p = page_alloc(ALLOC_ZERO);
        if (!p) {
            r = -E_NO_MEM;
            goto exit;
        }
        r = page_insert(e->env_pgdir, p, va, perm);
        if (r < 0) {
#ifdef USE_FINE_GRAINED_LOCK
            spin_lock(&page_lock);
#endif
            page_free(p);
#ifdef USE_FINE_GRAINED_LOCK
            spin_unlock(&page_lock);
#endif
            goto exit;
        }

exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (e != curenv) {
            spin_unlock(&e->env_lock);
        }
#endif
        return r;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
        int r;
        struct Env *srcenv, *dstenv;
        struct PageInfo *p;
        pte_t *pte;

        r = envid2env(srcenvid, &srcenv, true);
        if (r < 0) {
            goto _exit;
        }
        r = envid2env(dstenvid, &dstenv, true);
        if (r < 0) {
            goto exit;
        }

        if ((uintptr_t) srcva >= UTOP || (uintptr_t) dstva >= UTOP
                || srcva != ROUNDUP(srcva, PGSIZE)
                || dstva != ROUNDUP(dstva, PGSIZE)) {
            r = -E_INVAL;
            goto exit;
        }

        p = page_lookup(srcenv->env_pgdir, srcva, &pte);
        if (!p) {
            r = -E_INVAL;
            goto exit;
        }

        if (!(PTE_P & perm) || !(PTE_U & perm) || perm & ~PTE_SYSCALL) {
            r = -E_INVAL;
            goto exit;
        }

        if ((PTE_W & perm) && !(PTE_W & *pte)) {
            r = -E_INVAL;
            goto exit;
        }

        r = page_insert(dstenv->env_pgdir, p, dstva, perm);
        if (r < 0) {
            goto exit;
        }

exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (dstenv != curenv) {
            spin_unlock(&dstenv->env_lock);
        }
#endif
_exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (srcenv != curenv) {
            spin_unlock(&srcenv->env_lock);
        }
#endif
        return r;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
        int r;
        struct Env *e;

        r = envid2env(envid, &e, true);
        if (r < 0) {
            goto exit;
        }

        if ((uintptr_t) va >= UTOP || va != ROUNDUP(va, PGSIZE)) {
            r = -E_INVAL;
            goto exit;
        }

        page_remove(e->env_pgdir, va);

exit:
#ifdef USE_FINE_GRAINED_LOCK
        if (e != curenv) {
            spin_unlock(&e->env_lock);
        }
#endif
        return r;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
        int r;
        struct Env *e;
        pte_t *pte;
        struct PageInfo *p = NULL;

        if ((uintptr_t) srcva < UTOP) {
            if (srcva != ROUNDUP(srcva, PGSIZE)) {
                return -E_INVAL;
            }
            if (!(PTE_U & perm) || !(PTE_P & perm) || perm & ~PTE_SYSCALL) {
                return -E_INVAL;
            }
            p = page_lookup(curenv->env_pgdir, srcva, &pte);
            if (!p) {
                return -E_INVAL;
            }
            if ((PTE_W & perm) && !(PTE_W & *pte)) {
                return -E_INVAL;
            }
        }

        r = envid2env(envid, &e, false);
        if (r < 0) {
            goto exit;
        }

        if (e == curenv) {
            return -E_BAD_ENV;
        }

        if (!e->env_ipc_recving) {
            env_ipc_enqueue(e, curenv);

            curenv->env_ipc_pending_value = value;
            curenv->env_ipc_pending_page = p;
            curenv->env_ipc_pending_perm = perm;
            curenv->env_ipc_pending_next = -1;

            barrier();
            curenv->env_status = ENV_NOT_RUNNABLE;
            // Not a real return, as curenv has been marked as NOT_RUNNABLE.
            // Yield scheduler through trap() instead.
            r = -E_UNSPECIFIED;
            goto exit;
        }

        assert(e->env_ipc_pending_first == -1);
        if ((uintptr_t) e->env_ipc_dstva < UTOP && p) {
            r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm);
            if (r < 0) {
                goto exit;
            }
            e->env_ipc_perm = perm;
        } else {
            e->env_ipc_perm = 0;
        }

        e->env_ipc_recving = false;
        e->env_ipc_value = value;
        e->env_ipc_from = curenv->env_id;
        e->env_tf.tf_regs.reg_eax = 0;

        barrier();
        e->env_status = ENV_RUNNABLE;

exit:
#ifdef USE_FINE_GRAINED_LOCK
        spin_unlock(&e->env_lock);
#endif
        return r;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
//	-E_NO_MEM if there's not enough memory to map env_ipc_pending_page
//		in address space.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
        int r = 0;
        struct Env *e;

        if ((uintptr_t) dstva < UTOP && dstva != ROUNDUP(dstva, PGSIZE)) {
            return -E_INVAL;
        }

retry:
        if (curenv->env_ipc_pending_first < 0) {
            curenv->env_ipc_recving = true;
            curenv->env_ipc_dstva = dstva;

            barrier();
            curenv->env_status = ENV_NOT_RUNNABLE;
            // Not a real return, as curenv has been marked as NOT_RUNNABLE.
            // Yield scheduler through trap() instead.
            return -E_UNSPECIFIED;
        }

        env_ipc_dequeue(curenv, &e);
        if ((uintptr_t) dstva < UTOP && e->env_ipc_pending_page) {
            r = page_insert(curenv->env_pgdir, e->env_ipc_pending_page,
                    dstva, e->env_ipc_pending_perm);
            if (r < 0) {
                // Inform all pending senders that we ran out of memory.
                e->env_tf.tf_regs.reg_eax = r;

                barrier();
                e->env_status = ENV_RUNNABLE;
#ifdef USE_FINE_GRAINED_LOCK
                spin_unlock(&e->env_lock);
#endif
                goto retry;
            }
            curenv->env_ipc_perm = e->env_ipc_pending_perm;
        } else {
            curenv->env_ipc_perm = 0;
        }
        curenv->env_ipc_value = e->env_ipc_pending_value;
        curenv->env_ipc_from = e->env_id;
        e->env_tf.tf_regs.reg_eax = 0;

        barrier();
        e->env_status = ENV_RUNNABLE;

#ifdef USE_FINE_GRAINED_LOCK
        spin_unlock(&e->env_lock);
#endif
        return 0;
}

// Handling lock in fast system call path.
int32_t
syscall_wrapper(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    int32_t r;

#ifdef USE_FINE_GRAINED_LOCK
    spin_lock(&curenv->env_lock);
#else
    lock_kernel();
#endif
    r = syscall(syscallno, a1, a2, a3, a4, 0);
#ifdef USE_FINE_GRAINED_LOCK
    spin_unlock(&curenv->env_lock);
#else
    unlock_kernel();
#endif
    return r;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
        int32_t r = 0;

	switch (syscallno) {
        case SYS_cputs:
            sys_cputs((const char *) a1, (size_t) a2);
            break;
        case SYS_cgetc:
            r = sys_cgetc();
            break;
        case SYS_getenvid:
            r = sys_getenvid();
            break;
        case SYS_env_destroy:
            r = sys_env_destroy((envid_t) a1);
            break;
        case SYS_page_alloc:
            r = sys_page_alloc((envid_t) a1, (void *) a2, a3);
            break;
        case SYS_page_map:
            r = sys_page_map((envid_t) a1, (void *) a2, (envid_t) a3,
                    (void *) a4, a5);
            break;
        case SYS_page_unmap:
            r = sys_page_unmap((envid_t) a1, (void *) a2);
            break;
        case SYS_exofork:
            r = sys_exofork();
            break;
        case SYS_env_set_status:
            r = sys_env_set_status((envid_t) a1, a2);
            break;
        case SYS_env_set_pgfault_upcall:
            r = sys_env_set_pgfault_upcall((envid_t) a1, (void *) a2);
            break;
        case SYS_yield:
            sched_yield();
            break;
        case SYS_ipc_try_send:
            r = sys_ipc_try_send((envid_t) a1, a2, (void *) a3, a4);
            break;
        case SYS_ipc_recv:
            r = sys_ipc_recv((void *) a1);
            break;
	default:
            panic("Unknown syscallno: %d", syscallno);
	}

        return r;
}

