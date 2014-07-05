/*
 *	 bootsect.s		Copyright (C) 1991, 1992 Linus Torvalds
 *
 * bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
 * itself out of the way to address 0x90000, and jumps there.
 *
 * It then loads 'setup' directly after itself (0x90200), and the system
 * at 0x10000, using BIOS interrupts.
 *
 * The loader has been made as simple as possible, and continuos
 * read errors will result in a unbreakable loop. Reboot by hand. It
 * loads pretty fast by getting whole tracks at a time whenever possible.
 *
 * 1-Jan-96 Modified by Chris Brady for use as a boot loader for MemTest-86.
 */

#include "defs.h"

ROOT_DEV = 0

.code16
.section ".bootsect", "ax", @progbits
_boot:


# ld86 requires an entry symbol. This may as well be the usual one.
.globl	_main
_main:
	movw	$BOOTSEG, %ax
	movw	%ax, %ds
	movw	$INITSEG, %ax
	movw	%ax, %es
	movw	$256, %cx
	subw	%si, %si
	subw	%di, %di
	cld
	rep
	movsw
	ljmp	$INITSEG, $go - _boot

go:
	movw	%cs, %ax
	movw	$(0x4000-12), %dx	# 0x4000 is arbitrary value >= length of
					# bootsect + length of setup + room for stack
					# 12 is disk parm size

# bde - changed 0xff00 to 0x4000 to use debugger at 0x6400 up (bde).  We
# wouldn't have to worry about this if we checked the top of memory.  Also
# my BIOS can be configured to put the wini drive tables in high memory
# instead of in the vector table.  The old stack might have clobbered the
# drive table.

	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss		# put stack at INITSEG:0x4000-12.
	movw	%dx, %sp

/*
 *	Many BIOS's default disk parameter tables will not
 *	recognize multi-sector reads beyond the maximum sector number
 *	specified in the default diskette parameter tables - this may
 *	mean 7 sectors in some cases.
 *
 *	Since single sector reads are slow and out of the question,
 *	we must take care of this by creating new parameter tables
 *	(for the first disk) in RAM.  We will set the maximum sector
 *	count to 18 - the most we will encounter on an HD 1.44.
 *
 *	High doesn't hurt.  Low does.
 *
 *	Segments are as follows: ds=es=ss=cs - INITSEG,
 *		fs = 0, gs = parameter table segment
 */
	pushw	$0
	popw	%fs
	movw	$0x78, %bx		# fs:bx is parameter table address
	lgs	%fs:(%bx),%si		# gs:si is source

	movw	%dx, %di		# es:di is destination
	movw	$6, %cx			# copy 12 bytes
	cld

	rep	movsw %gs:(%si), (%di)

	movw	%dx, %di
	movb	$18, 4(%di)		# patch sector count

	movw	%di, %fs:(%bx)
	movw	%es, %fs:2(%bx)

	movw	%cs, %ax
	movw	%ax, %fs
	movw	%ax, %gs

	xorb	%ah, %ah		# reset FDC
	xorb	%dl, %dl
	int	$0x13

# load the setup-sectors directly after the bootblock.
# Note that 'es' is already set up.

load_setup:
	xorw	%dx, %dx			# drive 0, head 0
	movw	$0x0002, %cx			# sector 2, track 0
	movw	$0x0200, %bx			# address = 512, in INITSEG
	movw	$(0x0200 + SETUPSECS), %ax	# service 2, nr of sectors
						# (assume all on head 0, track 0)
	int	$0x13				# read it
	jnc	ok_load_setup			# ok - continue

	pushw	%ax			# dump error code
	call	print_nl
	movw	%sp, %bp
	call	print_hex
	popw	%ax

	xorb	%dl, %dl		# reset FDC
	xorb	%ah, %ah
	int	$0x13
	jmp	load_setup

ok_load_setup:

# Get disk drive parameters, specifically nr of sectors/track


/* It seems that there is no BIOS call to get the number of sectors.  Guess
 * 18 sectors if sector 18 can be read, 15 if sector 15 can be read.
 * Otherwise guess 9
 */

	xorw	%dx, %dx			# drive 0, head 0
	movw	$0x0012, %cx			# sector 18, track 0
	movw	$(0x200+(SETUPSECS*0x200)), %bx	# address after setup (es = cs)
	movw	$0x0201, %ax			# service 2, 1 sector
	int	$0x13
	jnc	got_sectors
	movb	$0x0f, %cl			# sector 15
	movw	$0x0201, %ax			# service 2, 1 sector
	int	$0x13
	jnc	got_sectors
	movb	$0x09, %cl

got_sectors:
	movw	%cx, %cs:sectors - _boot
	movw	$INITSEG, %ax
	movw	%ax, %es

# Print some inane message

	movb	$0x03, %ah		# read cursor pos
	xorb	%bh, %bh
	int	$0x10

	movw	$9, %cx
	movw	$0x0007, %bx		# page 0, attribute 7 (normal)
	movw	$msg1 - _boot, %bp
	movw	$0x1301, %ax		# write string, move cursor
	int	$0x10

# ok, we've written the message, now
# we want to load the system (at 0x10000)

	movw	$TSTLOAD, %ax
	movw	%ax, %es		# segment of 0x010000
	call	read_it
	call	kill_motor
	call  turnoffcursor
	call	print_nl

# after that (everyting loaded), we jump to
# the setup-routine loaded directly after
# the bootblock:

	ljmp	$SETUPSEG,$0

# This routine loads the system at address 0x10000, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.
#
# in:	es - starting address segment (normally 0x1000)
#
sread:	.word 1+SETUPSECS	# sectors read of current track
head:	.word 0			# current head
track:	.word 0			# current track

read_it:
	movw	%es, %ax
	testw	$0x0fff, %ax
die:
	jne	die		# es must be at 64kB boundary
	xorw	%bx,%bx		# bx is starting address within segment
rp_read:
	movw	%es, %ax
	subw	$TSTLOAD, %ax	# have we loaded all yet?
	cmpw	syssize - _boot, %ax
	jbe	ok1_read
	ret
ok1_read:
	movw	%cs:sectors - _boot, %ax
	subw	sread - _boot, %ax
	movw	%ax, %cx
	shlw	$9, %cx
	addw	%bx, %cx
	jnc	ok2_read
	je	ok2_read
	xorw	%ax, %ax
	subw	%bx, %ax
	shrw	$9, %ax
ok2_read:
	call	read_track
	movw	%ax, %cx
	add	sread - _boot, %ax
	cmpw	%cs:sectors - _boot, %ax
	jne	ok3_read
	movw	$1, %ax
	subw	head - _boot, %ax
	jne	ok4_read
	incw	track - _boot
ok4_read:
	movw	%ax, head - _boot
	xorw	%ax, %ax
ok3_read:
	movw	%ax, sread - _boot
	shlw	$9, %cx
	addw	%cx, %bx
	jnc	rp_read
	movw	%es, %ax
	addb	$0x10, %ah
	movw	%ax, %es
	xorw	%bx, %bx
	jmp	rp_read

read_track:
	pusha
	pusha
	movw	$0xe2e, %ax 	# loading... message 2e = .
	movw	$7, %bx
	int	$0x10
	popa

	movw	track - _boot, %dx
	movw	sread - _boot, %cx
	incw	%cx
	movb	%dl, %ch
	movw	head - _boot, %dx
	movb	%dl, %dh
	andw	$0x0100, %dx
	movb	$2, %ah

	pushw	%dx				# save for error dump
	pushw	%cx
	pushw	%bx
	pushw	%ax

	int	$0x13
	jc	bad_rt
	addw	$8, %sp
	popa
	ret

bad_rt:
	pushw	%ax				# save error code
	call	print_all			# ah = error, al = read

	xorb	%ah, %ah
	xorb	%dl, %dl
	int	$0x13

	addw	$10, %sp
	popa
	jmp read_track

/*
 *	print_all is for debugging purposes.
 *	It will print out all of the registers.  The assumption is that this is
 *	called from a routine, with a stack frame like
 *	dx
 *	cx
 *	bx
 *	ax
 *	error
 *	ret <- sp
 *
*/

print_all:
	movw	$5, %cx		# error code + 4 registers
	movw	%sp, %bp

print_loop:
	pushw	%cx		# save count left
	call	print_nl	# nl for readability

	cmpb	5, %cl		# see if register name is needed
	jae	no_reg

	movw	$(0xe05	+ 'A' - 1), %ax
	subb	%cl, %al
	int	$0x10
	movb	$'X', %al
	int	$0x10
	movb	$':', %al
	int	$0x10

no_reg:
	addw	$2, %bp		# next register
	call	print_hex	# print it
	popw	%cx
	loop	print_loop
	ret

print_nl:
	movw	$0xe0d, %ax	# CR
	int	$0x10
	movb	$0x0a, %al	# LF
	int	$0x10
	ret

/*
 *	print_hex is for debugging purposes, and prints the word
 *	pointed to by ss:bp in hexadecmial.
 */

print_hex:
	movw	$4, %cx		# 4 hex digits
	movw	(%bp), %dx	# load word into dx

print_digit:
	rolw	$4, %dx		# rotate so that lowest 4 bits are used
	movb	$0xe, %ah
	movb	%dl, %al	# mask off so we have only next nibble
	andb	$0xf, %al
	addb	$'0', %al	# convert to 0-based digit
	cmpb	$'9', %al	# check for overflow
	jbe	good_digit
	addb	$('A' - '0' - 10), %al

good_digit:
	int	$0x10
	loop	print_digit
	ret


/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * don't have to worry about it later.
 */
kill_motor:
	pushw	%dx
	movw	$0x3f2, %dx
	xorb	%al, %al
	outb	%al, %dx
	popw	%dx
	ret

turnoffcursor:
  movb  $0x01, %ah      # turn off the cursor
  movb  $0x00, %bh
  movw  $0x2000, %cx
  int   $0x10 
	ret
	
sectors:
	.word 0

msg1:
	.byte 13,10
	.ascii "Loading"

.org 497
setup_sects:
	.byte SETUPSECS
.org 500
syssize:
	.word _syssize
.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55
_eboot:
