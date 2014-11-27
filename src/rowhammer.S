/* void do_rowhammer(char *addr1, char *addr2, int count); */
/*
 * Load from addr1, addr2; clflush addr1 and addr2; mfence; repeat count times.
 * See 'Code 1' in ISCA '14 paper for code rationale.
 *
 * If addr1 and addr2 map to different rows of the same DRAM bank, it may
 * be possible to witness other rows in the bank cleared, perhaps by
 * coupling from repeated ACT/PRE signals on row activation lines.
 */
.globl do_rowhammer
do_rowhammer:
	movl	12(%esp), %ecx		/* count */
	movl	8(%esp), %edx		/* addr2 */
	movl	4(%esp), %eax		/* addr1 */
	pushl	%esi
	pushl	%edi
	/* Core loop has no memory accesses beyond ifetch, load, clflush */
1:	movl	(%eax), %esi		/* load addr1 */
	movl	(%edx), %edi		/* load addr2 */
	clflush	(%eax)			/* clflush addr1 */
	clflush (%edx)			/* clflush addr2 */
	mfence
	decl	%ecx
	jnz	1b
	popl	%edi
	popl	%esi
	ret
