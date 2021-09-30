// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

static int copy_page_to(envid_t envid, void *va, int perm)
{
	int r;

	r = sys_page_alloc(0, (void *) PFTEMP, perm);
	if (r < 0)
		goto exit;
	memmove((void *) PFTEMP, va, PGSIZE);
	r = sys_page_map(0, (void *) PFTEMP, envid, va, perm);
	if (r < 0)
		goto exit;
	assert(!sys_page_unmap(0, (void *) PFTEMP));
exit:
	return r;
}

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
		goto exit;

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if (copy_page_to(0, ROUNDDOWN(addr, PGSIZE), PTE_U | PTE_W | PTE_P) < 0)
		goto exit;
	return;
exit:
	cprintf("[%08x] pgfault: Can't handle page fault happened at"
		"0x%08x, eip: 0x%08x. Exiting...\n", thisenv->env_id,
		utf->utf_fault_va, utf->utf_eip);
	sys_env_destroy(0);
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
	void *va = (void *) (pn << PGSHIFT);
	int r;

	// LAB 4: Your code here.
	if (!(uvpt[pn] & PTE_COW) && !(uvpt[pn] & PTE_W))
		return sys_page_map(0, va, envid, va, PTE_U | PTE_P);

	r = sys_page_map(0, va, envid, va, PTE_COW | PTE_U | PTE_P);
	if (r < 0)
		return r;
	r = sys_page_map(0, va, 0, va, PTE_COW | PTE_U | PTE_P);
	return r;
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
	int r;

	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if (envid < 0)
		return envid;
	else if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (uintptr_t addr = 0; addr < UTOP; addr += PGSIZE) {
		if (!(uvpd[PDX(addr)] & PTE_P)) {
			addr += PTSIZE - PGSIZE;
			continue;
		}
		if (!(uvpt[PGNUM(addr)] & PTE_P))
			continue;
		if (addr == USTACKTOP - PGSIZE || addr == UXSTACKTOP - PGSIZE)
			continue;
		r = duppage(envid, PGNUM(addr));
		if (r < 0)
			goto exit;
	}

	r = copy_page_to(envid, (void *) USTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P);
	if (r < 0)
		goto exit;
	r = sys_page_alloc(envid, (void *) UXSTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P);
	if (r < 0)
		goto exit;
	r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
	if (r < 0)
		goto exit;
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0)
		goto exit;
	return envid;
exit:
	sys_env_destroy(envid);
	return r;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
