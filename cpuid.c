/*
 * cpuid.c --
 *
 *      Implements CPUID querying functions
 *
 */
#include "stdin.h"
#include "cpuid.h"

struct cpu_ident cpu_id;

void get_cpuid()
{
	unsigned int *v, dummy[3];
	char *p, *q;

	/* Get max std cpuid & vendor ID */
	cpuid(0x0, &cpu_id.max_cpuid, &cpu_id.vend_id.uint32_array[0],
	    &cpu_id.vend_id.uint32_array[2], &cpu_id.vend_id.uint32_array[1]);
	cpu_id.vend_id.char_array[11] = 0;

	/* Get processor family information & feature flags */
	if (cpu_id.max_cpuid >= 1) {
	    cpuid(0x00000001, &cpu_id.vers.flat, &cpu_id.info.flat,
		&cpu_id.fid.uint32_array[1], &cpu_id.fid.uint32_array[0]);
	}

	/* Get the digital thermal sensor & power management status bits */
	if(cpu_id.max_cpuid >= 6)	{
		cpuid(0x00000006, &cpu_id.dts_pmp, &dummy[0], &dummy[1], &dummy[2]);
	}
	
	/* Get the max extended cpuid */
	cpuid(0x80000000, &cpu_id.max_xcpuid, &dummy[0], &dummy[1], &dummy[2]);

	/* Get extended feature flags, only save EDX */
	if (cpu_id.max_xcpuid >= 0x80000001) {
	    cpuid(0x80000001, &dummy[0], &dummy[1],
		&dummy[2], &cpu_id.fid.uint32_array[2]);
	}

	/* Get the brand ID */
	if (cpu_id.max_xcpuid >= 0x80000004) {
	    v = (unsigned int *)&cpu_id.brand_id;
	    cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	    cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	    cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	    cpu_id.brand_id.char_array[47] = 0;
	}
        /*
         * Intel chips right-justify this string for some dumb reason;
         * undo that brain damage:
         */
        p = q = &cpu_id.brand_id.char_array[0];
        while (*p == ' ')
                p++;
        if (p != q) {
                while (*p)
                        *q++ = *p++;
                while (q <= &cpu_id.brand_id.char_array[48])
                        *q++ = '\0';    /* Zero-pad the rest */
	}

	/* Get cache information */
	switch(cpu_id.vend_id.char_array[0]) {
        case 'A':
            /* AMD Processors */
	    /* The cache information is only in ecx and edx so only save
	     * those registers */
	    if (cpu_id.max_xcpuid >= 0x80000005) {
		cpuid(0x80000005, &dummy[0], &dummy[1],
		    &cpu_id.cache_info.uint[0], &cpu_id.cache_info.uint[1]);
	    }
	    if (cpu_id.max_xcpuid >= 0x80000006) {
		cpuid(0x80000006, &dummy[0], &dummy[1],
		    &cpu_id.cache_info.uint[2], &cpu_id.cache_info.uint[3]);
	    }
	    break;
	case 'G':
                /* Intel Processors, Need to do this in init.c */
	    break;
	}

	/* Turn off mon bit since monitor based spin wait may not be reliable */
	cpu_id.fid.bits.mon = 0;

}
