/* vmem.c - MemTest-86 
 *
 * Virtual memory handling (PAE)
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 */
#include "stdint.h"
#include "test.h"
#include "cpuid.h"

extern struct cpu_ident cpu_id;

static unsigned long mapped_win = 1;
void paging_off(void)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__ (
		/* Disable paging */
		"movl %%cr0, %%eax\n\t"
		"andl $0x7FFFFFFF, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		: :
		: "ax"
		);
}

static void paging_on(void *pdp)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__(
		/* Load the page table address */
		"movl %0, %%cr3\n\t"
		/* Enable paging */
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		:
		: "r" (pdp)
		: "ax"
		);
}

static void paging_on_lm(void *pml)
{
	if (!cpu_id.fid.bits.pae)
		return;
	__asm__ __volatile__(
		/* Load the page table address */
		"movl %0, %%cr3\n\t"
		/* Enable paging */
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		:
		: "r" (pml)
		: "ax"
		);
}

int map_page(unsigned long page)
{
	unsigned long i;
	struct pde {
		unsigned long addr_lo;
		unsigned long addr_hi;
	};
	extern unsigned char pdp[];
	extern unsigned char pml4[];
	extern struct pde pd2[];
	unsigned long win = page >> 19;

	/* Less than 2 GB so no mapping is required */
	if (win == 0) {
		return 0;
	}
	if (cpu_id.fid.bits.pae == 0) {
		/* Fail, we don't have PAE */
		return -1;
	}
	if (cpu_id.fid.bits.lm == 0 && (page > 0x1000000)) {
		 /* Fail, we want an address that is out of bounds (> 64GB)
		 *  for PAE and no long mode (ie. 32 bit CPU).
		 */
		return -1;
	}
	/* Compute the page table entries... */
	for(i = 0; i < 1024; i++) {
		/*-----------------10/30/2004 12:37PM---------------
		 * 0xE3 --
		 * Bit 0 = Present bit.      1 = PDE is present
		 * Bit 1 = Read/Write.       1 = memory is writable
		 * Bit 2 = Supervisor/User.  0 = Supervisor only (CPL 0-2)
		 * Bit 3 = Writethrough.     0 = writeback cache policy
		 * Bit 4 = Cache Disable.    0 = page level cache enabled
		 * Bit 5 = Accessed.         1 = memory has been accessed.
		 * Bit 6 = Dirty.            1 = memory has been written to.
		 * Bit 7 = Page Size.        1 = page size is 2 MBytes
		 * --------------------------------------------------*/
		pd2[i].addr_lo = ((win & 1) << 31) + ((i & 0x3ff) << 21) + 0xE3;
		pd2[i].addr_hi = (win >> 1);
	}
	paging_off();
	if (cpu_id.fid.bits.lm == 1) {
		paging_on_lm(pml4);
	} else {
		paging_on(pdp);
	}
	mapped_win = win;
	return 0;
}

void *mapping(unsigned long page_addr)
{
	void *result;
	if (page_addr < 0x80000) {
		/* If the address is less than 1GB directly use the address */
		result = (void *)(page_addr << 12);
	}
	else {
		unsigned long alias;
		alias = page_addr & 0x7FFFF;
		alias += 0x80000;
		result = (void *)(alias << 12);
	}
	return result;
}

void *emapping(unsigned long page_addr)
{
	void *result;
	result = mapping(page_addr -1);
	/* Fill in the low address bits */
	result = ((unsigned char *)result) + 0xffc;
	return result;
}

unsigned long page_of(void *addr)
{
	unsigned long page;
	page = ((unsigned long)addr) >> 12;
	if (page >= 0x80000) {
		page &= 0x7FFFF;
		page += mapped_win << 19;
	}
#if 0
	cprint(LINE_SCROLL -2, 0, "page_of(        )->            ");
	hprint(LINE_SCROLL -2, 8, ((unsigned long)addr));
	hprint(LINE_SCROLL -2, 20, page);
#endif	
	return page;
}
