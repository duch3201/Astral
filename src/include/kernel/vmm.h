#ifndef _VMM_H_INCLUDE
#define _VMM_H_INCLUDE

#include <arch/mmu.h>
#include <stdbool.h>
#include <stddef.h>

#define KERNEL_SPACE_START 0xFFFF800000000000
#define KERNEL_SPACE_END   0xFFFFFFFFFFFFFFFF
#define USER_SPACE_START   0
#define USER_SPACE_END     0x00007FFFFFFFFFFF

#define VMM_TYPE_FREE 0
#define VMM_TYPE_ANON 1
#define VMM_TYPE_FILE 2

struct vmm_cacheheader;

typedef struct{
	int lock;
	size_t firstfree;
	struct vmm_cacheheader* next;
	size_t freecount;
} vmm_cacheheader;

struct vmm_mapping;

typedef struct _vmm_mapping {
	struct _vmm_mapping *next;
	struct _vmm_mapping *prev;
	vmm_cacheheader* cache;
	void* start;
	void* end;
	size_t mmuflags;
	size_t type;
	void*  data;
	size_t offset;
} vmm_mapping;


#define VMM_CACHE_ENTRY_COUNT ((PAGE_SIZE - sizeof(vmm_cacheheader)) / sizeof(vmm_mapping))

typedef struct{
	vmm_cacheheader header;
	vmm_mapping mappings[VMM_CACHE_ENTRY_COUNT];
} vmm_cache;

typedef struct {
	vmm_mapping* userstart;
	arch_mmu_tableptr context;
	int lock;
} vmm_context;

void vmm_init();

bool 		vmm_dealwithrequest(void* addr);
bool 		vmm_setused(void* addr, size_t pagec, size_t mmuflags);
bool		vmm_unmap(void* addr, size_t pagec);
bool		vmm_map(void* paddr, void* vaddr, size_t pagec, size_t mmuflags);
void*		vmm_alloc(size_t pagec, size_t mmuflags);
bool		vmm_setfree(void* addr, size_t pagec);
vmm_context*	vmm_newcontext();
#endif
