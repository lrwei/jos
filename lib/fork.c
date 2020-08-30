// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
        assert(err & FEC_WR);
        assert(uvpt[PGNUM(addr)] & PTE_COW);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
        assert(!sys_page_alloc(0, (void *) PFTEMP, PTE_U | PTE_W | PTE_P));
        memcpy((void *) PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
        assert(!sys_page_map(0, (void *) PFTEMP, 0, ROUNDDOWN(addr, PGSIZE),
                PTE_U | PTE_W | PTE_P));
        assert(!sys_page_unmap(0, (void *) PFTEMP));
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
        int perm = PTE_U | PTE_P;
        void *addr = (void *) (pn * PGSIZE);

        assert(uvpt[pn] & PTE_P);
        if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
            perm |= PTE_COW;
            // The order here matters. Because COW mark on stack page is
            // volatile and will be cleared immediately due to parent's
            // calling and returning from function. So if we map the parent's
            // stack page first, what the poor child actually gets will be a
            // constantly changing stack page.
            assert(!sys_page_map(0, addr, envid, addr, perm));
            assert(!sys_page_map(0, addr, 0, addr, perm));
        } else {
            assert(!sys_page_map(0, addr, envid, addr, perm));
        }

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
        envid_t envid;

        envid = sys_exofork();
        assert(envid >= 0);

        if (envid == 0) {
            thisenv = &envs[ENVX(sys_getenvid())];
            return 0;
        }

        set_pgfault_handler(pgfault);

        for (uintptr_t addr = UTEXT; addr < USTACKTOP; addr += PGSIZE) {
            if (!(uvpd[PDX(addr)] & PTE_P)) {
                addr += PTSIZE - PGSIZE;
                continue;
            } else if (!(uvpt[PGNUM(addr)] & PTE_P)) {
                continue;
            }
            duppage(envid, PGNUM(addr));
        }

        assert(!sys_page_alloc(envid, (void *) UXSTACKTOP - PGSIZE,
                    PTE_U | PTE_W | PTE_P));
        assert(!sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall));
        assert(!sys_env_set_status(envid, ENV_RUNNABLE));

        return envid;
}

// Challenge!
int
sfork(void)
{
        envid_t envid;
        uintptr_t addr;

        envid = sys_exofork();
        assert(envid >= 0);

        if (envid == 0) {
            thisenv = &envs[ENVX(sys_getenvid())];
            return 0;
        }

        for (addr = UTEXT; addr < USTACKTOP - PGSIZE; addr += PGSIZE) {
            if (!(uvpd[PDX(addr)] & PTE_P)) {
                addr += PTSIZE - PGSIZE;
                continue;
            } else if (!(uvpt[PGNUM(addr)] & PTE_P)) {
                continue;
            }

            // Only copy page table entry.
            assert(!sys_page_map(0, (void *) addr, envid, (void *) addr,
                        uvpt[PGNUM(addr)] & (PTE_U | PTE_W | PTE_P)));
        }

        // Allocate and copy the stack page.
        assert(!sys_page_alloc(0, (void *) PFTEMP, PTE_U | PTE_W | PTE_P));
        memcpy((void *) PFTEMP, (void *) USTACKTOP - PGSIZE, PGSIZE);
        assert(!sys_page_map(0, (void *) PFTEMP, envid,
                    (void *) USTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P));
        assert(!sys_page_unmap(0, (void *) PFTEMP));

        assert(!sys_env_set_status(envid, ENV_RUNNABLE));
        return envid;
}
