#include "linuxbios_tables.h"
#include "test.h"

static unsigned long ip_compute_csum(void *addr, unsigned long length)
{
	uint16_t *ptr;
	unsigned long sum;
	unsigned long len;
	unsigned long laddr;
	/* compute an ip style checksum */
	laddr = (unsigned long )addr;
	sum = 0;
	if (laddr & 1) {
		uint16_t buffer;
		unsigned char *ptr;
		/* copy the first byte into a 2 byte buffer.
		 * This way automatically handles the endian question
		 * of which byte (low or high) the last byte goes in.
		 */
		buffer = 0;
		ptr = addr;
		memmove(&buffer, ptr, 1);
		sum += buffer;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
		length -= 1;
		addr = ptr +1;
		
	}
	len = length >> 1;
	ptr = addr;
	while (len--) {
		sum += *(ptr++);
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	addr = ptr;
	if (length & 1) {
		uint16_t buffer;
		unsigned char *ptr;
		/* copy the last byte into a 2 byte buffer.
		 * This way automatically handles the endian question
		 * of which byte (low or high) the last byte goes in.
		 */
		buffer = 0;
		ptr = addr;
		memmove(&buffer, ptr, 1);
		sum += buffer;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return (~sum) & 0xFFFF;
	
}

#define for_each_lbrec(head, rec) \
	for(rec = (struct lb_record *)(((char *)head) + sizeof(*head)); \
		(((char *)rec) < (((char *)head) + sizeof(*head) + head->table_bytes))  && \
		(rec->size >= 1) && \
		((((char *)rec) + rec->size) <= (((char *)head) + sizeof(*head) + head->table_bytes)); \
		rec = (struct lb_record *)(((char *)rec) + rec->size)) 
		

static int count_lb_records(struct lb_header *head)
{
	struct lb_record *rec;
	int count;
	count = 0;
	for_each_lbrec(head, rec) {
		count++;
	}
	return count;
}

static struct lb_header * __find_lb_table(unsigned long start, unsigned long end)
{
	unsigned long addr;
	/* For now be stupid.... */
	for(addr = start; addr < end; addr += 16) {
		struct lb_header *head = (struct lb_header *)addr;
		struct lb_record *recs = (struct lb_record *)(addr + sizeof(*head));
		if (memcmp(head->signature, "LBIO", 4) != 0)
			continue;
		if (head->header_bytes != sizeof(*head))
			continue;
		if (ip_compute_csum((unsigned char *)head, sizeof(*head)) != 0)
			continue;
		if (ip_compute_csum((unsigned char *)recs, head->table_bytes) 
			!= head->table_checksum)
			continue;
		if (count_lb_records(head) != head->table_entries)
			continue;
		return head;
	};
	return 0;
}

static struct lb_header * find_lb_table(void)
{
	struct lb_header *head;
	head = 0;
	if (!head) {
		/* First try at address 0 */
		head = __find_lb_table(0x00000, 0x1000);
	}
	if (!head) {
		/* Then try at address 0xf0000 */
		head = __find_lb_table(0xf0000, 0x100000);
	}
	return head;
}

int query_linuxbios(void)
{
	struct lb_header *head;
	struct lb_record *rec;
	struct lb_memory *mem;
	struct lb_forward *forward;
	int i, entries;
	
	head = find_lb_table();
	if (!head) {
		return 0;
	}

	 /* coreboot also can forward the table to the high tables area. */
	 rec = (struct lb_record *)(((char *)head) + sizeof(*head));
	 if (rec->tag == LB_TAG_FORWARD) {
		 forward = (struct lb_forward *)rec;
		 head = (struct lb_header *)(unsigned long)(forward->forward);
		 if (!head) { return 0;	}
	 }

	mem = 0;
	for_each_lbrec(head, rec) {
		if (rec->tag == LB_TAG_MEMORY) {
			mem = (struct lb_memory *)rec;
			break;
		}
	}
	if (!mem) {
		return 1;
	}
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	if (entries == 0)
		return 1;
	mem_info.e820_nr = 0;
	for(i = 0; i < entries; i++) {
		unsigned long long start;
		unsigned long long size;
		unsigned long type;
		if (i >= E820MAX) {
			break;
		}
		start = mem->map[i].start;
		size = mem->map[i].size;
		type = (mem->map[i].type == LB_MEM_RAM)?E820_RAM: E820_RESERVED;
		mem_info.e820[mem_info.e820_nr].addr = start;
		mem_info.e820[mem_info.e820_nr].size = size;
		mem_info.e820[mem_info.e820_nr].type = type;
		mem_info.e820_nr++;
	}
	return 1;
}

