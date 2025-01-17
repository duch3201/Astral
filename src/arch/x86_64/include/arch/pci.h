#ifndef _PCI_H_INCLUDE
#define _PCI_H_INCLUDE

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PCI_DEV_HEADER 0
#define PCI_BRIDGE_HEADER 1
#define PCI_CARDBUS_HEADER 2

#define PCI_CARDBUS_HEADERSIZE 0x48
#define PCI_OTHER_HEADERSIZE 0x40

typedef struct{
	uint16_t vendor;
	uint16_t device;
	uint16_t command;
	uint16_t status;
	uint8_t  revision;
	uint8_t  progif;
	uint8_t  subclass;
	uint8_t  class;
	uint8_t  cachesize;
	uint8_t  latencytimer;
	uint8_t  type;
	uint8_t  bist;
} __attribute__((packed)) pci_common;

typedef struct{
	pci_common common;
	uint32_t BAR[6];
	uint32_t cardbuscis;
	uint16_t subsystemvendor;
	uint16_t subsystem;
	uint32_t rombase;
	uint8_t  capabilities;
	uint8_t  r[3];
	uint32_t reserved;
	uint8_t  interruptline;
	uint8_t  interruptpin;
	uint8_t  mingrant;
	uint8_t  maxgrant;
} __attribute__((packed)) pci_deviceheader;

typedef struct{	
	uint32_t mlow;
	uint32_t mhi;
	uint32_t data;
	uint32_t ctrl;
} __attribute__((packed)) msixmsg_t;

typedef struct _pci_enumeration{
	struct pci_enumeration* next;
	uint8_t bus;
	uint8_t device;
	uint8_t function;
	pci_common* header;
	union{
		struct{
			int offset;
		} msi;
		struct{
			int offset;
			msixmsg_t* table;
			int entrycount;
		} msix;
	};

} pci_enumeration;

#define PCI_TYPE_MEM 1
#define PCI_MEM_64 2

// bit 0 set if mem; bit 1 set if 64 bit (in case of mem)

static inline int pci_bartype(uint32_t bar){
	
	if(bar & 1)
		return 0;
	

	if(((bar >> 1) & 3) == 2)
		return 3;

	return 1;

}

static inline void* getbarmemaddr(pci_deviceheader* e, int bar){

	int type = pci_bartype(e->BAR[bar]);

	if(type == 0)
		return NULL;

	if(type & PCI_MEM_64)
		return (void*)(((uint64_t)e->BAR[bar] & ~(0xF)) + ((uint64_t)e->BAR[bar + 1] << 32));
	else
		return (void*)((uint64_t)e->BAR[bar] & ~(0xF));
}

#define PCI_COMMAND_IO 1
#define PCI_COMMAND_MEMORY 2
#define PCI_COMMAND_MASTER 4
#define PCI_COMMAND_INTDISABLE 1024

bool pci_msisupport(pci_enumeration* e);
void pci_msienable(pci_enumeration* e);
bool pci_msixsupport(pci_enumeration* e);
void pci_msixenable(pci_enumeration* e);
void pci_msimask(pci_enumeration* e, int which, int val);
void pci_msimaskall(pci_enumeration* e, int val);
void pci_msiadd(pci_enumeration* e, int cpu, int vec, bool edgetrigger, bool deassert);
void pci_msixadd(pci_enumeration* e, int msixvec, int cpu, int vec, bool edgetrigger, bool deassert);

void pci_setcommand(pci_enumeration* e, int which, int val);
pci_enumeration* pci_getdevicecs(int class, int subclass, int n);
pci_enumeration* pci_getdevicecsp(int class, int subclass, int progif, int n);
uint64_t pci_msi_build(uint64_t* data, uint8_t vector, uint8_t processor, uint8_t edgetrigger, uint8_t deassert);
void pci_enumerate();

#endif
