/*
 * MemTest86+ V5 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * ------------------------------------------------
 * main.c - MemTest-86  Version 3.5
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 */
 
#include "stdint.h"
#include "stddef.h"
#include "test.h"
#include "defs.h"
#include "cpuid.h"
#include "smp.h"
#include "config.h"
#undef TEST_TIMES
#define DEFTESTS 9
#define FIRST_DIVISER 3

/* The main stack is allocated during boot time. The stack size should
 * preferably be a multiple of page size(4Kbytes)
*/

extern struct	cpu_ident cpu_id;
extern char	toupper(char c);
extern int	isxdigit(char c);
extern void	reboot();
extern void	bzero();
extern void	smp_set_ordinal(int me, int ord);
extern int	smp_my_ord_num(int me);
extern int	smp_ord_to_cpu(int me);
extern void	get_cpuid();
extern void	initialise_cpus();
extern ulong	rand(int cpu);
extern void	get_mem_speed(int cpu, int ncpus);
extern void	rand_seed(unsigned int seed1, unsigned int seed2, int cpu);
extern struct	barrier_s *barr;
extern int 	num_cpus;
extern int 	act_cpus;

static int	find_ticks_for_test(int test);
void		find_ticks_for_pass(void);
int		find_chunks(int test);
static void	test_setup(void);
static int	compute_segments(struct pmap map, int cpu);
int		do_test(int ord);
struct tseq tseq[] = {
	{1, -1,  0,   6, 0, "[Address test, walking ones, no cache] "},
	{1, -1,  1,   6, 0, "[Address test, own address Sequential] "},
	{1, 32,  2,   6, 0, "[Address test, own address Parallel]   "},
	{1, 32,  3,   6, 0, "[Moving inversions, 1s & 0s Parallel]  "},
	{1, 32,  5,   3, 0, "[Moving inversions, 8 bit pattern]     "},
	{1, 32,  6,  30, 0, "[Moving inversions, random pattern]    "},
	{1, 32,  7,  81, 0, "[Block move]                           "}, 
	{1,  1,  8,   3, 0, "[Moving inversions, 32 bit pattern]    "}, 
	{1, 32,  9,  48, 0, "[Random number sequence]               "},
  {1, 32, 10,   6, 0, "[Modulo 20, Random pattern]            "},
	{1, 1,  11, 240, 0, "[Bit fade test, 2 patterns]            "},
	{1, 0,   0,   0, 0, NULL}
};

volatile int    mstr_cpu;
volatile int	run_cpus;
volatile int	cpu_ord=0;
int		maxcpus=MAX_CPUS;
volatile short  cpu_sel;
volatile short	cpu_mode;
char		cpu_mask[MAX_CPUS];
long 		bin_mask=0xffffffff;
short		onepass;
volatile short	btflag = 0;
volatile int	test;
short	        restart_flag;
bool	        reloc_pending = FALSE;
uint8_t volatile stacks[MAX_CPUS][STACKSIZE];
int 		bitf_seq = 0;
char		cmdline_parsed = 0;
struct 		vars variables = {};
struct 		vars * const v = &variables;
volatile int 	bail;
int 		nticks;
int 		test_ticks;
volatile int 	segs;
static int	ltest;
static int	pass_flag = 0;
volatile short	start_seq = 0;
static int	c_iter;
ulong 		high_test_adr;
volatile static int window;
volatile static unsigned long win_next;
volatile static ulong win0_start;	/* Start test address for window 0 */
volatile static ulong win1_end;		/* End address for relocation */
volatile static struct pmap winx;  	/* Window struct for mapping windows */

/* Find the next selected test to run */
void next_test()
{
	test++;
	while (tseq[test].sel == 0 && tseq[test].cpu_sel != 0) {
	    test++;
	}

	if (tseq[test].cpu_sel == 0) {
	    /* We hit the end of the list so we completed a pass */
	    pass_flag++;
	    /* Find the next test to run, start searching from 0 */
	    test = 0;
	    while (tseq[test].sel == 0 && tseq[test].cpu_sel != 0) {
		test++;
	    }
	}
}

/* Set default values for all parameters */
void set_defaults()
{
	int i;

	if (start_seq == 2) {
		/* This is a restart so we reset everything */
		onepass = 0;
		i = 0;
		while (tseq[i].cpu_sel) {
			tseq[i].sel = 1;
			i++;
		}
		test = 0;
		if (tseq[0].sel == 0) {
			next_test();
		}
	}
	ltest = -1;
	win_next = 0;
	window = 0;
	bail = 0;
	cpu_mode = CPM_ALL;
	cpu_sel = 0;
	v->printmode=PRINTMODE_ADDRESSES;
	v->numpatn=0;
	v->plim_lower = 0;
	v->plim_upper = v->pmap[v->msegs-1].end;
	v->pass = 0;
	v->msg_line = 0;
	v->ecount = 0;
	v->ecc_ecount = 0;
	v->msg_line = LINE_SCROLL-1;
	v->scroll_start = v->msg_line * 160;
	v->erri.low_addr.page = 0x7fffffff;
	v->erri.low_addr.offset = 0xfff;
	v->erri.high_addr.page = 0;
	v->erri.high_addr.offset = 0;
	v->erri.min_bits = 32;
	v->erri.max_bits = 0;
	v->erri.min_bits = 32;
	v->erri.max_bits = 0;
	v->erri.maxl = 0;
	v->erri.cor_err = 0;
	v->erri.ebits = 0;
	v->erri.hdr_flag = 0;
	v->erri.tbits = 0;
	for (i=0; tseq[i].msg != NULL; i++) {
		tseq[i].errors = 0;
	}
	restart_flag = 0;
	tseq[10].sel = 0;
}

/* Boot trace function */
short tidx = 25;
void btrace(int me, int line, char *msg, int wait, long v1, long v2)
{
	int y, x;

	/* Is tracing turned on? */
	if (btflag == 0) return;

	spin_lock(&barr->mutex);
	y = tidx%13;
	x = tidx/13*40;
	cplace(y+11, x+1, ' ');
	if (++tidx > 25) {
		tidx = 0;
	}
	y = tidx%13;
	x = tidx/13*40;

	cplace(y+11, x+1, '>');
	dprint(y+11, x+2, me, 2, 0);
	dprint(y+11, x+5, line, 4, 0);
	cprint(y+11, x+10, msg);
	hprint(y+11, x+22, v1);
	hprint(y+11, x+31, v2);
	if (wait) {
		wait_keyup();
	}
	spin_unlock(&barr->mutex);
}

/* Relocate the test to a new address. Be careful to not overlap! */
static void run_at(unsigned long addr, int cpu)
{
	ulong *ja = (ulong *)(addr + startup_32 - _start);

	/* CPU 0, Copy memtest86+ code */
	if (cpu == 0) {
		memmove((void *)addr, &_start, _end - _start);
	}

	/* Wait for the copy */
	barrier();

	/* We use a lock to insure that only one CPU at a time jumps to
	 * the new code. Some of the startup stuff is not thread safe! */
  spin_lock(&barr->mutex);   

	/* Jump to the start address */
	goto *ja;
}

/* Switch from the boot stack to the main stack. First the main stack
 * is allocated, then the contents of the boot stack are copied, then
 * ESP is adjusted to point to the new stack.  
 */
static void
switch_to_main_stack(unsigned cpu_num)
{
	extern uintptr_t boot_stack;
	extern uintptr_t boot_stack_top; 
	uintptr_t *src, *dst;
	int offs;
	uint8_t * stackAddr, *stackTop;
   
	stackAddr = (uint8_t *) &stacks[cpu_num][0];

	stackTop  = stackAddr + STACKSIZE;
   
	src = (uintptr_t*)&boot_stack_top;
	dst = (uintptr_t*)stackTop;
	do {
		src--; dst--;
		*dst = *src;
	} while ((uintptr_t *)src > (uintptr_t *)&boot_stack);

	offs = (uint8_t *)&boot_stack_top - stackTop;
	__asm__ __volatile__ (
	"subl %%eax, %%esp" 
		: /*no output*/
		: "a" (offs) : "memory" 
	);
}

void reloc_internal(int cpu)
{
	/* clear variables */
        reloc_pending = FALSE;

	run_at(LOW_TEST_ADR, cpu);
}

void reloc(void)
{
	bail++;
        reloc_pending = TRUE;
}

/* command line passing using the 'old' boot protocol */
#define MK_PTR(seg,off) ((void*)(((unsigned long)(seg) << 4) + (off)))
#define OLD_CL_MAGIC_ADDR ((unsigned short*) MK_PTR(INITSEG,0x20))
#define OLD_CL_MAGIC 0xA33F 
#define OLD_CL_OFFSET_ADDR ((unsigned short*) MK_PTR(INITSEG,0x22))

static void parse_command_line(void)
{
	long simple_strtoul(char *cmd, char *ptr, int base);
	char *cp, dummy;
	int i, j, k;

	if (cmdline_parsed)
		return;

	/* Fill in the cpu mask array with the default */
	for (i=0; i<MAX_CPUS; i++) {
		cpu_mask[i] = 1;
	}

	if (*OLD_CL_MAGIC_ADDR != OLD_CL_MAGIC)
		return;

	unsigned short offset = *OLD_CL_OFFSET_ADDR;
	cp = MK_PTR(INITSEG, offset);

	/* skip leading spaces */
	while (*cp == ' ')
		cp++;

	while (*cp) {
		if (!strncmp(cp, "console=", 8)) {
			cp += 8;
			serial_console_setup(cp);
		}
		/* Enable boot trace? */
		if (!strncmp(cp, "btrace", 6)) {
			cp += 6;
			btflag++;
		}
		/* Limit number of CPUs */
		if (!strncmp(cp, "maxcpus=", 8)) {
			cp += 8;
			maxcpus=(int)simple_strtoul(cp, &dummy, 10);
		}
		/* Run one pass and exit if there are no errors */
		if (!strncmp(cp, "onepass", 7)) {
			cp += 7;
			onepass++;
		}
		/* Setup a list of tests to run */
		if (!strncmp(cp, "tstlist=", 8)) {
			cp += 8;
			/* Clear all of the tests first */
			k = 0;
			while (tseq[k].cpu_sel) {
				tseq[k].sel = 0;
				k++;
			}

			/* Now enable all of the tests in the list */
			j = 0;
			while(*cp && isdigit(*cp)) {
			    i = *cp-'0';
			    j = j*10 + i;
			    cp++;
			    if (*cp == ',' || !isdigit(*cp)) {
				if (j < k) {
				    tseq[j].sel = 1;
				}
				if (*cp != ',') break;
				j = 0;
			    	cp++;
			    }
			}
		}
		/* Set a CPU mask to select CPU's to use for testing */
		if (!strncmp(cp, "cpumask=", 8)) {
		    cp += 8;
		    if (cp[0] == '0' && toupper(cp[1]) == 'X') cp += 2;
		    while (*cp && *cp != ' ' && isxdigit(*cp)) {
			i = isdigit(*cp) ? *cp-'0' : toupper(*cp)-'A'+10; 
			bin_mask = bin_mask * 16 + i;
			cp++;
		    }
		    /* Force CPU zero to always be selected */
		    bin_mask |= 1;
		    for (i=0; i<32; i++) {
			if (((bin_mask>>i) & 1) == 0) {
			     cpu_mask[i] = 0; 
			}
		    }
		}
		/* go to the next parameter */
		while (*cp && *cp != ' ') cp++;
		while (*cp == ' ') cp++;
	}

	cmdline_parsed = 1;
}

void clear_screen()
{
	int i;
	char *pp;

	/* Clear screen & set background to blue */
	for(i=0, pp=(char *)(SCREEN_ADR); i<80*25; i++) {
		*pp++ = ' ';
		*pp++ = 0x17;
	}
	if (btflag) {
	    cprint(1, 0, "Boot Trace Enabled");
	    cprint(1, 0, "Press any key to advance to next trace point");
	    cprint(9, 1,"CPU Line Message     Param #1 Param #2  CPU Line Message     Param #1 Param #2");
	    cprint(10,1,"--- ---- ----------- -------- --------  --- ---- ----------- -------- --------");
	}

}
/* This is the test entry point. We get here on statup and also whenever
 * we relocate. */
void test_start(void)
{
	int my_cpu_num, my_cpu_ord, run;

	/* If this is the first time here we are CPU 0 */
	if (start_seq == 0) {
		my_cpu_num = 0;
	} else {
		my_cpu_num = smp_my_cpu_num();
	}
	/* First thing, switch to main stack */
	switch_to_main_stack(my_cpu_num);

	/* First time (for this CPU) initialization */
	if (start_seq < 2) {

	    /* These steps are only done by the boot cpu */
	    if (my_cpu_num == 0) {
		my_cpu_ord = cpu_ord++;
		smp_set_ordinal(my_cpu_num, my_cpu_ord);
		parse_command_line();
		clear_screen();
		/* Initialize the barrier so the lock in btrace will work.
		 * Will get redone later when we know how many CPUs we have */
		barrier_init(1);
		btrace(my_cpu_num, __LINE__, "Begin     ", 1, 0, 0);
		/* Find memory size */
		 mem_size();	/* must be called before initialise_cpus(); */
		/* Fill in the CPUID table */
		get_cpuid();
		/* Startup the other CPUs */
		start_seq = 1;
		//initialise_cpus();
		btrace(my_cpu_num, __LINE__, "BeforeInit", 1, 0, 0);
		/* Draw the screen and get system information */
	  init();

		/* Set defaults and initialize variables */
		set_defaults();
	
		/* Setup base address for testing, 1 MB */
		win0_start = 0x100;

		/* Set relocation address to 32Mb if there is enough
		 * memory. Otherwise set it to 3Mb */
		/* Large reloc addr allows for more testing overlap */
	        if ((ulong)v->pmap[v->msegs-1].end > 0x2f00) {
			high_test_adr = 0x2000000;
	        } else {
			high_test_adr = 0x300000;
		} 
		win1_end = (high_test_adr >> 12);

		/* Adjust the map to not test the page at 939k,
		 *  reserved for locks */
		v->pmap[0].end--;

		find_ticks_for_pass();
       	    } else {
		/* APs only, Register the APs */
		btrace(my_cpu_num, __LINE__, "AP_Start  ", 0, my_cpu_num,
			cpu_ord);
		smp_ap_booted(my_cpu_num);
		/* Asign a sequential CPU ordinal to each active cpu */
		spin_lock(&barr->mutex);
		my_cpu_ord = cpu_ord++;
		smp_set_ordinal(my_cpu_num, my_cpu_ord);
		spin_unlock(&barr->mutex);
		btrace(my_cpu_num, __LINE__, "AP_Done   ", 0, my_cpu_num,
			my_cpu_ord);
	    }

	} else {
	    /* Unlock after a relocation */
	    spin_unlock(&barr->mutex);
	    /* Get the CPU ordinal since it is lost during relocation */
	    my_cpu_ord = smp_my_ord_num(my_cpu_num);
	    btrace(my_cpu_num, __LINE__, "Reloc_Done",0,my_cpu_num,my_cpu_ord);
	}

	/* A barrier to insure that all of the CPUs are done with startup */
	barrier();
	btrace(my_cpu_num, __LINE__, "1st Barr  ", 1, my_cpu_num, my_cpu_ord);
	

	/* Setup Memory Management and measure memory speed, we do it here
	 * because we need all of the available CPUs */
	if (start_seq < 2) {

	   /* Enable floating point processing */
	   if (cpu_id.fid.bits.fpu)
        	__asm__ __volatile__ (
		    "movl %%cr0, %%eax\n\t"
		    "andl $0x7, %%eax\n\t"
		    "movl %%eax, %%cr0\n\t"
                    : :
                    : "ax"
                );
	   if (cpu_id.fid.bits.sse)
        	__asm__ __volatile__ (
                    "movl %%cr4, %%eax\n\t"
                    "orl $0x00000200, %%eax\n\t"
                    "movl %%eax, %%cr4\n\t"
                    : :
                    : "ax"
                );

	    btrace(my_cpu_num, __LINE__, "Mem Mgmnt ", 1, cpu_id.fid.bits.pae, cpu_id.fid.bits.lm);
	    /* Setup memory management modes */
	    /* If we have PAE, turn it on */
	    if (cpu_id.fid.bits.pae == 1) {
		__asm__ __volatile__(
                    "movl %%cr4, %%eax\n\t"
                    "orl $0x00000020, %%eax\n\t"
                    "movl %%eax, %%cr4\n\t"
                    : :
                    : "ax"
                );
        cprint(LINE_TITLE+1, COL_MODE, "(PAE Mode)");
       	    }
	    /* If this is a 64 CPU enable long mode */
	    if (cpu_id.fid.bits.lm == 1) {
		__asm__ __volatile__(
		    "movl $0xc0000080, %%ecx\n\t"
		    "rdmsr\n\t"
		    "orl $0x00000100, %%eax\n\t"
		    "wrmsr\n\t"
		    : :
		    : "ax", "cx"
		);
		cprint(LINE_TITLE+1, COL_MODE, "(X64 Mode)");
            }
	    /* Get the memory Speed with all CPUs */
	    get_mem_speed(my_cpu_num, num_cpus);
	}

	/* Set the initialized flag only after all of the CPU's have
	 * Reached the barrier. This insures that relocation has
	 * been completed for each CPU. */
	btrace(my_cpu_num, __LINE__, "Start Done", 1, 0, 0);
	start_seq = 2;

	/* Loop through all tests */
	while (1) {
	    /* If the restart flag is set all initial params */
	    if (restart_flag) {
		set_defaults();
		continue;
	    }
            /* Skip single CPU tests if we are using only one CPU */
            if (tseq[test].cpu_sel == -1 && 
                    (num_cpus == 1 || cpu_mode != CPM_ALL)) {
                test++; 
                continue;
            }

	    test_setup();

	    /* Loop through all possible windows */
	    while (win_next <= ((ulong)v->pmap[v->msegs-1].end + WIN_SZ)) {

		/* Main scheduling barrier */
		cprint(8, my_cpu_num+7, "W");
		btrace(my_cpu_num, __LINE__, "Sched_Barr", 1,window,win_next);
		barrier();

		/* Don't go over the 8TB PAE limit */
		if (win_next > MAX_MEM) {
			break;
		}

		/* For the bit fade test, #11, we cannot relocate so bump the
		 * window to 1 */
		if (tseq[test].pat == 11 && window == 0) {
			window = 1;
		}

		/* Relocate if required */
		if (window != 0 && (ulong)&_start != LOW_TEST_ADR) {
			btrace(my_cpu_num, __LINE__, "Sched_RelL", 1,0,0);
			run_at(LOW_TEST_ADR, my_cpu_num);
	        }
		if (window == 0 && v->plim_lower >= win0_start) {
			window++;
		}
		if (window == 0 && (ulong)&_start == LOW_TEST_ADR) {
			btrace(my_cpu_num, __LINE__, "Sched_RelH", 1,0,0);
			run_at(high_test_adr, my_cpu_num);
		}

		/* Decide which CPU(s) to use */
		btrace(my_cpu_num, __LINE__, "Sched_CPU0",1,cpu_sel,
			tseq[test].cpu_sel);
		run = 1;
		switch(cpu_mode) {
		case CPM_RROBIN:
		case CPM_SEQ:
			/* Select a single CPU */
			if (my_cpu_ord == cpu_sel) {
				mstr_cpu = cpu_sel;
				run_cpus = 1;
	    		} else {
				run = 0;
			}
			break;
		case CPM_ALL:
		    /* Use all CPUs */
		    if (tseq[test].cpu_sel == -1) {
			/* Round robin through all of the CPUs */
			if (my_cpu_ord == cpu_sel) {
				mstr_cpu = cpu_sel;
				run_cpus = 1;
	    		} else {
				run = 0;
			}
		    } else {
			/* Use the number of CPUs specified by the test,
			 * Starting with zero */
			if (my_cpu_ord >= tseq[test].cpu_sel) {
				run = 0;
			}
			/* Set the master CPU to the highest CPU number 
			 * that has been selected */
			if (act_cpus < tseq[test].cpu_sel) {
				mstr_cpu = act_cpus-1;
				run_cpus = act_cpus;
			} else {
				mstr_cpu = tseq[test].cpu_sel-1;
				run_cpus = tseq[test].cpu_sel;
			}
		    }
		}
		btrace(my_cpu_num, __LINE__, "Sched_CPU1",1,run_cpus,run);
		barrier();
		dprint(9, 7, run_cpus, 2, 0);

		/* Setup a sub barrier for only the selected CPUs */
		if (my_cpu_ord == mstr_cpu) {
			s_barrier_init(run_cpus);
		}

		/* Make sure the the sub barrier is ready before proceeding */
		barrier();

		/* Not selected CPUs go back to the scheduling barrier */
		if (run == 0 ) {
			continue;
		}
		cprint(8, my_cpu_num+7, "-");
		btrace(my_cpu_num, __LINE__, "Sched_Win0",1,window,win_next);

		/* Do we need to exit */
		if(reloc_pending) {
		    reloc_internal(my_cpu_num);
	 	}

		if (my_cpu_ord == mstr_cpu) {
		    switch (window) {
		    /* Special case for relocation */
		    case 0:
			winx.start = 0;
			winx.end = win1_end;
			window++;
			break;
		    /* Special case for first segment */
		    case 1:
			winx.start = win0_start;
			winx.end = WIN_SZ;
			win_next += WIN_SZ;
			window++;
			break;
		    /* For all other windows */
		    default:
			winx.start = win_next;
			win_next += WIN_SZ;
			winx.end = win_next;
		    }
		    btrace(my_cpu_num,__LINE__,"Sched_Win1",1,winx.start,
				winx.end);

	            /* Find the memory areas to test */
	            segs = compute_segments(winx, my_cpu_num);
		}
		s_barrier();
		btrace(my_cpu_num,__LINE__,"Sched_Win2",1,segs,
			v->map[0].pbase_addr);

	        if (segs == 0) {
		/* No memory in this window so skip it */
		    continue;
	        }

		/* map in the window... */
		if (map_page(v->map[0].pbase_addr) < 0) {
		    /* Either there is no PAE or we are at the PAE limit */
		    break;
		}

		btrace(my_cpu_num, __LINE__, "Strt_Test ",1,my_cpu_num,
			my_cpu_ord);
		do_test(my_cpu_ord);
		btrace(my_cpu_num, __LINE__, "End_Test  ",1,my_cpu_num,
			my_cpu_ord);

            	paging_off();

	    } /* End of window loop */

	    s_barrier();
	    btrace(my_cpu_num, __LINE__, "End_Win   ",1,test, window);

	    /* Setup for the next set of windows */
	    win_next = 0;
	    window = 0;
	    bail = 0;

	    /* Only the master CPU does the end of test housekeeping */
	    if (my_cpu_ord != mstr_cpu) {
		continue;
	    }

	    /* Special handling for the bit fade test #11 */
	    if (tseq[test].pat == 11 && bitf_seq != 6) {
		/* Keep going until the sequence is complete. */
		bitf_seq++;
		continue;
	    } else {
		bitf_seq = 0;
	    }

	    /* Select advancement of CPUs and next test */
	    switch(cpu_mode) {
	    case CPM_RROBIN:
		if (++cpu_sel >= act_cpus) {
		    cpu_sel = 0;
		}
		next_test();
		break;
	    case CPM_SEQ:
		if (++cpu_sel >= act_cpus) {
		    cpu_sel = 0;
		    next_test();
		}
		break;
	    case CPM_ALL:
	      if (tseq[test].cpu_sel == -1) 
	      	{
			    /* Do the same test for each CPU */
			    if (++cpu_sel >= act_cpus) 
			    	{
	            cpu_sel = 0;
			        next_test();
			    	} else {
			        continue;
			    	}
	        } else {
		    		next_test();
					}
	    } //????
	    btrace(my_cpu_num, __LINE__, "Next_CPU  ",1,cpu_sel,test);

	    /* If this was the last test then we finished a pass */
	  if (pass_flag) 
	  	{
			pass_flag = 0;
			
			v->pass++;
			
			dprint(LINE_INFO, 49, v->pass, 5, 0);
			find_ticks_for_pass();
			ltest = -1;
			
			if (v->ecount == 0) 
				{
			    /* If onepass is enabled and we did not get any errors
			     * reboot to exit the test */
			    if (onepass) {	reboot();   }
			    if (!btflag) cprint(LINE_MSG, COL_MSG-8, "** Pass complete, no errors, press Esc to exit **");
					if(BEEP_END_NO_ERROR) 
						{
							beep(1000);
							beep(2000);
							beep(1000);
							beep(2000);
						}
				}
	    }

	    bail=0;
	} /* End test loop */
}


void test_setup()
{
	static int ltest = -1;

	/* See if a specific test has been selected */
	if (v->testsel >= 0) {
                test = v->testsel;
        }

	/* Only do the setup if this is a new test */
	if (test == ltest) {
		return;
	}
	ltest = test;

	/* Now setup the test parameters based on the current test number */
	if (v->pass == 0) {
		/* Reduce iterations for first pass */
		c_iter = tseq[test].iter/FIRST_DIVISER;
	} else {
		c_iter = tseq[test].iter;
	}

	/* Set the number of iterations. We only do half of the iterations */
        /* on the first pass */
	//dprint(LINE_INFO, 28, c_iter, 3, 0);
	test_ticks = find_ticks_for_test(test);
	nticks = 0;
	v->tptr = 0;

	cprint(LINE_PAT, COL_PAT, "            ");
	cprint(LINE_PAT, COL_PAT-3, "   ");
	dprint(LINE_TST, COL_MID+6, tseq[test].pat, 2, 1);
	cprint(LINE_TST, COL_MID+9, tseq[test].msg);
	cprint(2, COL_MID+8, "                                         ");
}

/* A couple static variables for when all cpus share the same pattern */
static ulong sp1, sp2;

int do_test(int my_ord)
{
	int i=0, j=0;
	static int bitf_sleep;
	unsigned long p0=0, p1=0, p2=0;

	if (my_ord == mstr_cpu) {
	  if ((ulong)&_start > LOW_TEST_ADR) {
		/* Relocated so we need to test all selected lower memory */
		v->map[0].start = mapping(v->plim_lower);
		
		/* Good 'ol Legacy USB_WAR */
		if (v->map[0].start < (ulong*)0x500) 
		{
    	v->map[0].start = (ulong*)0x500;
		}
		
		cprint(LINE_PAT, COL_MID+25, " R");
	    } else {
		cprint(LINE_PAT, COL_MID+25, "  ");
	    }

	    /* Update display of memory segments being tested */
	    p0 = page_of(v->map[0].start);
	    p1 = page_of(v->map[segs-1].end);
	    aprint(LINE_RANGE, COL_MID+9, p0);
	    cprint(LINE_RANGE, COL_MID+14, " - ");
	    aprint(LINE_RANGE, COL_MID+17, p1);
	    aprint(LINE_RANGE, COL_MID+25, p1-p0);
	    cprint(LINE_RANGE, COL_MID+30, " of ");
	    aprint(LINE_RANGE, COL_MID+34, v->selected_pages);
	}
	
	switch(tseq[test].pat) {

	/* Do the testing according to the selected pattern */

	case 0: /* Address test, walking ones (test #0) */
		/* Run with cache turned off */
		set_cache(0);
		addr_tst1(my_ord);
		set_cache(1);
		BAILOUT;
		break;

	case 1:
	case 2: /* Address test, own address (test #1, 2) */
		addr_tst2(my_ord);
		BAILOUT;
		break;

	case 3:
	case 4:	/* Moving inversions, all ones and zeros (tests #3, 4) */
		p1 = 0;
		p2 = ~p1;
		s_barrier();
		movinv1(c_iter,p1,p2,my_ord);
		BAILOUT;
	
		/* Switch patterns */
		s_barrier();
		movinv1(c_iter,p2,p1,my_ord);
		BAILOUT;
		break;
		
	case 5: /* Moving inversions, 8 bit walking ones and zeros (test #5) */
		p0 = 0x80;
		for (i=0; i<8; i++, p0=p0>>1) {
			p1 = p0 | (p0<<8) | (p0<<16) | (p0<<24);
			p2 = ~p1;
			s_barrier();
			movinv1(c_iter,p1,p2, my_ord);
			BAILOUT;
	
			/* Switch patterns */
			s_barrier();
			movinv1(c_iter,p2,p1, my_ord);
			BAILOUT
		}
		break;
		
	case 6: /* Random Data (test #6) */
		/* Seed the random number generator */
		if (my_ord == mstr_cpu) {
		    if (cpu_id.fid.bits.rdtsc) {
                	asm __volatile__ ("rdtsc":"=a" (sp1),"=d" (sp2));
        	    } else {
                	sp1 = 521288629 + v->pass;
                	sp2 = 362436069 - v->pass;
        	    }
		    rand_seed(sp1, sp2, 0);
		}

		s_barrier();
		for (i=0; i < c_iter; i++) {
			if (my_ord == mstr_cpu) {
				sp1 = rand(0);
				sp2 = ~p1;
			}
			s_barrier();
			movinv1(2,sp1,sp2, my_ord);
			BAILOUT;
		}
		break;


	case 7: /* Block move (test #7) */
		block_move(c_iter, my_ord);
		BAILOUT;
		break;

	case 8: /* Moving inversions, 32 bit shifting pattern (test #8) */
		for (i=0, p1=1; p1; p1=p1<<1, i++) {
			s_barrier();
			movinv32(c_iter,p1, 1, 0x80000000, 0, i, my_ord);
			BAILOUT
			s_barrier();
			movinv32(c_iter,~p1, 0xfffffffe,
				0x7fffffff, 1, i, my_ord);
			BAILOUT
		}
		break;

	case 9: /* Random Data Sequence (test #9) */
		for (i=0; i < c_iter; i++) {
			s_barrier();
			movinvr(my_ord);
			BAILOUT;
		}
		break;

	case 10: /* Modulo 20 check, Random pattern (test #10) */
		for (j=0; j<c_iter; j++) {
			p1 = rand(0);
			for (i=0; i<MOD_SZ; i++) {
				p2 = ~p1;
				s_barrier();
				modtst(i, 2, p1, p2, my_ord);
				BAILOUT

				/* Switch patterns */
				s_barrier();
				modtst(i, 2, p2, p1, my_ord);
				BAILOUT
			}
		}
		break;

	case 11: /* Bit fade test, fill (test #11) */
		/* Use a sequence to process all windows for each stage */
		switch(bitf_seq) {
		case 0:	/* Fill all of memory 0's */
			bit_fade_fill(0, my_ord);
			bitf_sleep = 1;
			break;
		case 1: /* Sleep for the specified time */
			/* Only sleep once */
			if (bitf_sleep) {
				sleep(c_iter, 1, my_ord, 0);
				bitf_sleep = 0;
			}
			break;
		case 2: /* Now check all of memory for changes */
			bit_fade_chk(0, my_ord);
			break;
		case 3:	/* Fill all of memory 1's */
			bit_fade_fill(-1, my_ord);
			bitf_sleep = 1;
			break;
		case 4: /* Sleep for the specified time */
			/* Only sleep once */
			if (bitf_sleep) {
				sleep(c_iter, 1, my_ord, 0);
				bitf_sleep = 0;
			}
			break;
		case 5: /* Now check all of memory for changes */
			bit_fade_chk(-1, my_ord);
			break;
		}
		BAILOUT;
		break;

	case 90: /* Modulo 20 check, all ones and zeros (unused) */
		p1=0;
		for (i=0; i<MOD_SZ; i++) {
			p2 = ~p1;
			modtst(i, c_iter, p1, p2, my_ord);
			BAILOUT

			/* Switch patterns */
			p2 = p1;
			p1 = ~p2;
			modtst(i, c_iter, p1,p2, my_ord);
			BAILOUT
		}
		break;

	case 91: /* Modulo 20 check, 8 bit pattern (unused) */
		p0 = 0x80;
		for (j=0; j<8; j++, p0=p0>>1) {
			p1 = p0 | (p0<<8) | (p0<<16) | (p0<<24);
			for (i=0; i<MOD_SZ; i++) {
				p2 = ~p1;
				modtst(i, c_iter, p1, p2, my_ord);
				BAILOUT

				/* Switch patterns */
				p2 = p1;
				p1 = ~p2;
				modtst(i, c_iter, p1, p2, my_ord);
				BAILOUT
			}
		}
		break;
	}
	return(0);
}

/* Compute number of SPINSZ chunks being tested */
int find_chunks(int tst) 
{
	int i, j, sg, wmax, ch;
	struct pmap twin={0,0};
	unsigned long wnxt = WIN_SZ;
	unsigned long len;

	wmax = MAX_MEM/WIN_SZ+2;  /* The number of 2 GB segments +2 */
	/* Compute the number of SPINSZ memory segments */
	ch = 0;
	for(j = 0; j < wmax; j++) {
		/* special case for relocation */
		if (j == 0) {
			twin.start = 0;
			twin.end = win1_end;
		}

		/* special case for first 2 GB */
		if (j == 1) {
			twin.start = win0_start;
			twin.end = WIN_SZ;
		}

		/* For all other windows */
		if (j > 1) {
			twin.start = wnxt;
			wnxt += WIN_SZ;
			twin.end = wnxt;
		}

	        /* Find the memory areas I am going to test */
		sg = compute_segments(twin, -1);
		for(i = 0; i < sg; i++) {
			len = v->map[i].end - v->map[i].start;

			if (cpu_mode == CPM_ALL && num_cpus > 1) {
				switch(tseq[tst].pat) {
				case 2:
				case 4:
				case 5:
				case 6:
				case 9:
				case 10:
				    len /= act_cpus;
				    break;
				case 7:
				case 8:
				    len /= act_cpus;
				    break;
				}
			}
			ch += (len + SPINSZ -1)/SPINSZ;
		}
	}
	return(ch);
}

/* Compute the total number of ticks per pass */
void find_ticks_for_pass(void)
{
	int i;

	v->pptr = 0;
	v->pass_ticks = 0;
	v->total_ticks = 0;
	cprint(1, COL_MID+8, "                                         ");
	i = 0;
	while (tseq[i].cpu_sel != 0) {
		/* Skip tests 2 and 4 if we are using 1 cpu */
		if (act_cpus == 1 && (i == 2 || i == 4)) { 
		    i++;
		    continue;
		}
		v->pass_ticks += find_ticks_for_test(i);
		i++;
	}
}

static int find_ticks_for_test(int tst)
{
	int ticks=0, c, ch;

	if (tseq[tst].sel == 0) {
		return(0);
	}

	/* Determine the number of chunks for this test */
	ch = find_chunks(tst);

	/* Set the number of iterations. We only do 1/2 of the iterations */
        /* on the first pass */
	if (v->pass == 0) {
		c = tseq[tst].iter/FIRST_DIVISER;
	} else {
		c = tseq[tst].iter;
	}

	switch(tseq[tst].pat) {
	case 0: /* Address test, walking ones */
		ticks = 2;
		break;
	case 1: /* Address test, own address */
	case 2:
		ticks = 2;
		break;
	case 3: /* Moving inversions, all ones and zeros */
	case 4:
		ticks = 2 + 4 * c;
		break;
	case 5: /* Moving inversions, 8 bit walking ones and zeros */
		ticks = 24 + 24 * c;
		break;
	case 6: /* Random Data */
		ticks = c + 4 * c;
		break;
	case 7: /* Block move */
		ticks = (ch + ch/act_cpus + c*ch);
		break;
	case 8: /* Moving inversions, 32 bit shifting pattern */
		ticks = (1 + c * 2) * 64;
		break;
	case 9: /* Random Data Sequence */
		ticks = 3 * c;
		break;
	case 10: /* Modulo 20 check, Random pattern */
		ticks = 4 * 40 * c;
		break;
	case 11: /* Bit fade test */
		ticks = c * 2 + 4 * ch;
		break;
	case 90: /* Modulo 20 check, all ones and zeros (unused) */
		ticks = (2 + c) * 40;
		break;
	case 91: /* Modulo 20 check, 8 bit pattern (unused) */
		ticks = (2 + c) * 40 * 8;
		break;
	}
	if (cpu_mode == CPM_SEQ || tseq[tst].cpu_sel == -1) {
		ticks *= act_cpus;
	}
	if (tseq[tst].pat == 7 || tseq[tst].pat == 11) {
		return ticks;
	}
	return ticks*ch;
}

static int compute_segments(struct pmap win, int me)
{
	unsigned long wstart, wend;
	int i, sg;

	/* Compute the window I am testing memory in */
	wstart = win.start;
	wend = win.end;
	sg = 0;

	/* Now reduce my window to the area of memory I want to test */
	if (wstart < v->plim_lower) {
		wstart = v->plim_lower;
	}
	if (wend > v->plim_upper) {
		wend = v->plim_upper;
	}
	if (wstart >= wend) {
		return(0);
	}
	/* List the segments being tested */
	for (i=0; i< v->msegs; i++) {
		unsigned long start, end;
		start = v->pmap[i].start;
		end = v->pmap[i].end;
		if (start <= wstart) {
			start = wstart;
		}
		if (end >= wend) {
			end = wend;
		}
#if 0
		cprint(LINE_SCROLL+(2*i), 0, " (");
		hprint(LINE_SCROLL+(2*i), 2, start);
		cprint(LINE_SCROLL+(2*i), 10, ", ");
		hprint(LINE_SCROLL+(2*i), 12, end);
		cprint(LINE_SCROLL+(2*i), 20, ") ");

		cprint(LINE_SCROLL+(2*i), 22, "r(");
		hprint(LINE_SCROLL+(2*i), 24, wstart);
		cprint(LINE_SCROLL+(2*i), 32, ", ");
		hprint(LINE_SCROLL+(2*i), 34, wend);
		cprint(LINE_SCROLL+(2*i), 42, ") ");

		cprint(LINE_SCROLL+(2*i), 44, "p(");
		hprint(LINE_SCROLL+(2*i), 46, v->plim_lower);
		cprint(LINE_SCROLL+(2*i), 54, ", ");
		hprint(LINE_SCROLL+(2*i), 56, v->plim_upper);
		cprint(LINE_SCROLL+(2*i), 64, ") ");

		cprint(LINE_SCROLL+(2*i+1),  0, "w(");
		hprint(LINE_SCROLL+(2*i+1),  2, win.start);
		cprint(LINE_SCROLL+(2*i+1), 10, ", ");
		hprint(LINE_SCROLL+(2*i+1), 12, win.end);
		cprint(LINE_SCROLL+(2*i+1), 20, ") ");

		cprint(LINE_SCROLL+(2*i+1), 22, "m(");
		hprint(LINE_SCROLL+(2*i+1), 24, v->pmap[i].start);
		cprint(LINE_SCROLL+(2*i+1), 32, ", ");
		hprint(LINE_SCROLL+(2*i+1), 34, v->pmap[i].end);
		cprint(LINE_SCROLL+(2*i+1), 42, ") ");

		cprint(LINE_SCROLL+(2*i+1), 44, "i=");
		hprint(LINE_SCROLL+(2*i+1), 46, i);
		
		cprint(LINE_SCROLL+(2*i+2), 0, 
			"                                        "
			"                                        ");
		cprint(LINE_SCROLL+(2*i+3), 0, 
			"                                        "
			"                                        ");
#endif
		if ((start < end) && (start < wend) && (end > wstart)) {
			v->map[sg].pbase_addr = start;
			v->map[sg].start = mapping(start);
			v->map[sg].end = emapping(end);
#if 0
		hprint(LINE_SCROLL+(sg+1), 0, sg);
		hprint(LINE_SCROLL+(sg+1), 12, v->map[sg].pbase_addr);
		hprint(LINE_SCROLL+(sg+1), 22, start);
		hprint(LINE_SCROLL+(sg+1), 32, end);
		hprint(LINE_SCROLL+(sg+1), 42, mapping(start));
		hprint(LINE_SCROLL+(sg+1), 52, emapping(end));
		cprint(LINE_SCROLL+(sg+2), 0, 
			"                                        "
			"                                        ");
#endif
#if 0
		cprint(LINE_SCROLL+(2*i+1), 54, ", sg=");
		hprint(LINE_SCROLL+(2*i+1), 59, sg);
#endif
			sg++;
		}
	}
	return (sg);
}

