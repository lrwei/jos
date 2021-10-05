#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/string.h>

static volatile uint32_t *e1000;

static inline uint32_t
e1000r(uint32_t index)
{
	return e1000[index >> 2];
}

static inline void
e1000w(uint32_t index, uint32_t value)
{
	e1000[index >> 2] = value;
}

#define TX_QUEUE_SIZE		64
#define RX_QUEUE_SIZE		128
_Static_assert(TX_QUEUE_SIZE % 8 == 0 && RX_QUEUE_SIZE % 8 == 0);
static struct tx_desc tx_queue[TX_QUEUE_SIZE];
static uint8_t tx_buffer[TX_QUEUE_SIZE][TX_BUFFER_SIZE];
static struct rx_desc rx_queue[RX_QUEUE_SIZE];
static uint8_t rx_buffer[RX_QUEUE_SIZE][RX_BUFFER_SIZE];

static void net_tx_initialize(void)
{
	e1000w(E1000_TDBAL, PADDR(tx_queue));
	e1000w(E1000_TDBAH, 0);
	e1000w(E1000_TDLEN, sizeof(tx_queue));
	e1000w(E1000_TDH, 0);
	e1000w(E1000_TDT, 0);
	e1000w(E1000_TIPG, TIPG_IEEE_802_3);

	/* Statically allocate buffers for transmit descriptors.  */
	for (size_t i = 0; i < TX_QUEUE_SIZE; i++) {
		tx_queue[i].buffer_addr = PADDR(&tx_buffer[i]);
		tx_queue[i].status = E1000_TXD_STAT_DD;
	}
	e1000w(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | COL_FULL_DUPLEX);
}

size_t net_packet_tx(const void *packet, size_t length)
{
	uint32_t tdt = e1000r(E1000_TDT);
	struct tx_desc *tdesc = &tx_queue[tdt];
	size_t n_transmitted = MIN(length, TX_BUFFER_SIZE);

	if (!(tdesc->status & E1000_TXD_STAT_DD))
		return 0;
	memcpy(KADDR(tdesc->buffer_addr), packet, n_transmitted);
	tdesc->length = n_transmitted;
	tdesc->cmd = E1000_TXD_CMD_RS;
	if (length == n_transmitted)
		tdesc->cmd |= E1000_TXD_CMD_EOP;
	tdesc->status = 0;
	e1000w(E1000_TDT, ++tdt == TX_QUEUE_SIZE ? 0 : tdt);
	return n_transmitted;
}

static void net_rx_initialize(void)
{
	e1000w(E1000_RDBAL, PADDR(rx_queue));
	e1000w(E1000_RDBAH, 0);
	e1000w(E1000_RDLEN, sizeof(rx_queue));
	e1000w(E1000_RDH, 0);
	e1000w(E1000_RDT, RX_QUEUE_SIZE - 1);
	e1000w(E1000_RAL0, 0x12005452);
	e1000w(E1000_RAH0, 0x5634 | E1000_RAH_AV);
	for (size_t i = 0; i < 128; i++)
		e1000w(E1000_MTA + i * 4, 0);
	e1000w(E1000_IMC, 0xFFFFFFFF);

	/* Statically allocate buffers for receive descriptors.  */
	for (size_t i = 0; i < RX_QUEUE_SIZE; i++) {
		rx_queue[i].buffer_addr = PADDR(&rx_buffer[i]);
		rx_queue[i].status = 0;
	}
	e1000w(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_SZ_2048 | E1000_RCTL_BAM |
			   E1000_RCTL_SECRC);
}

size_t net_packet_rx(uint8_t *buffer)
{
	uint32_t rdt = (e1000r(E1000_RDT) + 1) % RX_QUEUE_SIZE;
	struct rx_desc *rdesc = &rx_queue[rdt];

	if (!(rdesc->status & E1000_RXD_STAT_DD))
		return 0;
	memcpy(buffer, KADDR(rdesc->buffer_addr), rdesc->length);
	rdesc->status = 0;
	e1000w(E1000_RDT, rdt);
	return rdesc->length;
}

// LAB 6: Your driver code here
int
pci_e1000_attach(struct pci_func *f)
{
	pci_func_enable(f);
	e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);
	cprintf("E1000_STATUS: %x\n", e1000r(E1000_STATUS));
	net_tx_initialize();
	net_rx_initialize();
	return 1;
}
