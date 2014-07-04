/******************************************************************/
/* Random number generator */
/* concatenation of following two 16-bit multiply with carry generators */
/* x(n)=a*x(n-1)+carry mod 2^16 and y(n)=b*y(n-1)+carry mod 2^16, */
/* number and carry packed within the same 32 bit integer.        */
/******************************************************************/
#include "stdint.h"
#include "cpuid.h"
#include "smp.h"

/* Keep a separate seed for each CPU */
/* Space the seeds by at least a cache line or performance suffers big time! */
static unsigned int SEED_X[MAX_CPUS*16];
static unsigned int SEED_Y[MAX_CPUS*16];

unsigned long rand (int cpu)
{
   static unsigned int a = 18000, b = 30903;
   int me;

   me = cpu*16;

   SEED_X[me] = a*(SEED_X[me]&65535) + (SEED_X[me]>>16);
   SEED_Y[me] = b*(SEED_Y[me]&65535) + (SEED_Y[me]>>16);

   return ((SEED_X[me]<<16) + (SEED_Y[me]&65535));
}


void rand_seed( unsigned int seed1, unsigned int seed2, int cpu)
{
   int me;

   me = cpu*16;
   SEED_X[me] = seed1;   
   SEED_Y[me] = seed2;
}

