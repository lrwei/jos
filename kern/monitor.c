// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display current stack backtrace", mon_backtrace },
	{ "continue", "Resume execution of suspended program", mon_continue },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    // Your code here.
    uint32_t ebp = read_ebp();
    struct Eipdebuginfo info;
    int n_args = 3;

    cprintf("Stack backtrace:\n");
    while (ebp != 0) {
        /* ptr     -> &ptr[1]        -> &ptr[2]   -> ...      -> stack top
         * old_ebp    return address    first arg    last arg */
        uint32_t *ptr = (uint32_t *) ebp;

        cprintf("  ebp %08x  eip %08x  args", ebp, ptr[1]);
        if (n_args == 0) {
            cprintf(" (void)");
        } else {
            for (int i = 2; i < 2 + n_args; i++) {
                cprintf(" %08x", ptr[i]);
            }
        }
        assert(!debuginfo_eip(ptr[1], &info));
        cprintf("\n    %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
                info.eip_fn_namelen, info.eip_fn_name,
                ptr[1] - info.eip_fn_addr);
        n_args = info.eip_fn_narg;
        ebp = ptr[0];
    }

    return 0;
}

int mon_continue(int argc, char **argv, struct Trapframe *tf)
{
    if (!tf) {
        cprintf("No pending environment, command ignored.\n");
        return 0;
    }

    switch (tf->tf_trapno) {
    case T_DEBUG:
        /* Do something else.  */
    case T_BRKPT:
        tf->tf_eflags |= FL_TF;
        break;
    default:
        cprintf("No pending debug exception, command ignored.\n");
        return 0;
    }

    if (argc == 1) {
        tf->tf_eflags &= ~FL_TF;
    }

    assert(curenv);
    assert(curenv->env_status == ENV_RUNNING);
    env_run(curenv);
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
