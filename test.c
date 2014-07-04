/* test.c - MemTest-86  Version 3.4
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 * ----------------------------------------------------
 * MemTest86+ V5 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * Thanks to Passmark for calculate_chunk() and various comments !
 */
 
#include "test.h"
#include "config.h"
#include "stdint.h"
#include "cpuid.h"
#include "smp.h"
#include <sys/io.h>

extern struct cpu_ident cpu_id;
extern volatile int    mstr_cpu;
extern volatile int    run_cpus;
extern volatile int    test;
extern volatile int segs, bail;
extern int test_ticks, nticks;
extern struct tseq tseq[];
extern void update_err_counts(void);
extern void print_err_counts(void);
void rand_seed( unsigned int seed1, unsigned int seed2, int me);
ulong rand(int me);
void poll_errors();

static inline ulong roundup(ulong value, ulong mask)
{
	return (value + mask) & ~mask;
}

// start / end - return values for range to test
// me - this threads CPU number
// j - index into v->map for current segment we are testing
// align - number of bytes to align each block to
void calculate_chunk(ulong** start, ulong** end, int me, int j, int makeMultipleOf)
{
	ulong chunk;


	// If we are only running 1 CPU then test the whole block
	if (run_cpus == 1) {
		*start = v->map[j].start;
		*end = v->map[j].end;
	} 
	else{

		// Divide the current segment by the number of CPUs
		chunk = (ulong)v->map[j].end-(ulong)v->map[j].start;
		chunk /= run_cpus;
		
		// Round down to the nearest desired bitlength multiple
		chunk = (chunk + (makeMultipleOf-1)) &  ~(makeMultipleOf-1);

		// Figure out chunk boundaries
		*start = (ulong*)((ulong)v->map[j].start+(chunk*me));
		/* Set end addrs for the highest CPU num to the
			* end of the segment for rounding errors */
		// Also rounds down to boundary if needed, may miss some ram but better than crashing or producing false errors.
		// This rounding probably will never happen as the segments should be in 4096 bytes pages if I understand correctly.
		if (me == mstr_cpu) {
			*end = (ulong*)(v->map[j].end);
		} else {
			*end = (ulong*)((ulong)(*start) + chunk);
			(*end)--;
		}
	}
}

/*
 * Memory address test, walking ones
 */
void addr_tst1(int me)
{
	int i, j, k;
	volatile ulong *p, *pt, *end;
	ulong bad, mask, bank, p1;

	/* Test the global address bits */
	for (p1=0, j=0; j<2; j++) {
        	hprint(LINE_PAT, COL_PAT, p1);

		/* Set pattern in our lowest multiple of 0x20000 */
		p = (ulong *)roundup((ulong)v->map[0].start, 0x1ffff);
		*p = p1;
	
		/* Now write pattern compliment */
		p1 = ~p1;
		end = v->map[segs-1].end;
		for (i=0; i<100; i++) {
			mask = 4;
			do {
				pt = (ulong *)((ulong)p | mask);
				if (pt == p) {
					mask = mask << 1;
					continue;
				}
				if (pt >= end) {
					break;
				}
				*pt = p1;
				if ((bad = *p) != ~p1) {
					ad_err1((ulong *)p, (ulong *)mask,
						bad, ~p1);
					i = 1000;
				}
				mask = mask << 1;
			} while(mask);
		}
		do_tick(me);
		BAILR
	}

	/* Now check the address bits in each bank */
	/* If we have more than 8mb of memory then the bank size must be */
	/* bigger than 256k.  If so use 1mb for the bank size. */
	if (v->pmap[v->msegs - 1].end > (0x800000 >> 12)) {
		bank = 0x100000;
	} else {
		bank = 0x40000;
	}
	for (p1=0, k=0; k<2; k++) {
        	hprint(LINE_PAT, COL_PAT, p1);

		for (j=0; j<segs; j++) {
			p = v->map[j].start;
			/* Force start address to be a multiple of 256k */
			p = (ulong *)roundup((ulong)p, bank - 1);
			end = v->map[j].end;
			/* Redundant checks for overflow */
                        while (p < end && p > v->map[j].start && p != 0) {
				*p = p1;

				p1 = ~p1;
				for (i=0; i<50; i++) {
					mask = 4;
					do {
						pt = (ulong *)
						    ((ulong)p | mask);
						if (pt == p) {
							mask = mask << 1;
							continue;
						}
						if (pt >= end) {
							break;
						}
						*pt = p1;
						if ((bad = *p) != ~p1) {
							ad_err1((ulong *)p,
							    (ulong *)mask,
							    bad,~p1);
							i = 200;
						}
						mask = mask << 1;
					} while(mask);
				}
				if (p + bank > p) {
					p += bank;
				} else {
					p = end;
				}
				p1 = ~p1;
			}
		}
		do_tick(me);
		BAILR
		p1 = ~p1;
	}
}

/*
 * Memory address test, own address
 */
void addr_tst2(int me)
{
	int j, done;
	ulong *p, *pe, *end, *start;

        cprint(LINE_PAT, COL_PAT, "address ");

	/* Write each address with it's own address */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}

/* Original C code replaced with hand tuned assembly code
 *			for (; p <= pe; p++) {
 *				*p = (ulong)p;
 *			}
 */
			asm __volatile__ (
				"jmp L91\n\t"
				".p2align 4,,7\n\t"
				"L90:\n\t"
				"addl $4,%%edi\n\t"
				"L91:\n\t"
				"movl %%edi,(%%edi)\n\t"
				"cmpl %%edx,%%edi\n\t"
				"jb L90\n\t"
				: : "D" (p), "d" (pe)
			);
			p = pe + 1;
		} while (!done);
	}

	/* Each address should have its own address */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
                                pe += SPINSZ;
                        } else {
                                pe = end;
                        }
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
/* Original C code replaced with hand tuned assembly code
 *			for (; p <= pe; p++) {
 *				if((bad = *p) != (ulong)p) {
 *					ad_err2((ulong)p, bad);
 *				}
 *			}
 */
			asm __volatile__ (
				"jmp L95\n\t"
				".p2align 4,,7\n\t"
				"L99:\n\t"
				"addl $4,%%edi\n\t"
				"L95:\n\t"
				"movl (%%edi),%%ecx\n\t"
				"cmpl %%edi,%%ecx\n\t"
				"jne L97\n\t"
				"L96:\n\t"
				"cmpl %%edx,%%edi\n\t"
				"jb L99\n\t"
				"jmp L98\n\t"
			
				"L97:\n\t"
				"pushl %%edx\n\t"
				"pushl %%ecx\n\t"
				"pushl %%edi\n\t"
				"call ad_err2\n\t"
				"popl %%edi\n\t"
				"popl %%ecx\n\t"
				"popl %%edx\n\t"
				"jmp L96\n\t"

				"L98:\n\t"
				: : "D" (p), "d" (pe)
				: "ecx"
			);
			p = pe + 1;
		} while (!done);
	}
}

/*
 * Test all of memory using a "half moving inversions" algorithm using random
 * numbers and their complment as the data pattern. Since we are not able to
 * produce random numbers in reverse order testing is only done in the forward
 * direction.
 */
void movinvr(int me)
{
	int i, j, done, seed1, seed2;
	ulong *p;
	ulong *pe;
	ulong *start,*end;
	ulong xorVal;
	//ulong num, bad;

	/* Initialize memory with initial sequence of random numbers.  */
	if (cpu_id.fid.bits.rdtsc) {
		asm __volatile__ ("rdtsc":"=a" (seed1),"=d" (seed2));
	} else {
		seed1 = 521288629 + v->pass;
		seed2 = 362436069 - v->pass;
	}

	/* Display the current seed */
        if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, seed1);
	rand_seed(seed1, seed2, me);
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
/* Original C code replaced with hand tuned assembly code */
/*
			for (; p <= pe; p++) {
				*p = rand(me);
			}
 */

                        asm __volatile__ (
                                "jmp L200\n\t"
                                ".p2align 4,,7\n\t"
                                "L201:\n\t"
                                "addl $4,%%edi\n\t"
                                "L200:\n\t"
				"pushl %%ecx\n\t" \
                                "call rand\n\t"
				"popl %%ecx\n\t" \
				"movl %%eax,(%%edi)\n\t"
                                "cmpl %%ebx,%%edi\n\t"
                                "jb L201\n\t"
                                : : "D" (p), "b" (pe), "c" (me)
				: "eax"
                        );
			p = pe + 1;
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location.
	 */
	for (i=0; i<2; i++) {
		rand_seed(seed1, seed2, me);
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = start;
			p = start;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Check for overflow */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
/* Original C code replaced with hand tuned assembly code */
				
				/*for (; p <= pe; p++) {
					num = rand(me);
					if (i) {
						num = ~num;
					}
					if ((bad=*p) != num) {
						error((ulong*)p, num, bad);
					}
					*p = ~num;
				}*/

				if (i) {
					xorVal = 0xffffffff;
				} else {
					xorVal = 0;
				}
				asm __volatile__ (
					
                    "pushl %%ebp\n\t"

					// Skip first increment
					"jmp L26\n\t"
					".p2align 4,,7\n\t"

					// increment 4 bytes (32-bits)
					"L27:\n\t"
					"addl $4,%%edi\n\t"

					// Check this byte
					"L26:\n\t"

					// Get next random number, pass in me(edx), random value returned in num(eax)
					// num = rand(me);
					// cdecl call maintains all registers except eax, ecx, and edx
					// We maintain edx with a push and pop here using it also as an input
					// we don't need the current eax value and want it to change to the return value
					// we overwrite ecx shortly after this discarding its current value
					"pushl %%edx\n\t" // Push function inputs onto stack
					"call rand\n\t"
					"popl %%edx\n\t" // Remove function inputs from stack

					// XOR the random number with xorVal(ebx), which is either 0xffffffff or 0 depending on the outer loop
					// if (i) { num = ~num; }
					"xorl %%ebx,%%eax\n\t"

					// Move the current value of the current position p(edi) into bad(ecx)
					// (bad=*p)
					"movl (%%edi),%%ecx\n\t"

					// Compare bad(ecx) to num(eax)
					"cmpl %%eax,%%ecx\n\t"

					// If not equal jump the error case
					"jne L23\n\t"

					// Set a new value or not num(eax) at the current position p(edi)
					// *p = ~num;
					"L25:\n\t"
					"movl $0xffffffff,%%ebp\n\t"
					"xorl %%ebp,%%eax\n\t"
					"movl %%eax,(%%edi)\n\t"

					// Loop until current position p(edi) equals the end position pe(esi)
					"cmpl %%esi,%%edi\n\t"
					"jb L27\n\t"
					"jmp L24\n"

					// Error case
					"L23:\n\t"
					// Must manually maintain eax, ecx, and edx as part of cdecl call convention
					"pushl %%edx\n\t"
					"pushl %%ecx\n\t" // Next three pushes are functions input
					"pushl %%eax\n\t"
					"pushl %%edi\n\t"
					"call error\n\t"
					"popl %%edi\n\t" // Remove function inputs from stack and restore register values
					"popl %%eax\n\t"
					"popl %%ecx\n\t"
					"popl %%edx\n\t"
					"jmp L25\n" 

					"L24:\n\t"
                                        "popl %%ebp\n\t"
					:: "D" (p), "S" (pe), "b" (xorVal),
						 "d" (me)
					: "eax", "ecx"
				);
				p = pe + 1;
			} while (!done);
		}
	}
}

/*
 * Test all of memory using a "moving inversions" algorithm using the
 * pattern in p1 and it's complement in p2.
 */
void movinv1 (int iter, ulong p1, ulong p2, int me)
{
	int i, j, done;
	ulong *p, *pe, len, *start, *end;

	/* Display the current pattern */
        if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, p1);

	/* Initialize memory with the initial pattern.  */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);


		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			len = pe - p + 1;
			if (p == pe ) {
				break;
			}

			//Original C code replaced with hand tuned assembly code
			// seems broken
			/*for (; p <= pe; p++) {
				*p = p1;
			}*/

			asm __volatile__ (
				"rep\n\t" \
				"stosl\n\t"
				: : "c" (len), "D" (p), "a" (p1)
			);

			p = pe + 1;
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down.  */
	for (i=0; i<iter; i++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = start;
			p = start;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Check for overflow */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}

				// Original C code replaced with hand tuned assembly code 
				// seems broken
 				/*for (; p <= pe; p++) {
					if ((bad=*p) != p1) {
 						error((ulong*)p, p1, bad);
 					}
 					*p = p2;
 				}*/

				asm __volatile__ (
					"jmp L2\n\t" \
					".p2align 4,,7\n\t" \
					"L0:\n\t" \
					"addl $4,%%edi\n\t" \
					"L2:\n\t" \
					"movl (%%edi),%%ecx\n\t" \
					"cmpl %%eax,%%ecx\n\t" \
					"jne L3\n\t" \
					"L5:\n\t" \
					"movl %%ebx,(%%edi)\n\t" \
					"cmpl %%edx,%%edi\n\t" \
					"jb L0\n\t" \
					"jmp L4\n" \

					"L3:\n\t" \
					"pushl %%edx\n\t" \
					"pushl %%ebx\n\t" \
					"pushl %%ecx\n\t" \
					"pushl %%eax\n\t" \
					"pushl %%edi\n\t" \
					"call error\n\t" \
					"popl %%edi\n\t" \
					"popl %%eax\n\t" \
					"popl %%ecx\n\t" \
					"popl %%ebx\n\t" \
					"popl %%edx\n\t" \
					"jmp L5\n" \

					"L4:\n\t" \
					:: "a" (p1), "D" (p), "d" (pe), "b" (p2)
					: "ecx"
				);
				p = pe + 1;
			} while (!done);
		}
		for (j=segs-1; j>=0; j--) {
		    calculate_chunk(&start, &end, me, j, 4);
			pe = end;
			p = end;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Check for underflow */
				if (pe - SPINSZ < pe && pe != 0) {
					pe -= SPINSZ;
				} else {
					pe = start;
					done++;
				}

				/* Since we are using unsigned addresses a 
				 * redundent check is required */
				if (pe < start || pe > end) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}

				//Original C code replaced with hand tuned assembly code
				// seems broken
				/*do {
					if ((bad=*p) != p2) {
					error((ulong*)p, p2, bad);
					}
					*p = p1;
				} while (--p >= pe);*/

				asm __volatile__ (
					"jmp L9\n\t"
					".p2align 4,,7\n\t"
					"L11:\n\t"
					"subl $4, %%edi\n\t"
					"L9:\n\t"
					"movl (%%edi),%%ecx\n\t"
					"cmpl %%ebx,%%ecx\n\t"
					"jne L6\n\t"
					"L10:\n\t"
					"movl %%eax,(%%edi)\n\t"
					"cmpl %%edi, %%edx\n\t"
					"jne L11\n\t"
					"jmp L7\n\t"

					"L6:\n\t"
					"pushl %%edx\n\t"
					"pushl %%eax\n\t"
					"pushl %%ecx\n\t"
					"pushl %%ebx\n\t"
					"pushl %%edi\n\t"
					"call error\n\t"
					"popl %%edi\n\t"
					"popl %%ebx\n\t"
					"popl %%ecx\n\t"
					"popl %%eax\n\t"
					"popl %%edx\n\t"
					"jmp L10\n"

					"L7:\n\t"
					:: "a" (p1), "D" (p), "d" (pe), "b" (p2)
					: "ecx"
				);
				p = pe - 1;
			} while (!done);
		}
	}
}

void movinv32(int iter, ulong p1, ulong lb, ulong hb, int sval, int off,int me)
{
	int i, j, k=0, n=0, done;
	ulong *p, *pe, *start, *end, pat = 0, p3;

	p3 = sval << 31;
	/* Display the current pattern */
	if (mstr_cpu == me) hprint(LINE_PAT, COL_PAT, p1);

	/* Initialize memory with the initial pattern.  */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);
		pe = start;
		p = start;
		done = 0;
		k = off;
		pat = p1;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			/* Do a SPINSZ section of memory */
/* Original C code replaced with hand tuned assembly code
 *			while (p <= pe) {
 *				*p = pat;
 *				if (++k >= 32) {
 *					pat = lb;
 *					k = 0;
 *				} else {
 *					pat = pat << 1;
 *					pat |= sval;
 *				}
 *				p++;
 *			}
 */
			asm __volatile__ (
                                "jmp L20\n\t"
                                ".p2align 4,,7\n\t"
                                "L923:\n\t"
                                "addl $4,%%edi\n\t"
                                "L20:\n\t"
                                "movl %%ecx,(%%edi)\n\t"
                                "addl $1,%%ebx\n\t"
                                "cmpl $32,%%ebx\n\t"
                                "jne L21\n\t"
                                "movl %%esi,%%ecx\n\t"
                                "xorl %%ebx,%%ebx\n\t"
                                "jmp L22\n"
                                "L21:\n\t"
                                "shll $1,%%ecx\n\t"
                                "orl %%eax,%%ecx\n\t"
                                "L22:\n\t"
                                "cmpl %%edx,%%edi\n\t"
                                "jb L923\n\t"
                                : "=b" (k), "=c" (pat)
                                : "D" (p),"d" (pe),"b" (k),"c" (pat),
                                        "a" (sval), "S" (lb)
			);
			p = pe + 1;
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down.  */
	for (i=0; i<iter; i++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 64);
			pe = start;
			p = start;
			done = 0;
			k = off;
			pat = p1;
			do {
				do_tick(me);
				BAILR

				/* Check for overflow */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
/* Original C code replaced with hand tuned assembly code
 *				while (1) {
 *					if ((bad=*p) != pat) {
 *						error((ulong*)p, pat, bad);
 *					}
 *					*p = ~pat;
 *					if (p >= pe) break;
 *					p++;
 *
 *					if (++k >= 32) {
 *						pat = lb;
 *						k = 0;
 *					} else {
 *						pat = pat << 1;
 *						pat |= sval;
 *					}
 *				}
 */
				asm __volatile__ (
                                        "pushl %%ebp\n\t"
                                        "jmp L30\n\t"
                                        ".p2align 4,,7\n\t"
                                        "L930:\n\t"
                                        "addl $4,%%edi\n\t"
                                        "L30:\n\t"
                                        "movl (%%edi),%%ebp\n\t"
                                        "cmpl %%ecx,%%ebp\n\t"
                                        "jne L34\n\t"

                                        "L35:\n\t"
                                        "notl %%ecx\n\t"
                                        "movl %%ecx,(%%edi)\n\t"
                                        "notl %%ecx\n\t"
                                        "incl %%ebx\n\t"
                                        "cmpl $32,%%ebx\n\t"
                                        "jne L31\n\t"
                                        "movl %%esi,%%ecx\n\t"
                                        "xorl %%ebx,%%ebx\n\t"
                                        "jmp L32\n"
                                        "L31:\n\t"
                                        "shll $1,%%ecx\n\t"
                                        "orl %%eax,%%ecx\n\t"
					"L32:\n\t"
                                        "cmpl %%edx,%%edi\n\t"
                                        "jb L930\n\t"
                                        "jmp L33\n\t"

                                        "L34:\n\t" \
                                        "pushl %%esi\n\t"
                                        "pushl %%eax\n\t"
                                        "pushl %%ebx\n\t"
                                        "pushl %%edx\n\t"
                                        "pushl %%ebp\n\t"
                                        "pushl %%ecx\n\t"
                                        "pushl %%edi\n\t"
                                        "call error\n\t"
                                        "popl %%edi\n\t"
                                        "popl %%ecx\n\t"
                                        "popl %%ebp\n\t"
                                        "popl %%edx\n\t"
                                        "popl %%ebx\n\t"
                                        "popl %%eax\n\t"
                                        "popl %%esi\n\t"
                                        "jmp L35\n"

                                        "L33:\n\t"
                                        "popl %%ebp\n\t"
                                        : "=b" (k),"=c" (pat)
                                        : "D" (p),"d" (pe),"b" (k),"c" (pat),
                                                "a" (sval), "S" (lb)
				);
				p = pe + 1;
			} while (!done);
		}

                if (--k < 0) {
                        k = 31;
                }
                for (pat = lb, n = 0; n < k; n++) {
                        pat = pat << 1;
                        pat |= sval;
                }
		k++;

		for (j=segs-1; j>=0; j--) {
			calculate_chunk(&start, &end, me, j, 64);
			p = end;
			pe = end;
			done = 0;
			do {
				do_tick(me);
				BAILR

				/* Check for underflow */
                                if (pe - SPINSZ < pe && pe != 0) {
                                        pe -= SPINSZ;
                                } else {
                                        pe = start;
					done++;
                                }
				/* We need this redundant check because we are
				 * using unsigned longs for the address.
				 */
				if (pe < start || pe > end) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
/* Original C code replaced with hand tuned assembly code
 *				while(1) {
 *					if ((bad=*p) != ~pat) {
 *						error((ulong*)p, ~pat, bad);
 *					}
 *					*p = pat;
					if (p >= pe) break;
					p++;
 *					if (--k <= 0) {
 *						pat = hb;
 *						k = 32;
 *					} else {
 *						pat = pat >> 1;
 *						pat |= p3;
 *					}
 *				};
 */
				asm __volatile__ (
                                        "pushl %%ebp\n\t"
                                        "jmp L40\n\t"
                                        ".p2align 4,,7\n\t"
                                        "L49:\n\t"
                                        "subl $4,%%edi\n\t"
                                        "L40:\n\t"
                                        "movl (%%edi),%%ebp\n\t"
                                        "notl %%ecx\n\t"
                                        "cmpl %%ecx,%%ebp\n\t"
                                        "jne L44\n\t"

                                        "L45:\n\t"
                                        "notl %%ecx\n\t"
                                        "movl %%ecx,(%%edi)\n\t"
                                        "decl %%ebx\n\t"
                                        "cmpl $0,%%ebx\n\t"
                                        "jg L41\n\t"
                                        "movl %%esi,%%ecx\n\t"
                                        "movl $32,%%ebx\n\t"
                                        "jmp L42\n"
                                        "L41:\n\t"
                                        "shrl $1,%%ecx\n\t"
                                        "orl %%eax,%%ecx\n\t"
					"L42:\n\t"
                                        "cmpl %%edx,%%edi\n\t"
                                        "ja L49\n\t"
                                        "jmp L43\n\t"

                                        "L44:\n\t" \
                                        "pushl %%esi\n\t"
                                        "pushl %%eax\n\t"
                                        "pushl %%ebx\n\t"
                                        "pushl %%edx\n\t"
                                        "pushl %%ebp\n\t"
                                        "pushl %%ecx\n\t"
                                        "pushl %%edi\n\t"
                                        "call error\n\t"
                                        "popl %%edi\n\t"
                                        "popl %%ecx\n\t"
                                        "popl %%ebp\n\t"
                                        "popl %%edx\n\t"
                                        "popl %%ebx\n\t"
                                        "popl %%eax\n\t"
                                        "popl %%esi\n\t"
                                        "jmp L45\n"

                                        "L43:\n\t"
                                        "popl %%ebp\n\t"
                                        : "=b" (k), "=c" (pat)
                                        : "D" (p),"d" (pe),"b" (k),"c" (pat),
                                                "a" (p3), "S" (hb)
				);
				p = pe - 1;
			} while (!done);
		}
	}
}

/*
 * Test all of memory using modulo X access pattern.
 */
void modtst(int offset, int iter, ulong p1, ulong p2, int me)
{
	int j, k, l, done;
	ulong *p;
	ulong *pe;
	ulong *start, *end;

	/* Display the current pattern */
        if (mstr_cpu == me) {
		hprint(LINE_PAT, COL_PAT-2, p1);
		cprint(LINE_PAT, COL_PAT+6, "-");
       		dprint(LINE_PAT, COL_PAT+7, offset, 2, 1);
	}

	/* Write every nth location with pattern */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		end -= MOD_SZ;	/* adjust the ending address */
		pe = (ulong *)start;
		p = start+offset;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
/* Original C code replaced with hand tuned assembly code
 *			for (; p <= pe; p += MOD_SZ) {
 *				*p = p1;
 *			}
 */
			asm __volatile__ (
				"jmp L60\n\t" \
				".p2align 4,,7\n\t" \

				"L60:\n\t" \
				"movl %%eax,(%%edi)\n\t" \
				"addl $80,%%edi\n\t" \
				"cmpl %%edx,%%edi\n\t" \
				"jb L60\n\t" \
				: "=D" (p)
				: "D" (p), "d" (pe), "a" (p1)
			);
		} while (!done);
	}

	/* Write the rest of memory "iter" times with the pattern complement */
	for (l=0; l<iter; l++) {
		for (j=0; j<segs; j++) {
			calculate_chunk(&start, &end, me, j, 4);
			pe = (ulong *)start;
			p = start;
			done = 0;
			k = 0;
			do {
				do_tick(me);
				BAILR

				/* Check for overflow */
				if (pe + SPINSZ > pe && pe != 0) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
/* Original C code replaced with hand tuned assembly code
 *				for (; p <= pe; p++) {
 *					if (k != offset) {
 *						*p = p2;
 *					}
 *					if (++k > MOD_SZ-1) {
 *						k = 0;
 *					}
 *				}
 */
				asm __volatile__ (
					"jmp L50\n\t" \
					".p2align 4,,7\n\t" \

					"L54:\n\t" \
					"addl $4,%%edi\n\t" \
					"L50:\n\t" \
					"cmpl %%ebx,%%ecx\n\t" \
					"je L52\n\t" \
					  "movl %%eax,(%%edi)\n\t" \
					"L52:\n\t" \
					"incl %%ebx\n\t" \
					"cmpl $19,%%ebx\n\t" \
					"jle L53\n\t" \
					  "xorl %%ebx,%%ebx\n\t" \
					"L53:\n\t" \
					"cmpl %%edx,%%edi\n\t" \
					"jb L54\n\t" \
					: "=b" (k)
					: "D" (p), "d" (pe), "a" (p2),
						"b" (k), "c" (offset)
				);
				p = pe + 1;
			} while (!done);
		}
	}

	/* Now check every nth location */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 4);
		pe = (ulong *)start;
		p = start+offset;
		done = 0;
		end -= MOD_SZ;	/* adjust the ending address */
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
/* Original C code replaced with hand tuned assembly code
 *			for (; p <= pe; p += MOD_SZ) {
 *				if ((bad=*p) != p1) {
 *					error((ulong*)p, p1, bad);
 *				}
 *			}
 */
			asm __volatile__ (
				"jmp L70\n\t" \
				".p2align 4,,7\n\t" \

				"L70:\n\t" \
				"movl (%%edi),%%ecx\n\t" \
				"cmpl %%eax,%%ecx\n\t" \
				"jne L71\n\t" \
				"L72:\n\t" \
				"addl $80,%%edi\n\t" \
				"cmpl %%edx,%%edi\n\t" \
				"jb L70\n\t" \
				"jmp L73\n\t" \

				"L71:\n\t" \
				"pushl %%edx\n\t"
				"pushl %%ecx\n\t"
				"pushl %%eax\n\t"
				"pushl %%edi\n\t"
				"call error\n\t"
				"popl %%edi\n\t"
				"popl %%eax\n\t"
				"popl %%ecx\n\t"
				"popl %%edx\n\t"
				"jmp L72\n"

				"L73:\n\t" \
				: "=D" (p)
				: "D" (p), "d" (pe), "a" (p1)
				: "ecx"
			);
		} while (!done);
	}
}

/*
 * Test memory using block moves 
 * Adapted from Robert Redelmeier's burnBX test
 */
void block_move(int iter, int me)
{
	int i, j, done;
	ulong len;
	ulong *p, *pe, pp;
	ulong *start, *end;

        cprint(LINE_PAT, COL_PAT-2, "          ");

	/* Initialize memory with the initial pattern.  */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// end is always xxxxxffc, so increment so that length calculations are correct
		end = end + 1;

		pe = start;
		p = start;

		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			len  = ((ulong)pe - (ulong)p) / 64;
			//len++;
			asm __volatile__ (
				"jmp L100\n\t"

				".p2align 4,,7\n\t"
				"L100:\n\t"

				// First loop eax is 0x00000001, edx is 0xfffffffe
				"movl %%eax, %%edx\n\t"
				"notl %%edx\n\t"

				// Set a block of 64-bytes	// First loop DWORDS are 
				"movl %%eax,0(%%edi)\n\t"	// 0x00000001
				"movl %%eax,4(%%edi)\n\t"	// 0x00000001
				"movl %%eax,8(%%edi)\n\t"	// 0x00000001
				"movl %%eax,12(%%edi)\n\t"	// 0x00000001
				"movl %%edx,16(%%edi)\n\t"	// 0xfffffffe
				"movl %%edx,20(%%edi)\n\t"	// 0xfffffffe
				"movl %%eax,24(%%edi)\n\t"	// 0x00000001
				"movl %%eax,28(%%edi)\n\t"	// 0x00000001
				"movl %%eax,32(%%edi)\n\t"	// 0x00000001
				"movl %%eax,36(%%edi)\n\t"	// 0x00000001
				"movl %%edx,40(%%edi)\n\t"	// 0xfffffffe
				"movl %%edx,44(%%edi)\n\t"	// 0xfffffffe
				"movl %%eax,48(%%edi)\n\t"	// 0x00000001
				"movl %%eax,52(%%edi)\n\t"	// 0x00000001
				"movl %%edx,56(%%edi)\n\t"	// 0xfffffffe
				"movl %%edx,60(%%edi)\n\t"	// 0xfffffffe

				// rotate left with carry, 
				// second loop eax is		 0x00000002
				// second loop edx is (~eax) 0xfffffffd
				"rcll $1, %%eax\n\t"		
				
				// Move current position forward 64-bytes (to start of next block)
				"leal 64(%%edi), %%edi\n\t"

				// Loop until end
				"decl %%ecx\n\t"
				"jnz  L100\n\t"

				: "=D" (p)
				: "D" (p), "c" (len), "a" (1)
				: "edx"
			);
		} while (!done);
	}
	s_barrier();

	/* Now move the data around 
	 * First move the data up half of the segment size we are testing
	 * Then move the data to the original location + 32 bytes
	 */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// end is always xxxxxffc, so increment so that length calculations are correct
		end = end + 1;
		pe = start;
		p = start;
		done = 0;

		do {

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pp = (ulong)p + (((ulong)pe - (ulong)p) / 2); // Mid-point of this block
			len  = ((ulong)pe - (ulong)p) / 8; // Half the size of this block in DWORDS
			for(i=0; i<iter; i++) {
				do_tick(me);
				BAILR
				asm __volatile__ (
					"cld\n"
					"jmp L110\n\t"

					".p2align 4,,7\n\t"
					"L110:\n\t"

					//
					// At the end of all this 
					// - the second half equals the inital value of the first half
					// - the first half is right shifted 32-bytes (with wrapping)
					//

					// Move first half to second half
					"movl %1,%%edi\n\t" // Destionation, pp (mid point)
					"movl %0,%%esi\n\t" // Source, p (start point)
					"movl %2,%%ecx\n\t" // Length, len (size of a half in DWORDS)
					"rep\n\t"
					"movsl\n\t"

					// Move the second half, less the last 32-bytes. To the first half, offset plus 32-bytes
					"movl %0,%%edi\n\t"
					"addl $32,%%edi\n\t"	// Destination, p(start-point) plus 32 bytes
					"movl %1,%%esi\n\t"		// Source, pp(mid-point)
					"movl %2,%%ecx\n\t"
					"subl $8,%%ecx\n\t"		// Length, len(size of a half in DWORDS) minus 8 DWORDS (32 bytes)
					"rep\n\t"
					"movsl\n\t"

					// Move last 8 DWORDS (32-bytes) of the second half to the start of the first half
					"movl %0,%%edi\n\t"		// Destination, p(start-point)
											// Source, 8 DWORDS from the end of the second half, left over by the last rep/movsl
					"movl $8,%%ecx\n\t"		// Length, 8 DWORDS (32-bytes)
					"rep\n\t"
					"movsl\n\t"

					:: "g" (p), "g" (pp), "g" (len)
					: "edi", "esi", "ecx"
				);
			}
			p = pe;
		} while (!done);
	}
	s_barrier();

	/* Now check the data 
	 * The error checking is rather crude.  We just check that the
	 * adjacent words are the same.
	 */
	for (j=0; j<segs; j++) {
		calculate_chunk(&start, &end, me, j, 64);

		// end is always xxxxxffc, so increment so that length calculations are correct
		end = end + 1;
		pe = start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pe-=2;	/* the last dwords to test are pe[0] and pe[1] */
			asm __volatile__ (
				"jmp L120\n\t"

				".p2align 4,,7\n\t"
				"L124:\n\t"
				"addl $8,%%edi\n\t" // Next QWORD
				"L120:\n\t"

				// Compare adjacent DWORDS
				"movl (%%edi),%%ecx\n\t"
				"cmpl 4(%%edi),%%ecx\n\t"
				"jnz L121\n\t" // Print error if they don't match

				// Loop until end of block
				"L122:\n\t"
				"cmpl %%edx,%%edi\n\t"
				"jb L124\n"
				"jmp L123\n\t"

				"L121:\n\t"
				// eax not used so we don't need to save it as per cdecl
				// ecx is used but not restored, however we don't need it's value anymore after this point
				"pushl %%edx\n\t"
				"pushl 4(%%edi)\n\t"
				"pushl %%ecx\n\t"
				"pushl %%edi\n\t"
				"call error\n\t"
				"popl %%edi\n\t"
				"addl $8,%%esp\n\t"
				"popl %%edx\n\t"
				"jmp L122\n"
				"L123:\n\t"
				: "=D" (p)
				: "D" (p), "d" (pe)
				: "ecx"
			);
		} while (!done);
	}
}

/*
 * Test memory for bit fade, fill memory with pattern.
 */
void bit_fade_fill(ulong p1, int me)
{
	int j, done;
	ulong *p, *pe;
	ulong *start,*end;

	/* Display the current pattern */
	hprint(LINE_PAT, COL_PAT, p1);

	/* Initialize memory with the initial pattern.  */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe;) {
				*p = p1;
				p++;
			}
			p = pe + 1;
		} while (!done);
	}
}

void bit_fade_chk(ulong p1, int me)
{
	int j, done;
	ulong *p, *pe, bad;
	ulong *start,*end;

	/* Make sure that nothing changed while sleeping */
	for (j=0; j<segs; j++) {
		start = v->map[j].start;
		end = v->map[j].end;
		pe = (ulong *)start;
		p = start;
		done = 0;
		do {
			do_tick(me);
			BAILR

			/* Check for overflow */
			if (pe + SPINSZ > pe && pe != 0) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe;) {
 				if ((bad=*p) != p1) {
					error((ulong*)p, p1, bad);
				}
				p++;
			}
			p = pe + 1;
		} while (!done);
	}
}




/* Sleep for N seconds */
void sleep(long n, int flag, int me, int sms)
{
	ulong sh, sl, l, h, t, ip=0;

	/* save the starting time */
	asm __volatile__(
		"rdtsc":"=a" (sl),"=d" (sh));

	/* loop for n seconds */
	while (1) {
		asm __volatile__(
			"rep ; nop\n\t"
			"rdtsc":"=a" (l),"=d" (h));
		asm __volatile__ (
			"subl %2,%0\n\t"
			"sbbl %3,%1"
			:"=a" (l), "=d" (h)
			:"g" (sl), "g" (sh),
			"0" (l), "1" (h));

		if (sms != 0) {
			t = h * ((unsigned)0xffffffff / v->clks_msec);
			t += (l / v->clks_msec);
		} else {
			t = h * ((unsigned)0xffffffff / v->clks_msec) / 1000;
			t += (l / v->clks_msec) / 1000;
		}
		
		/* Is the time up? */
		if (t >= n) {
			break;
		}

		/* Only display elapsed time if flag is set */
		if (flag == 0) {
			continue;
		}

		if (t != ip) {
			do_tick(me);
			BAILR
			ip = t;
		}
	}
}

/* Beep function */

void beep(unsigned int frequency)
{
	
	unsigned int count = 1193180 / frequency;

	// Switch on the speaker
	outb_p(inb_p(0x61)|3, 0x61);

	// Set command for counter 2, 2 byte write
	outb_p(0xB6, 0x43);

	// Select desired Hz
	outb_p(count & 0xff, 0x42);
	outb((count >> 8) & 0xff, 0x42);

	// Block for 100 microseconds
	sleep(100, 0, 0, 1);

	// Switch off the speaker
	outb(inb_p(0x61)&0xFC, 0x61);
}
