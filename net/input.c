#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	// With the current environment forked from serv, nsipcbuf is still
	// marked COW, which prevents it from being used in sys_net_try_recv.
	assert(!sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W | PTE_P));
	while (true) {
		while (!(nsipcbuf.pkt.jp_len =
			 sys_net_try_recv((uint8_t *) nsipcbuf.pkt.jp_data)))
			sys_yield();
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_P);
		while (pageref(&nsipcbuf) > 1) {
			sys_yield();
		}
	}
}
