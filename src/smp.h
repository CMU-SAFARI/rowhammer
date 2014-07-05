/* **********************************************************
 * Copyright 2002 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/


#ifndef _SMP_H_
#define _SMP_H_
#include "stdint.h"
#include "defs.h"
#define MAX_CPUS 32

#define FPSignature ('_' | ('M' << 8) | ('P' << 16) | ('_' << 24))

typedef struct {
   uint32_t signature;   // "_MP_"
   uint32_t phys_addr;
   uint8_t  length;
   uint8_t  spec_rev;
   uint8_t  checksum;
   uint8_t  feature[5];
} floating_pointer_struct_t;

#define MPCSignature ('P' | ('C' << 8) | ('M' << 16) | ('P' << 24))
typedef struct {
   uint32_t signature;   // "PCMP"
   uint16_t length;
   uint8_t  spec_rev;
   uint8_t  checksum;
   char   oem[8];
   char   productid[12];
   uint32_t oem_ptr;
   uint16_t oem_size;
   uint16_t oem_count;
   uint32_t lapic_addr;
   uint32_t reserved;
} mp_config_table_header_t;

/* Followed by entries */

#define MP_PROCESSOR    0
#define MP_BUS          1
#define MP_IOAPIC       2
#define MP_INTSRC       3
#define MP_LINTSRC      4

typedef struct {
   uint8_t type;          /* MP_PROCESSOR */
   uint8_t apic_id;       /* Local APIC number */
   uint8_t apic_ver;      /* Its versions */
   uint8_t cpu_flag;
#define CPU_ENABLED             1       /* Processor is available */
#define CPU_BOOTPROCESSOR       2       /* Processor is the BP */
   uint32_t cpu_signature;           
#define CPU_STEPPING_MASK 0x0F
#define CPU_MODEL_MASK  0xF0
#define CPU_FAMILY_MASK 0xF00
   uint32_t featureflag;  /* CPUID feature value */
   uint32_t reserved[2];
} mp_processor_entry_t;

typedef struct {
   uint8_t type;   // has value MP_BUS
   uint8_t busid;
   char  bustype[6];
} mp_bus_entry_t;

/* We don't understand the others */

typedef struct {
   uint8_t  type;   // set to MP_IOAPIC
   uint8_t  apicid;
   uint8_t  apicver;
   uint8_t  flags;
#define MPC_APIC_USABLE         0x01
   uint32_t apicaddr;
} mp_io_apic_entry_t;


typedef struct {
   uint8_t  type;
   uint8_t  irqtype;
   uint16_t irqflag;
   uint8_t  srcbus;
   uint8_t  srcbusirq;
   uint8_t  dstapic;
   uint8_t  dstirq;
} mp_interrupt_entry_t;

#define MP_INT_VECTORED         0
#define MP_INT_NMI              1
#define MP_INT_SMI              2
#define MP_INT_EXTINT           3

#define MP_IRQDIR_DEFAULT       0
#define MP_IRQDIR_HIGH          1
#define MP_IRQDIR_LOW           3


typedef struct {
   uint8_t  type;
   uint8_t  irqtype;
   uint16_t irqflag;
   uint8_t  srcbusid;
   uint8_t  srcbusirq;
   uint8_t  destapic;     
#define MP_APIC_ALL     0xFF
   uint8_t  destapiclint;
} mp_local_interrupt_entry_t;

#define RSDPSignature ('R' | ('S' << 8) | ('D' << 16) | (' ' << 24))
typedef struct {
   char signature[8];   // "RSD "
   uint8_t  checksum;
   char oemid[6];
   uint8_t revision; 
   uint32_t rsdt;
   uint32_t length;
   uint32_t xrsdt[2];
   uint8_t  xsum;
} rsdp_t;

#define RSDTSignature ('R' | ('S' << 8) | ('D' << 16) | ('T' << 24))
#define XSDTSignature ('X' | ('S' << 8) | ('D' << 16) | ('T' << 24))
typedef struct {
   char signature[4];   // "RSDT"
   uint32_t length;
   uint8_t revision; 
   uint8_t  checksum;
   char oemid[18];
   char cid[4];
   char cver[4];
} rsdt_t;

#define MADTSignature ('A' | ('P' << 8) | ('I' << 16) | ('C' << 24))
typedef struct {
   uint8_t type; 
   uint8_t length;
   uint8_t acpi_id;
   uint8_t apic_id;       /* Local APIC number */
   uint32_t enabled;
} madt_processor_entry_t;

/* APIC definitions */
/*
 * APIC registers
 */
#define APICR_ID         0x02
#define APICR_ESR        0x28
#define APICR_ICRLO      0x30
#define APICR_ICRHI      0x31

/* APIC destination shorthands */
#define APIC_DEST_DEST        0
#define APIC_DEST_LOCAL       1
#define APIC_DEST_ALL_INC     2
#define APIC_DEST_ALL_EXC     3

/* APIC IPI Command Register format */
#define APIC_ICRHI_RESERVED		0x00ffffff
#define APIC_ICRHI_DEST_MASK		0xff000000
#define APIC_ICRHI_DEST_OFFSET		24

#define APIC_ICRLO_RESERVED		0xfff32000
#define APIC_ICRLO_DEST_MASK		0x000c0000
#define APIC_ICRLO_DEST_OFFSET		18
#define APIC_ICRLO_TRIGGER_MASK		0x00008000
#define APIC_ICRLO_TRIGGER_OFFSET	15
#define APIC_ICRLO_LEVEL_MASK		0x00004000
#define APIC_ICRLO_LEVEL_OFFSET		14
#define APIC_ICRLO_STATUS_MASK		0x00001000
#define APIC_ICRLO_STATUS_OFFSET	12
#define APIC_ICRLO_DESTMODE_MASK	0x00000800
#define APIC_ICRLO_DESTMODE_OFFSET	11
#define APIC_ICRLO_DELMODE_MASK		0x00000700
#define APIC_ICRLO_DELMODE_OFFSET	8
#define APIC_ICRLO_VECTOR_MASK		0x000000ff
#define APIC_ICRLO_VECTOR_OFFSET	0

/* APIC trigger types (edge/level) */
#define APIC_TRIGGER_EDGE     0
#define APIC_TRIGGER_LEVEL    1

/* APIC delivery modes */
#define APIC_DELMODE_FIXED    0
#define APIC_DELMODE_LOWEST   1
#define APIC_DELMODE_SMI      2
#define APIC_DELMODE_NMI      4
#define APIC_DELMODE_INIT     5
#define APIC_DELMODE_STARTUP  6
#define APIC_DELMODE_EXTINT   7
typedef uint32_t apic_register_t[4];

extern volatile apic_register_t *APIC;

unsigned smp_my_cpu_num();

void smp_init_bsp(void);
void smp_init_aps(void);

void smp_boot_ap(unsigned cpu_num);
void smp_ap_booted(unsigned cpu_num);

typedef struct {
        unsigned int slock;
} spinlock_t;

struct barrier_s
{
        spinlock_t mutex;
        spinlock_t lck;
        int maxproc;
        volatile int count;
        spinlock_t st1;
        spinlock_t st2;
        spinlock_t s_lck;
        int s_maxproc;
        volatile int s_count;
        spinlock_t s_st1;
        spinlock_t s_st2;
};

void barrier();
void s_barrier();
void barrier_init(int max);
void s_barrier_init(int max);

static inline void
__GET_CPUID(int ax, uint32_t *regs)
{
   __asm__ __volatile__("\t"
   	/* save ebx in case -fPIC is being used */
      "push %%ebx; cpuid; mov %%ebx, %%edi; pop %%ebx"
      : "=a" (regs[0]), "=D" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
      : "a" (ax)
      : "memory"
   );
}

#define GET_CPUID(_ax,_bx,_cx,_dx) { \
   uint32_t regs[4];                   \
   __GET_CPUID(_ax,regs);            \
   _ax = regs[0];                    \
   _bx = regs[1];                    \
   _cx = regs[2];                    \
   _dx = regs[3];                    \
}

/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * volatile because the tsc always changes without the compiler knowing it.
 */
static inline uint64_t
RDTSC(void)
{
   uint64_t tim;

   __asm__ __volatile__(
      "rdtsc"
      : "=A" (tim)
   );

   return tim;
}

static inline uint64_t __GET_MSR(int cx)
{
   uint64_t msr;

   __asm__ __volatile__(
      "rdmsr"
      : "=A" (msr)
      : "c" (cx)
   );

   return msr;
}

#define __GCC_OUT(s, s2, port, val) do { \
   __asm__(                              \
      "out" #s " %" #s2 "1, %w0"         \
      :                                  \
      : "Nd" (port), "a" (val)           \
   );                                    \
} while (0)
#define OUTB(port, val) __GCC_OUT(b, b, port, val)

static inline void spin_wait(spinlock_t *lck)
{
	if (cpu_id.fid.bits.mon) {
	    /* Use monitor/mwait for a low power, low contention spin */
            asm volatile(
		"movl $0,%%ecx\n\t"
		"movl %%ecx, %%edx\n\t"
		"1:\n\t"
		"movl %%edi,%%eax\n\t"
		"monitor\n\t"
		"cmpb $0,(%%edi)\n\t"
		"jne 2f\n\t"
		"movl %%ecx, %%eax\n\t"
		"mwait\n\t"
		"jmp 1b\n"
		"2:"
                : : "D" (lck): "%eax", "%ecx", "%edx"
	    );
	} else {
	    /* No monitor/mwait so just spin with a lot of nop's */
       	    int inc = 0x400;
            asm volatile(
		"1:\t"
		"cmpb $0,%1\n\t"
		"jne 2f\n\t"
		"rep ; nop\n\t"
		"jmp 1b\n"
		"2:"
		: : "c" (inc), "m" (lck->slock): "memory"
	    );
	}
}

static inline void spin_lock(spinlock_t *lck)
{
	if (cpu_id.fid.bits.mon) {
	    /* Use monitor/mwait for a low power, low contention spin */
            asm volatile(
		"\n1:\t"
		" ; lock;decb (%%edi)\n\t"
		"jns 3f\n"
		"movl $0,%%ecx\n\t"
		"movl %%ecx, %%edx\n\t"
		"2:\t"
		"movl %%edi,%%eax\n\t"
		"monitor\n\t"
		"movl %%ecx, %%eax\n\t"
		"mwait\n\t"
		"cmpb $0,(%%edi)\n\t"
		"jle 2b\n\t"
		"jmp 1b\n"
		"3:\n\t"
		: : "D" (lck): "%eax", "%ecx", "%edx"
	    );
	} else {
	    /* No monitor/mwait so just spin with a lot of nop's */
            int inc = 0x400;
            asm volatile(
		"\n1:\t"
		" ; lock;decb %0\n\t"
		"jns 3f\n"
		"2:\t"
		"rep;nop\n\t"
		"cmpb $0,%0\n\t"
		"jle 2b\n\t"
		"jmp 1b\n"
		"3:\n\t"
		: "+m" (lck->slock) : "c" (inc) : "memory"
	    );
	}
}

static inline void spin_unlock(spinlock_t *lock)
{
        asm volatile("movb $1,%0" : "+m" (lock->slock) :: "memory");
}


#endif /* _SMP_H_ */
