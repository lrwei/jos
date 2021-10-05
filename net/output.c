#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (true) {
		envid_t whom;
		int perm, r;

		r = ipc_recv(&whom, &nsipcbuf, &perm);
		assert(r == NSREQ_OUTPUT);
		assert(whom == ns_envid);
		assert(perm & PTE_P);

		for (size_t off = 0, sent; off < nsipcbuf.pkt.jp_len; off += sent) {
			sent = sys_net_try_send(&nsipcbuf.pkt.jp_data[off],
						nsipcbuf.pkt.jp_len - off);
			if (sent == 0)
				sys_yield();
		}
	}
}
