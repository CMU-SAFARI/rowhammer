/* defs.h - MemTest-86 Version 3.3
 * assembler/compiler definitions
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 */ 

#define SETUPSECS	4		/* Number of setup sectors */

/*
 * Caution!! There is magic in the build process.  Read
 * README.build-process before you change anything.  
 * Unlike earlier versions all of the settings are in defs.h
 * so the build process should be more robust.
 */
#define LOW_TEST_ADR	0x00010000		/* Final adrs for test code */

#define BOOTSEG		0x07c0			/* Segment adrs for inital boot */
#define INITSEG		0x9000			/* Segment adrs for relocated boot */
#define SETUPSEG	(INITSEG+0x20)		/* Segment adrs for relocated setup */
#define TSTLOAD		0x1000			/* Segment adrs for load of test */

#define KERNEL_CS	0x10			/* 32 bit segment adrs for code */
#define KERNEL_DS	0x18			/* 32 bit segment adrs for data */
#define REAL_CS		0x20			/* 16 bit segment adrs for code */
#define REAL_DS		0x28			/* 16 bit segment adrs for data */
