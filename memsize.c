/* memsize.c - MemTest-86  Version 3.3
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 */

#include "test.h"
#include "defs.h"
#include "config.h"

short e820_nr;
short memsz_mode = SZ_MODE_BIOS;

static ulong alt_mem_k;
static ulong ext_mem_k;
static struct e820entry e820[E820MAX];

ulong p1, p2;
ulong *p;

static void sort_pmap(void);
//static void memsize_bios(void);
static void memsize_820(void);
static void memsize_801(void);
static int sanitize_e820_map(struct e820entry *orig_map,
struct e820entry *new_bios, short old_nr);
static void memsize_linuxbios();

/*
 * Find out how much memory there is.
 */
void mem_size(void)
{
	int i, flag=0;
	v->test_pages = 0;

	/* Get the memory size from the BIOS */
        /* Determine the memory map */
	if (query_linuxbios()) {
		flag = 1;
	} else if (query_pcbios()) {
		flag = 2;
	}

	/* On the first time thru only */
	/* Make a copy of the memory info table so that we can re-evaluate */
	/* The memory map later */
	if (e820_nr == 0 && alt_mem_k == 0 && ext_mem_k == 0) {
		ext_mem_k = mem_info.e88_mem_k;
		alt_mem_k = mem_info.e801_mem_k;
		e820_nr   = mem_info.e820_nr;
		for (i=0; i< mem_info.e820_nr; i++) {
			e820[i].addr = mem_info.e820[i].addr;
			e820[i].size = mem_info.e820[i].size;
			e820[i].type = mem_info.e820[i].type;
		}
	}
	if (flag == 1) {
		memsize_linuxbios();
	} else if (flag == 2) {
		memsize_820();
	}

	/* Guarantee that pmap entries are in ascending order */
	sort_pmap();
	v->plim_lower = 0;
	v->plim_upper = v->pmap[v->msegs-1].end;

	adj_mem();
}

static void sort_pmap(void)
{
	int i, j;
	/* Do an insertion sort on the pmap, on an already sorted
	 * list this should be a O(1) algorithm.
	 */
	for(i = 0; i < v->msegs; i++) {
		/* Find where to insert the current element */
		for(j = i -1; j >= 0; j--) {
			if (v->pmap[i].start > v->pmap[j].start) {
				j++;
				break;
			}
		}
		/* Insert the current element */
		if (i != j) {
			struct pmap temp;
			temp = v->pmap[i];
			memmove(&v->pmap[j], &v->pmap[j+1], 
				(i -j)* sizeof(temp));
			v->pmap[j] = temp;
		}
	}
}
static void memsize_linuxbios(void)
{
	int i, n;
	/* Build the memory map for testing */
	n = 0;
	for (i=0; i < e820_nr; i++) {
		unsigned long long end;

		if (e820[i].type != E820_RAM) {
			continue;
		}
		end = e820[i].addr;
		end += e820[i].size;
		v->pmap[n].start = (e820[i].addr + 4095) >> 12;
		v->pmap[n].end = end >> 12;
		v->test_pages += v->pmap[n].end - v->pmap[n].start;
		n++;
	}
	v->msegs = n;
}
static void memsize_820()
{
	int i, n, nr;
	struct e820entry nm[E820MAX];
	unsigned long long start;
	unsigned long long end;

	/* Clean up, adjust and copy the BIOS-supplied E820-map. */
	nr = sanitize_e820_map(e820, nm, e820_nr);

	/* If there is not a good 820 map use the BIOS 801/88 info */
	if (nr < 1 || nr > E820MAX) {
		memsize_801();
		return;
	}

	/* Build the memory map for testing */
	n = 0;
	for (i=0; i<nr; i++) {
		if (nm[i].type == E820_RAM || nm[i].type == E820_ACPI) {
			start = nm[i].addr;
			end = start + nm[i].size;

			/* Don't ever use memory between 640 and 1024k */
			if (start > RES_START && start < RES_END) {
				if (end < RES_END) {
					continue;
				}
				start = RES_END;
			}
			if (end > RES_START && end < RES_END) {
				end = RES_START;
			}
			v->pmap[n].start = (start + 4095) >> 12;
			v->pmap[n].end = end >> 12;
			v->test_pages += v->pmap[n].end - v->pmap[n].start;
			n++;
#if 0			
	 		int epmap = 0;
	 		int lpmap = 0;
	 		if(n > 12) { epmap = 34; lpmap = -12; }
			hprint (11+n+lpmap,0+epmap,v->pmap[n-1].start);
			hprint (11+n+lpmap,10+epmap,v->pmap[n-1].end);
			hprint (11+n+lpmap,20+epmap,v->pmap[n-1].end - v->pmap[n-1].start);
			dprint (11+n+lpmap,30+epmap,nm[i].type,0,0);	
#endif				
		}
	}
	v->msegs = n;
}
	
static void memsize_801(void)
{
	ulong mem_size;

	/* compare results from 88 and 801 methods and take the greater */
	/* These sizes are for extended memory in 1k units. */

	if (alt_mem_k < ext_mem_k) {
		mem_size = ext_mem_k;
	} else {
		mem_size = alt_mem_k;
	}
	/* First we map in the first 640k */
	v->pmap[0].start = 0;
	v->pmap[0].end = RES_START >> 12;
	v->test_pages = RES_START >> 12;

	/* Now the extended memory */
	v->pmap[1].start = (RES_END + 4095) >> 12;
	v->pmap[1].end = (mem_size + 1024) >> 2;
	v->test_pages += mem_size >> 2;
	v->msegs = 2;
}

/*
 * Sanitize the BIOS e820 map.
 *
 * Some e820 responses include overlapping entries.  The following 
 * replaces the original e820 map with a new one, removing overlaps.
 *
 */
static int sanitize_e820_map(struct e820entry *orig_map, struct e820entry *new_bios,
	short old_nr)
{
	struct change_member {
		struct e820entry *pbios; /* pointer to original bios entry */
		unsigned long long addr; /* address for this change point */
	};
	struct change_member change_point_list[2*E820MAX];
	struct change_member *change_point[2*E820MAX];
	struct e820entry *overlap_list[E820MAX];
	struct e820entry biosmap[E820MAX];
	struct change_member *change_tmp;
	ulong current_type, last_type;
	unsigned long long last_addr;
	int chgidx, still_changing;
	int overlap_entries;
	int new_bios_entry;
	int i;

	/*
		Visually we're performing the following (1,2,3,4 = memory types)...
		Sample memory map (w/overlaps):
		   ____22__________________
		   ______________________4_
		   ____1111________________
		   _44_____________________
		   11111111________________
		   ____________________33__
		   ___________44___________
		   __________33333_________
		   ______________22________
		   ___________________2222_
		   _________111111111______
		   _____________________11_
		   _________________4______

		Sanitized equivalent (no overlap):
		   1_______________________
		   _44_____________________
		   ___1____________________
		   ____22__________________
		   ______11________________
		   _________1______________
		   __________3_____________
		   ___________44___________
		   _____________33_________
		   _______________2________
		   ________________1_______
		   _________________4______
		   ___________________2____
		   ____________________33__
		   ______________________4_
	*/
	/* First make a copy of the map */
	for (i=0; i<old_nr; i++) {
		biosmap[i].addr = orig_map[i].addr;
		biosmap[i].size = orig_map[i].size;
		biosmap[i].type = orig_map[i].type;
	}

	/* bail out if we find any unreasonable addresses in bios map */
	for (i=0; i<old_nr; i++) {
		if (biosmap[i].addr + biosmap[i].size < biosmap[i].addr)
			return 0;
	}

	/* create pointers for initial change-point information (for sorting) */
	for (i=0; i < 2*old_nr; i++)
		change_point[i] = &change_point_list[i];

	/* record all known change-points (starting and ending addresses) */
	chgidx = 0;
	for (i=0; i < old_nr; i++)	{
		change_point[chgidx]->addr = biosmap[i].addr;
		change_point[chgidx++]->pbios = &biosmap[i];
		change_point[chgidx]->addr = biosmap[i].addr + biosmap[i].size;
		change_point[chgidx++]->pbios = &biosmap[i];
	}

	/* sort change-point list by memory addresses (low -> high) */
	still_changing = 1;
	while (still_changing)	{
		still_changing = 0;
		for (i=1; i < 2*old_nr; i++)  {
			/* if <current_addr> > <last_addr>, swap */
			/* or, if current=<start_addr> & last=<end_addr>, swap */
			if ((change_point[i]->addr < change_point[i-1]->addr) ||
				((change_point[i]->addr == change_point[i-1]->addr) &&
				 (change_point[i]->addr == change_point[i]->pbios->addr) &&
				 (change_point[i-1]->addr != change_point[i-1]->pbios->addr))
			   )
			{
				change_tmp = change_point[i];
				change_point[i] = change_point[i-1];
				change_point[i-1] = change_tmp;
				still_changing=1;
			}
		}
	}

	/* create a new bios memory map, removing overlaps */
	overlap_entries=0;	 /* number of entries in the overlap table */
	new_bios_entry=0;	 /* index for creating new bios map entries */
	last_type = 0;		 /* start with undefined memory type */
	last_addr = 0;		 /* start with 0 as last starting address */
	/* loop through change-points, determining affect on the new bios map */
	for (chgidx=0; chgidx < 2*old_nr; chgidx++)
	{
		/* keep track of all overlapping bios entries */
		if (change_point[chgidx]->addr == change_point[chgidx]->pbios->addr)
		{
			/* add map entry to overlap list (> 1 entry implies an overlap) */
			overlap_list[overlap_entries++]=change_point[chgidx]->pbios;
		}
		else
		{
			/* remove entry from list (order independent, so swap with last) */
			for (i=0; i<overlap_entries; i++)
			{
				if (overlap_list[i] == change_point[chgidx]->pbios)
					overlap_list[i] = overlap_list[overlap_entries-1];
			}
			overlap_entries--;
		}
		/* if there are overlapping entries, decide which "type" to use */
		/* (larger value takes precedence -- 1=usable, 2,3,4,4+=unusable) */
		current_type = 0;
		for (i=0; i<overlap_entries; i++)
			if (overlap_list[i]->type > current_type)
				current_type = overlap_list[i]->type;
		/* continue building up new bios map based on this information */
		if (current_type != last_type)	{
			if (last_type != 0)	 {
				new_bios[new_bios_entry].size =
					change_point[chgidx]->addr - last_addr;
				/* move forward only if the new size was non-zero */
				if (new_bios[new_bios_entry].size != 0)
					if (++new_bios_entry >= E820MAX)
						break; 	/* no more space left for new bios entries */
			}
			if (current_type != 0)	{
				new_bios[new_bios_entry].addr = change_point[chgidx]->addr;
				new_bios[new_bios_entry].type = current_type;
				last_addr=change_point[chgidx]->addr;
			}
			last_type = current_type;
		}
	}
	return(new_bios_entry);
}
