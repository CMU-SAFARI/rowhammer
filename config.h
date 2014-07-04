/*
 * MemTest86+ V5 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 * ------------------------------------------------
 * config.h - MemTest-86  Version 3.3
 *
 * Compile time configuration options
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 */

/* CONSERVATIVE_SMP - If set to 0, SMP will be enabled by default */
/* Might be enabled in future revision after extensive testing */
/* In all cases, SMP is disabled by defaut on server platform */
#define CONSERVATIVE_SMP 1

/* BEEP_MODE - Beep on error. Default off, Change to 1 to enable */
#define BEEP_MODE 0

/* BEEP_END_NO_ERROR - Beep at end of each pass without error. Default off, Change to 1 to enable */
#define BEEP_END_NO_ERROR 0

/* PARITY_MEM - Enables support for reporting memory parity errors */
/*	Experimental, normally enabled */
#define PARITY_MEM

/* SERIAL_CONSOLE_DEFAULT -  The default state of the serial console. */
/*	This is normally off since it slows down testing.  Change to a 1 */
/*	to enable. */
#define SERIAL_CONSOLE_DEFAULT 0

/* SERIAL_TTY - The default serial port to use. 0=ttyS0, 1=ttyS1 */ 
#define SERIAL_TTY 0

/* SERIAL_BAUD_RATE - Baud rate for the serial console */
#define SERIAL_BAUD_RATE 9600

/* SCRN_DEBUG - extra check for SCREEN_BUFFER
 */ 
/* #define SCRN_DEBUG */

/* APM - Turns off APM at boot time to avoid blanking the screen */
/*	Normally enabled */
#define APM_OFF

/* USB_WAR - Enables a workaround for errors caused by BIOS USB keyboard */
/*	and mouse support*/
/*	Normally enabled */
#define USB_WAR

