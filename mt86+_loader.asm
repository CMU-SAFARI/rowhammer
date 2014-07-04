; A loader for www.memtest.org images, by Eric Auer 2003.
; This assumes that the image starts with the boot sector,
; which has the size of setup.S in sectors in a byte at offset
; 1f1h (497). Further, I assume setup.S directly after the boot
; sector and the actual memtest head.S after setup.S ...

; This version is derived from memtestL loader, which loads
; memtest.bin from a separate file. This version is meant to
; be used like (DOS / Unix variants):
; copy /b memteste.bin + memtest.bin memtest.exe
; cat memteste.bin memtest.bin > memtest.exe
; The good thing is that you get a single file which can be
; compressed, for example with http://upx.sf.net/ (UPX).

%define fullsize (150024 + buffer - exeh)
	; 150024 is the size of memtest86+ V5.01, adjust as needed!

%define stacksize 2048
%define stackpara ((stacksize + 15) / 16)

	; the trick is that NASM believes the header would be part
	; of the loaded image, so we "org 20h bytes too early" to fix:
	org 0e0h	; NASM thinks after header we have 100h
			; which is what we want it to think.

exeh:	db "MZ"
	dw fullsize % 512		; how much to load from
	dw (fullsize + 511) / 512	;      .exe to RAM
	dw 0		; no relocations used
	dw 2		; header size is 2 * 16 bytes
	dw stackpara	; minimum heap is 128 * 16 bytes, for stack
	dw stackpara	; we do not need more heap either
	dw (fullsize + 15) / 16		; SS is after file
			; segment offsets are relative to PSPseg+10h
			; initial DS and ES point to PSPseg, and file
			; except headers is loaded to PSPseg+10h.

	dw stacksize-4	; initial SP value
	dw 0		; no checksum
	dw 100h		; initial IP
	dw -10h		; initial CS relative to PSPseg+10h
	dw 0		; no relocation table, "offset 0 in file"
	dw 0		; this is not an overlay
	db "MEMT"	; padding to a multiple of 16 bytes

	; loaded part begins here (set CS so that IP is 100h here)

start:	; entry point	; if you use obj + linker, use "..start:"
  mov    ah, 01h      
  mov    bh, 00h
  mov   cx, 2000h
  int    10h

	mov ax,cs	; ***
	mov ds,ax	; ***
	mov es,ax	; ***

		; test if we have 386 or better:
	pushf   ; save flags
	xor ax,ax
	push ax
	popf    ; try to clear all bits
	pushf
	pop ax
	and ax,0f000h
	cmp ax,0f000h
	jz noinst1      ; 4 msb stuck to 1: 808x or 80186
	mov ax,0f000h
	push ax
	popf    ; try to set 4 msb
	pushf
	pop ax
	test ax,0f000h
	jz noinst1      ; 4 msb stuck to 0: 80286
	popf	; restore flags
	jmp short found386

noinst1:
	popf	; restore flags
	mov dx,need386
	jmp generror


found386:		; now test if the system is in real mode:
	smsw ax		; MSW is the low half of CR0
			; (smsw is not priv'd, unlike mov eax,cr0)
	test al,1	; if the PE (protected mode) flag on?
%ifndef DEBUG		; ignore findings in debug mode
	jnz foundprotected
%endif
	jmp foundreal

foundprotected:
	mov dx,noreal
	jmp generror

; ------------

need386	db "Sorry, you need at least a 386 CPU to use Memtest86+."
	db 13,10,"$"
noreal	db "You cannot run Memtest86+ if the system already is in"
	db " protected mode.",13,10,"$"

; ------------

generror:	; generic error exit
	push cs
	pop ds
	push cs
	pop es
	mov ah,9
	int 21h
	mov ax,4c01h
	int 21h

; ------------

foundreal:
	mov cx,buffer+15
	shr cx,4		; buffer offset in paragraphs
	mov ax,cs
	add ax,cx		; buffer offset in paragraphs
				; now AX is the buffer segment
	mov [cs:bufsetup+2],ax	; far pointer to boot sector now
	mov cx,20h		; size of boot sector in paragraphs
	add [cs:bufsetup+2],cx	; far pointer to setup now
	movzx eax,ax
	shl eax,4		; linear buffer offset
	mov [cs:buflinear],eax

findpoint:		; now patch the loader!
	mov al,[buffer+1f1h]	; size of setup.S in sectors
	; should be 4 ...
	inc al			; the boot sector itself
	movzx eax,al
	shl eax,9		; log 2 of sector size
	add [cs:buflinear],eax	; linear address of head.S now
	mov ax,[buffer+251h]	; should be jmp far dword (ofs, seg)
	cmp ax,0ea66h
	jz foundpatch
patchbug:			; could not patch the jump
	mov dx,nopatch
	jmp generror

gdtbug:
	mov dx,nogdt
	jmp generror

foundpatch:
	mov eax,[cs:buflinear]
	mov [buffer+253h],eax	; patch the protected mode entry jump
	; (offset only - segment selector unchanged: flat linear CS)

findgdt:
	mov eax,[cs:buffer+20ch]	; should be lgdt offset
	and eax,00ffffffh
	cmp eax,0016010fh	; lgdt ...
	jnz gdtbug

	mov ax,[cs:buffer+20fh]		; GDTR contents pointer
	mov bx,ax
	mov eax,[cs:buffer+200h+bx+2]	; GDT linear offset
	and eax,1ffh	; assume GDT in first sector of setup.S
	; *** WARNING: this is needed because setup.S contains
	; *** HARDCODED offset of setup.S on linear 90200h, which
	; *** is 90000h + bootsect.S ... flaw in Memtest86!

	mov cx,[cs:bufsetup+2]		; setup.S segment
	movzx ecx,cx
	shl ecx,4			; linear setup.S address
	add eax,ecx			; fixed GDT linear offset
	mov [cs:buffer+200h+bx+2],eax	; patch it

	;mov dx,trying
	;mov ah,9
	;int 21h

	;xor ax,ax
	;int 16h		; wait for a keypress from the user

	mov ax,[cs:bufsetup+2]	; setup segment
	mov ds,ax	; set nice data segments for setup.S ...
	mov es,ax
	xor ax,ax
	mov fs,ax
	mov gs,ax

	cli
	lss sp,[cs:newstack]	; stack in first 64k now!
	movzx esp,sp		; ensure 16bit stack pointer
	; Memtest86 head.S assumes that it can just turn SS to
	; linear. This would put the stack at 0:200h or so for us
	; if we fail to move the stack around ...

%ifdef DEBUG
	mov ebp,[cs:buflinear]	; will show up in debugging logs
	mov esi,[cs:bufsetup]	; will show up in debugging logs
%endif

	jmp far [cs:bufsetup]
	; setup.S will enable the A20 (ignoring HIMEM, just using
	; the classic 8042 programming trick) and turn on protected
	; mode. Then it will jump to head.S, which luckily can run
	; from any offset inside the linear 4 GB CS ...

; ------------

buflinear	dd 0	; linear address of head.S entry point
bufsetup	dw 0,0	; far pointer to setup.S entry point

newstack	dw 03fch,0	; beware, stack will overwrite IDT.

; ------------

nopatch	db "jmp far dword not found at setup.S offset 37h,",13,10
	db "(file offset 237h is not 66h, 0eah)",13,10
	db "please adjust and recompile memtestl...",13,10,"$"

nogdt	db "lgdt [...] not found at setup.S offset 0ch,",13,10
	db "(file offset 20ch is not 0fh, 01h, 16h)",13,10
	db "please adjust and recompile memtestl...",13,10,"$"

trying	db "Now trying to start Memtest86...",13,10
	db "You have to reboot to leave Memtest86 again.",13,10
	db "Press a key to go on.",13,10,"$"

; ------------

	align 16
buffer:	; a label pointing to where in the file memtest.bin will be.

