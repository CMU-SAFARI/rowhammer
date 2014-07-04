/* Pattern extension for memtest86
 *
 * Generates patterns for the Linux kernel's BadRAM extension that avoids
 * allocation of faulty pages.
 *
 * Released under version 2 of the Gnu Public License.
 *
 * By Rick van Rein, vanrein@zonnet.nl
 * ----------------------------------------------------
 * MemTest86+ V1.60 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.x86-secret.com - http://www.memtest.org 
 */


#include "test.h"


/*
 * DEFAULT_MASK covers a longword, since that is the testing granularity.
 */
#define DEFAULT_MASK ((~0L) << 2)


/* extern struct vars *v; */


/* What it does:
 *  - Keep track of a number of BadRAM patterns in an array;
 *  - Combine new faulty addresses with it whenever possible;
 *  - Keep masks as selective as possible by minimising resulting faults;
 *  - Print a new pattern only when the pattern array is changed.
 */

#define COMBINE_MASK(a,b,c,d) ((a & b & c & d) | (~a & b & ~c & d))

/* Combine two adr/mask pairs to one adr/mask pair.
 */
void combine (ulong adr1, ulong mask1, ulong adr2, ulong mask2,
		ulong *adr, ulong *mask) {

	*mask = COMBINE_MASK (adr1, mask1, adr2, mask2);

	*adr  = adr1 | adr2;
	*adr &= *mask;	// Normalise, no fundamental need for this
}

/* Count the number of addresses covered with a mask.
 */
ulong addresses (ulong mask) {
	ulong ctr=1;
	int i=32;
	while (i-- > 0) {
		if (! (mask & 1)) {
			ctr += ctr;
		}
		mask >>= 1;
	}
	return ctr;
}

/* Count how much more addresses would be covered by adr1/mask1 when combined
 * with adr2/mask2.
 */
ulong combicost (ulong adr1, ulong mask1, ulong adr2, ulong mask2) {
	ulong cost1=addresses (mask1);
	ulong tmp, mask;
	combine (adr1, mask1, adr2, mask2, &tmp, &mask);
	return addresses (mask) - cost1;
}

/* Find the cheapest array index to extend with the given adr/mask pair.
 * Return -1 if nothing below the given minimum cost can be found.
 */
int cheapindex (ulong adr1, ulong mask1, ulong mincost) {
	int i=v->numpatn;
	int idx=-1;
	while (i-- > 0) {
		ulong tmpcost=combicost(v->patn[i].adr, v->patn[i].mask, adr1, mask1);
		if (tmpcost < mincost) {
			mincost=tmpcost;
			idx=i;
		}
	}
	return idx;
}

/* Try to find a relocation index for idx if it costs nothing.
 * Return -1 if no such index exists.
 */
int relocateidx (int idx) {
	ulong adr =v->patn[idx].adr;
	ulong mask=v->patn[idx].mask;
	int new;
	v->patn[idx].adr ^= ~0L;	// Never select idx
	new=cheapindex (adr, mask, 1+addresses (mask));
	v->patn[idx].adr = adr;
	return new;
}

/* Relocate the given index idx only if free of charge.
 * This is useful to combine to `neighbouring' sections to integrate.
 * Inspired on the Buddy memalloc principle in the Linux kernel.
 */
void relocateiffree (int idx) {
	int newidx=relocateidx (idx);
	if (newidx>=0) {
		ulong cadr, cmask;
		combine (v->patn [newidx].adr, v->patn[newidx].mask,
		         v->patn [   idx].adr, v->patn[   idx].mask,
			 &cadr, &cmask);
		v->patn[newidx].adr =cadr;
		v->patn[newidx].mask=cmask;
		if (idx < --v->numpatn) {
			v->patn[idx].adr =v->patn[v->numpatn].adr;
			v->patn[idx].mask=v->patn[v->numpatn].mask;
		}
		relocateiffree (newidx);
	}
}

/* Insert a single faulty address in the pattern array.
 * Return 1 only if the array was changed.
 */
int insertaddress (ulong adr) {
	if (cheapindex (adr, DEFAULT_MASK, 1L) != -1)
		return 0;

	if (v->numpatn < BADRAM_MAXPATNS) {
		v->patn[v->numpatn].adr =adr;
		v->patn[v->numpatn].mask=DEFAULT_MASK;
		v->numpatn++;
		relocateiffree (v->numpatn-1);
	} else {
		int idx=cheapindex (adr, DEFAULT_MASK, ~0L);
		ulong cadr, cmask;
		combine (v->patn [idx].adr, v->patn[idx].mask,
		         adr, DEFAULT_MASK, &cadr, &cmask);
		v->patn[idx].adr =cadr;
		v->patn[idx].mask=cmask;
		relocateiffree (idx);
	}
	return 1;
}
