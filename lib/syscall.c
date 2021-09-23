// System call stubs.

#include <inc/syscall.h>
#include <inc/lib.h>

static inline int32_t
syscall(int num, int check, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t ret;

	// Generic system call: pass system call number in AX,
	// up to five parameters in DX, CX, BX, DI, SI.
	// Interrupt kernel with T_SYSCALL.
	//
	// The "volatile" tells the assembler not to optimize
	// this instruction away just because we don't use the
	// return value.
	//
	// The last clause tells the assembler that this can
	// potentially change the condition codes and arbitrary
	// memory locations.

	asm volatile("int %1\n"
		     : "=a" (ret)
		     : "i" (T_SYSCALL),
		       "a" (num),
		       "d" (a1),
		       "c" (a2),
		       "b" (a3),
		       "D" (a4),
		       "S" (a5)
		     : "cc", "memory");

	if(check && ret > 0)
		panic("syscall %d returned %d (> 0)", num, ret);

	return ret;
}

static inline int32_t __attribute__((unused))
syscall2(int num, int check, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	int32_t ret;

	// eax                - syscall number
	// edx, ecx, ebx, edi - arg1, arg2, arg3, arg4
	// esi                - return pc
	// ebp                - return esp
	// esp                - trashed by sysenter
	asm volatile("pushl %%ebp\n"
		     "movl %%esp, %%ebp\n"
		     "leal 1f, %%esi\n"
		     "sysenter\n"
		     "1:\n"
		     "popl %%ebp\n"
		     : "=a" (ret)
		     : "a" (num), "d" (a1), "c" (a2), "b" (a3), "D" (a4)
		     : "cc", "memory", "esi");

	if(check && ret > 0)
		panic("syscall %d returned %d (> 0)", num, ret);
	return ret;
}

void
sys_cputs(const char *s, size_t len)
{
	syscall2(SYS_cputs, 0, (uint32_t)s, len, 0, 0);
}

int
sys_cgetc(void)
{
	return syscall2(SYS_cgetc, 0, 0, 0, 0, 0);
}

int
sys_env_destroy(envid_t envid)
{
	return syscall2(SYS_env_destroy, 1, envid, 0, 0, 0);
}

envid_t
sys_getenvid(void)
{
	 return syscall2(SYS_getenvid, 0, 0, 0, 0, 0);
}

