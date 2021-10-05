#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <inc/types.h>
#include <kern/pci.h>

/* Selected Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */

#define E1000_STATUS	0x00008  /* Device Status - RO */

/* Transmit Control */
#define E1000_TCTL	0x00400  /* TX Control - RW */
# define E1000_TCTL_RST		0x00000001    /* software reset */
# define E1000_TCTL_EN		0x00000002    /* enable tx */
# define E1000_TCTL_BCE		0x00000004    /* busy check enable */
# define E1000_TCTL_PSP		0x00000008    /* pad short packets */
# define E1000_TCTL_CT		0x00000ff0    /* collision threshold */
# define E1000_TCTL_COLD	0x003ff000    /* collision distance */
#  define COL_FULL_DUPLEX	((0x40 << 12) | (0x10 << 4))
# define E1000_TCTL_SWXOFF	0x00400000    /* SW Xoff transmission */
# define E1000_TCTL_PBE		0x00800000    /* Packet Burst Enable */
# define E1000_TCTL_RTLC	0x01000000    /* Re-transmit on late collision */
# define E1000_TCTL_NRTU	0x02000000    /* No Re-transmit on underrun */
# define E1000_TCTL_MULR	0x10000000    /* Multiple request support */
#define E1000_TIPG	0x00410  /* TX Inter-packet gap -RW */
# define TIPG_IEEE_802_3	((12 << 20) | (8 << 10) | 10)
#define E1000_TDBAL	0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH	0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN	0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH	0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT	0x03818  /* TX Descripotr Tail - RW */

/* Transmit Descriptor */
struct tx_desc {
	uint64_t buffer_addr;	/* Address of the descriptor's data buffer */
	uint16_t length;	/* Data buffer length */
	uint8_t cso;		/* Checksum offset */
	uint8_t cmd;		/* Descriptor control */
#define E1000_TXD_CMD_EOP	0x01	/* End of Packet */
#define E1000_TXD_CMD_IFCS	0x02	/* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC	0x04	/* Insert Checksum */
#define E1000_TXD_CMD_RS	0x08	/* Report Status */
#define E1000_TXD_CMD_RPS	0x10	/* Report Packet Sent */
#define E1000_TXD_CMD_DEXT	0x20	/* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE	0x40	/* Add VLAN tag */
#define E1000_TXD_CMD_IDE	0x80	/* Enable Tidv register */
	uint8_t status;		/* Descriptor status */
#define E1000_TXD_STAT_DD	0x1	/* Descriptor Done */
#define E1000_TXD_STAT_EC	0x2	/* Excess Collisions */
#define E1000_TXD_STAT_LC	0x4	/* Late Collisions */
#define E1000_TXD_STAT_TU	0x8	/* Transmit underrun */
	uint8_t css;		/* Checksum start */
	uint16_t special;
};

/* Transmit Descriptor bit definitions */
#define E1000_TXD_DTYP_D	0x00100000 /* Data Descriptor */
#define E1000_TXD_DTYP_C	0x00000000 /* Context Descriptor */
#define E1000_TXD_POPTS_IXSM	0x01       /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM	0x02       /* Insert TCP/UDP checksum */

#define E1000_TXD_CMD_TCP	0x01000000 /* TCP packet */
#define E1000_TXD_CMD_IP	0x02000000 /* IP packet */
#define E1000_TXD_CMD_TSE	0x04000000 /* TCP Seg enable */
#define E1000_TXD_STAT_TC	0x00000004 /* Tx Underrun */

/* Receive Control */
#define E1000_IMC	0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_RCTL	0x00100  /* RX Control - RW */
# define E1000_RCTL_RST            0x00000001    /* Software reset */
# define E1000_RCTL_EN             0x00000002    /* enable */
# define E1000_RCTL_SBP            0x00000004    /* store bad packet */
# define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
# define E1000_RCTL_MPE            0x00000010    /* multicast promiscuous enab */
# define E1000_RCTL_LPE            0x00000020    /* long packet enable */
# define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
# define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
# define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
# define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
# define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
# define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
# define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
# define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
# define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
# define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
# define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
# define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
# define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
# define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
# define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
# define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
# define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
# define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
# define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
# define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
# define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
# define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
# define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
# define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
# define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
# define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
# define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
# define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
# define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */


#define E1000_RDBAL	0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH	0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN	0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH	0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT	0x02818  /* RX Descriptor Tail - RW */
#define E1000_MTA	0x05200  /* Multicast Table Array - RW Array */
#define E1000_RAL0	0x05400  /* Receive Address Low (0) - RW */
#define E1000_RAH0	0x05404  /* Receive Address High (0) - RW */
#define E1000_RAH_AV	0x80000000  /* Receive descriptor valid */


struct rx_desc {
	uint64_t buffer_addr;
	uint16_t length;
	uint16_t csum;
	uint8_t status;
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */
#define E1000_RXD_STAT_IXSM     0x04    /* Ignore checksum */
#define E1000_RXD_STAT_VP       0x08    /* IEEE VLAN Packet */
#define E1000_RXD_STAT_UDPCS    0x10    /* UDP xsum caculated */
#define E1000_RXD_STAT_TCPCS    0x20    /* TCP xsum calculated */
#define E1000_RXD_STAT_IPCS     0x40    /* IP xsum calculated */
#define E1000_RXD_STAT_PIF      0x80    /* passed in-exact filter */
	uint8_t error;
	uint16_t special;
};
/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */
#define E1000_RXD_STAT_IXSM     0x04    /* Ignore checksum */
#define E1000_RXD_STAT_VP       0x08    /* IEEE VLAN Packet */
#define E1000_RXD_STAT_UDPCS    0x10    /* UDP xsum caculated */
#define E1000_RXD_STAT_TCPCS    0x20    /* TCP xsum calculated */
#define E1000_RXD_STAT_IPCS     0x40    /* IP xsum calculated */
#define E1000_RXD_STAT_PIF      0x80    /* passed in-exact filter */
#define E1000_RXD_STAT_IPIDV    0x200   /* IP identification valid */
#define E1000_RXD_STAT_UDPV     0x400   /* Valid UDP checksum */
#define E1000_RXD_STAT_ACK      0x8000  /* ACK Packet indication */
#define E1000_RXD_ERR_CE        0x01    /* CRC Error */
#define E1000_RXD_ERR_SE        0x02    /* Symbol Error */
#define E1000_RXD_ERR_SEQ       0x04    /* Sequence Error */
#define E1000_RXD_ERR_CXE       0x10    /* Carrier Extension Error */
#define E1000_RXD_ERR_TCPE      0x20    /* TCP/UDP Checksum Error */
#define E1000_RXD_ERR_IPE       0x40    /* IP Checksum Error */
#define E1000_RXD_ERR_RXE       0x80    /* Rx Data Error */
#define E1000_RXD_SPC_VLAN_MASK 0x0FFF  /* VLAN ID is in lower 12 bits */
#define E1000_RXD_SPC_PRI_MASK  0xE000  /* Priority is in upper 3 bits */
#define E1000_RXD_SPC_PRI_SHIFT 13
#define E1000_RXD_SPC_CFI_MASK  0x1000  /* CFI is bit 12 */
#define E1000_RXD_SPC_CFI_SHIFT 12

#define PCI_VENDER_INTEL	0x8086
#define PCI_DEVICE_E1000	0x100E
int pci_e1000_attach(struct pci_func *f);

#define TX_BUFFER_SIZE		1518
#define RX_BUFFER_SIZE		2048
size_t net_packet_tx(const void *packet, size_t length);
size_t net_packet_rx(uint8_t *buffer);

#endif  // SOL >= 6
