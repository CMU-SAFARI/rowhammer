/* 
 * MemTest86+ V5 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 */

//#include "defs.h"
#include "config.h"
//#include "test.h"
#include "pci.h"
#include "controller.h"
#include "spd.h"
#include "test.h"
#include "stdint.h"
#include "cpuid.h"
#include "msr.h"
#include "dmi.h"

int col, col2;
int nhm_bus = 0x3F;
	
extern ulong extclock;
extern unsigned long imc_type;
extern struct cpu_ident cpu_id;
/*
#define rdmsr(msr,val1,val2) \
	__asm__ __volatile__("rdmsr" \
			  : "=a" (val1), "=d" (val2) \
			  : "c" (msr) : "edi")

#define wrmsr(msr,val1,val2) \
	__asm__ __volatile__("wrmsr" \
			  :  \
			  : "c" (msr), "a" (val1), "d" (val2) : "edi")
*/
/* controller ECC capabilities and mode */
#define __ECC_UNEXPECTED 1      /* Unknown ECC capability present */
#define __ECC_DETECT     2	/* Can detect ECC errors */
#define __ECC_CORRECT    4	/* Can correct some ECC errors */
#define __ECC_SCRUB      8	/* Can scrub corrected ECC errors */
#define __ECC_CHIPKILL  16	/* Can corrected multi-errors */

#define ECC_UNKNOWN      (~0UL)    /* Unknown error correcting ability/status */
#define ECC_NONE         0       /* Doesnt support ECC (or is BIOS disabled) */
#define ECC_RESERVED     __ECC_UNEXPECTED  /* Reserved ECC type */
#define ECC_DETECT       __ECC_DETECT
#define ECC_CORRECT      (__ECC_DETECT | __ECC_CORRECT)
#define ECC_CHIPKILL	 (__ECC_DETECT | __ECC_CORRECT | __ECC_CHIPKILL)
#define ECC_SCRUB        (__ECC_DETECT | __ECC_CORRECT | __ECC_SCRUB)


static struct ecc_info {
	int index;
	int poll;
	unsigned bus;
	unsigned dev;
	unsigned fn;
	unsigned cap;
	unsigned mode;
} ctrl =
{
	.index = 0,
	/* I know of no case where the memory controller is not on the
	 * host bridge, and the host bridge is not on bus 0  device 0
	 * fn 0.  But just in case leave these as variables.
	 */
	.bus = 0,
	.dev = 0,
	.fn = 0,
	/* Properties of the current memory controller */
	.cap = ECC_UNKNOWN,
	.mode = ECC_UNKNOWN,
};


void coretemp(void) 
{
	unsigned int msrl, msrh;
	unsigned int tjunc, tabs, tnow;
	unsigned long rtcr;
	double amd_raw_temp;
	
	// Only enable coretemp if IMC is known
	if(imc_type == 0) { return; }
	
	tnow = 0;
	
	// Intel  CPU
	if(cpu_id.vend_id.char_array[0] == 'G' && cpu_id.max_cpuid >= 6)
	{
		if(cpu_id.dts_pmp & 1){
			rdmsr(MSR_IA32_THERM_STATUS, msrl, msrh);
			tabs = ((msrl >> 16) & 0x7F);
			rdmsr(MSR_IA32_TEMPERATURE_TARGET, msrl, msrh);
			tjunc = ((msrl >> 16) & 0x7F);
			if(tjunc < 50 || tjunc > 125) { tjunc = 90; } // assume Tjunc = 90°C if boggus value received.
			tnow = tjunc - tabs;		
			dprint(LINE_CPU+1, 30, v->check_temp, 3, 0);	
			v->check_temp = tnow;
		}
		return;
	}
	
	// AMD CPU
	if(cpu_id.vend_id.char_array[0] == 'A' && cpu_id.vers.bits.extendedFamily > 0)
	{
		pci_conf_read(0, 24, 3, 0xA4, 4, &rtcr);
		amd_raw_temp = ((rtcr >> 21) & 0x7FF);
		v->check_temp = (int)(amd_raw_temp / 8);
		dprint(LINE_CPU+1, 30, v->check_temp, 3, 0);	
	}	
	
				
}

void print_cpu_line(float dram_freq, float fsb_freq, int ram_type)
{
	int cur_col = COL_SPEC;
	
	cprint(LINE_CPU, cur_col, "RAM:                                ");
	cur_col += 5;
	dprint(LINE_CPU, cur_col, dram_freq, 4, 1);
	cur_col += 4;
	cprint(LINE_CPU, cur_col, "MHz (");
	cur_col += 5;
	
	switch(ram_type)
	{
		default:
		case 1:
			cprint(LINE_CPU, cur_col, "DDR-");
			cur_col += 4;
			break;		
		case 2:
			cprint(LINE_CPU, cur_col, "DDR2-");
			cur_col += 5;
			break;	
		case 3:
			cprint(LINE_CPU, cur_col, "DDR3-");
			cur_col += 5;
			break;	
	}		

	if(dram_freq < 500)
	{
		dprint(LINE_CPU, cur_col, dram_freq*2, 3, 0);
		cur_col += 3;
	} else {
		dprint(LINE_CPU, cur_col, dram_freq*2, 4, 0);
		cur_col += 4;		
	}
	cprint(LINE_CPU, cur_col, ")");
	cur_col++;	
	
	if(fsb_freq > 10)
	{
		cprint(LINE_CPU, cur_col, " - BCLK: ");
		cur_col += 9;
	
		dprint(LINE_CPU, cur_col, fsb_freq, 3, 0);	
	}
	
}

void print_ram_line(float cas, int rcd, int rp, int ras, int chan)
{
	int cur_col = COL_SPEC;

	cprint(LINE_RAM, cur_col, "Timings: CAS                        ");
	cur_col += 13;	

	// CAS Latency (tCAS)
	if (cas == 1.5) {
		cprint(LINE_RAM, cur_col, "1.5"); cur_col += 3;
	} else if (cas == 2.5) {
		cprint(LINE_RAM, cur_col, "2.5"); cur_col += 3;
	} else if (cas < 10) {
		dprint(LINE_RAM, cur_col, cas, 1, 0); cur_col += 1;
	} else {
		dprint(LINE_RAM, cur_col, cas, 2, 0); cur_col += 2;		
	}
	cprint(LINE_RAM, cur_col, "-"); cur_col += 1;

	// RAS-To-CAS (tRCD)
	if (rcd < 10) {
		dprint(LINE_RAM, cur_col, rcd, 1, 0);
		cur_col += 1;
	} else {
		dprint(LINE_RAM, cur_col, rcd, 2, 0);
		cur_col += 2;		
	}
	cprint(LINE_RAM, cur_col, "-"); cur_col += 1;

	// RAS Precharge (tRP)
	if (rp < 10) {
		dprint(LINE_RAM, cur_col, rp, 1, 0);
		cur_col += 1;
	} else {
		dprint(LINE_RAM, cur_col, rp, 2, 0);
		cur_col += 2;		
	}
	cprint(LINE_RAM, cur_col, "-"); cur_col += 1;

	// RAS Active to precharge (tRAS)
	if (ras < 10) {
		dprint(LINE_RAM, cur_col, ras, 1, 0);
		cur_col += 1;
	} else {
		dprint(LINE_RAM, cur_col, ras, 2, 0);
		cur_col += 2;
	}
	
		
	switch(chan)
	{
		case 0:
			break;
		case 1:
			cprint(LINE_RAM, cur_col, " @ 64-bit Mode");
			break;			
		case 2: 
			cprint(LINE_RAM, cur_col, " @ 128-bit Mode");
			break;			
		case 3:
			cprint(LINE_RAM, cur_col, " @ 192-bit Mode");
			break;	
		case 4:
			cprint(LINE_RAM, cur_col, " @ 256-bit Mode");
			break;	
	}
}

static void poll_fsb_nothing(void)
{

	char *name;
	
	/* Print the controller name */
	name = controllers[ctrl.index].name;
	cprint(LINE_CPU, COL_SPEC, "Chipset:               ");	
	cprint(LINE_CPU, COL_SPEC+9, name);
	return;
}

static void poll_timings_nothing(void)
{
	char *ram_type;
	
	/* Print the controller name */
	ram_type = controllers[ctrl.index].ram_type;
	cprint(LINE_RAM, COL_SPEC, "RAM Type:              ");
	cprint(LINE_RAM, COL_SPEC+10, ram_type);
	return;
}

static void setup_nothing(void)
{
	ctrl.cap = ECC_NONE;
	ctrl.mode = ECC_NONE;
}

static void poll_nothing(void)
{
/* Code to run when we don't know how, or can't ask the memory
 * controller about memory errors.
 */
	return;
}

static void setup_wmr(void)
{
	ulong dev0;
		
	// Activate MMR I/O
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	if (!(dev0 & 0x1)) {
		pci_conf_write( 0, 0, 0, 0x48, 1, dev0 | 1);
	}

}


static void setup_nhm(void)
{
	static float possible_nhm_bus[] = {0xFF, 0x7F, 0x3F};
	unsigned long did, vid, mc_control, mc_ssrcontrol;
	int i;
	
	//Nehalem supports Scrubbing */
	ctrl.cap = ECC_SCRUB;
	ctrl.mode = ECC_NONE;

	/* First, locate the PCI bus where the MCH is located */

	for(i = 0; i < sizeof(possible_nhm_bus); i++) {
		pci_conf_read( possible_nhm_bus[i], 3, 4, 0x00, 2, &vid);
		pci_conf_read( possible_nhm_bus[i], 3, 4, 0x02, 2, &did);
		vid &= 0xFFFF;
		did &= 0xFF00;
		if(vid == 0x8086 && did >= 0x2C00) { 
			nhm_bus = possible_nhm_bus[i]; 
			}
}

	/* Now, we have the last IMC bus number in nhm_bus */
	/* Check for ECC & Scrub */
	
	pci_conf_read(nhm_bus, 3, 0, 0x4C, 2, &mc_control);	
	if((mc_control >> 4) & 1) { 
		ctrl.mode = ECC_CORRECT; 
		pci_conf_read(nhm_bus, 3, 2, 0x48, 2, &mc_ssrcontrol);	
		if(mc_ssrcontrol & 3) { 
			ctrl.mode = ECC_SCRUB; 
		}		
	}
	
}

static void setup_nhm32(void)
{
	static float possible_nhm_bus[] = {0xFF, 0x7F, 0x3F};
	unsigned long did, vid, mc_control, mc_ssrcontrol;
	int i;
	
	//Nehalem supports Scrubbing */
	ctrl.cap = ECC_SCRUB;
	ctrl.mode = ECC_NONE;

	/* First, locate the PCI bus where the MCH is located */
	for(i = 0; i < sizeof(possible_nhm_bus); i++) {
		pci_conf_read( possible_nhm_bus[i], 3, 4, 0x00, 2, &vid);
		pci_conf_read( possible_nhm_bus[i], 3, 4, 0x02, 2, &did);
		vid &= 0xFFFF;
		did &= 0xFF00;
		if(vid == 0x8086 && did >= 0x2C00) { 
			nhm_bus = possible_nhm_bus[i]; 
			}
	}

	/* Now, we have the last IMC bus number in nhm_bus */
	/* Check for ECC & Scrub */
	pci_conf_read(nhm_bus, 3, 0, 0x48, 2, &mc_control);	
	if((mc_control >> 1) & 1) { 
		ctrl.mode = ECC_CORRECT; 
		pci_conf_read(nhm_bus, 3, 2, 0x48, 2, &mc_ssrcontrol);	
		if(mc_ssrcontrol & 1) { 
			ctrl.mode = ECC_SCRUB; 
		}		
	}
	
}

static void setup_amd64(void)
{
	static const int ddim[] = { ECC_NONE, ECC_CORRECT, ECC_RESERVED, ECC_CHIPKILL };
	unsigned long nbxcfg;
	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long mcanb;
	unsigned long dramcl;

	/* All AMD64 support Chipkill */
	ctrl.cap = ECC_CHIPKILL;

	/* Check First if ECC DRAM Modules are used */
	pci_conf_read(0, 24, 2, 0x90, 4, &dramcl);
	
	if (cpu_id.vers.bits.extendedModel >= 4) {
		/* NEW K8 0Fh Family 90 nm */
		
		if ((dramcl >> 19)&1){
			/* Fill in the correct memory capabilites */
			pci_conf_read(0, 24, 3, 0x44, 4, &nbxcfg);
			ctrl.mode = ddim[(nbxcfg >> 22)&3];
		} else {
			ctrl.mode = ECC_NONE;
		}
		/* Enable NB ECC Logging by MSR Write */
		rdmsr(0x017B, mcgsrl, mcgsth);
		wrmsr(0x017B, 0x10, mcgsth);
	
		/* Clear any previous error */
		pci_conf_read(0, 24, 3, 0x4C, 4, &mcanb);
		pci_conf_write(0, 24, 3, 0x4C, 4, mcanb & 0x7FFFFFFF );		

	} else { 
		/* OLD K8 130 nm */
		
		if ((dramcl >> 17)&1){
			/* Fill in the correct memory capabilites */
			pci_conf_read(0, 24, 3, 0x44, 4, &nbxcfg);
			ctrl.mode = ddim[(nbxcfg >> 22)&3];
		} else {
			ctrl.mode = ECC_NONE;
		}
		/* Enable NB ECC Logging by MSR Write */
		rdmsr(0x017B, mcgsrl, mcgsth);
		wrmsr(0x017B, 0x10, mcgsth);
	
		/* Clear any previous error */
		pci_conf_read(0, 24, 3, 0x4C, 4, &mcanb);
		pci_conf_write(0, 24, 3, 0x4C, 4, mcanb & 0x7F801EFC );
	}
}

static void setup_k10(void)
{
	static const int ddim[] = { ECC_NONE, ECC_CORRECT, ECC_CHIPKILL, ECC_CHIPKILL };
	unsigned long nbxcfg;
	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long mcanb;
	unsigned long dramcl;
	ulong msr_low, msr_high;

	// All AMD64 support Chipkill */
	ctrl.cap = ECC_CHIPKILL;

	// Check First if ECC DRAM Modules are used */
	pci_conf_read(0, 24, 2, 0x90, 4, &dramcl);
	
		if ((dramcl >> 19)&1){
			// Fill in the correct memory capabilites */
			pci_conf_read(0, 24, 3, 0x44, 4, &nbxcfg);
			ctrl.mode = ddim[(nbxcfg >> 22)&3];
		} else {
			ctrl.mode = ECC_NONE;
		}
		// Enable NB ECC Logging by MSR Write */
		rdmsr(0x017B, mcgsrl, mcgsth);
		wrmsr(0x017B, 0x10, mcgsth);
	
		// Clear any previous error */
		pci_conf_read(0, 24, 3, 0x4C, 4, &mcanb);
		pci_conf_write(0, 24, 3, 0x4C, 4, mcanb & 0x7FFFFFFF );	
		
		/* Enable ECS */
		rdmsr(0xC001001F, msr_low,  msr_high);
		wrmsr(0xC001001F, msr_low, (msr_high | 0x4000));
		rdmsr(0xC001001F, msr_low,  msr_high);

}

static void setup_apu(void)
{

	ulong msr_low, msr_high;
	
	/* Enable ECS */
	rdmsr(0xC001001F, msr_low,  msr_high);
	wrmsr(0xC001001F, msr_low, (msr_high | 0x4000));
	rdmsr(0xC001001F, msr_low,  msr_high);

}

/*
static void poll_amd64(void)
{

	unsigned long mcanb;
	unsigned long page, offset;
	unsigned long celog_syndrome;
	unsigned long mcanb_add;

	pci_conf_read(0, 24, 3, 0x4C, 4, &mcanb);

	if (((mcanb >> 31)&1) && ((mcanb >> 14)&1)) {
		// Find out about the first correctable error 
		// Syndrome code -> bits use a complex matrix. Will add this later 
		// Read the error location 
		pci_conf_read(0, 24, 3, 0x50, 4, &mcanb_add);

		// Read the syndrome 
		celog_syndrome = (mcanb >> 15)&0xFF;

		// Parse the error location 
		page = (mcanb_add >> 12);
		offset = (mcanb_add >> 3) & 0xFFF;

		// Report the error 
		print_ecc_err(page, offset, 1, celog_syndrome, 0);

		// Clear the error registers 
		pci_conf_write(0, 24, 3, 0x4C, 4, mcanb & 0x7FFFFFFF );
	}
	if (((mcanb >> 31)&1) && ((mcanb >> 13)&1)) {
		// Found out about the first uncorrectable error 
		// Read the error location 
		pci_conf_read(0, 24, 3, 0x50, 4, &mcanb_add);

		// Parse the error location 
		page = (mcanb_add >> 12);
		offset = (mcanb_add >> 3) & 0xFFF;

		// Report the error 
		print_ecc_err(page, offset, 0, 0, 0);

		// Clear the error registers 
		pci_conf_write(0, 24, 3, 0x4C, 4, mcanb & 0x7FFFFFF );

	}

}
*/

static void setup_amd751(void)
{
	unsigned long dram_status;

	/* Fill in the correct memory capabilites */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x5a, 2, &dram_status);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = (dram_status & (1 << 2))?ECC_CORRECT: ECC_NONE;
}

/*
static void poll_amd751(void)
{
	unsigned long ecc_status;
	unsigned long bank_addr;
	unsigned long bank_info;
	unsigned long page;
	int bits;
	int i;

	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x58, 2, &ecc_status);
	if (ecc_status & (3 << 8)) {
		for(i = 0; i < 6; i++) {
			if (!(ecc_status & (1 << i))) {
				continue;
			}
			// Find the bank the error occured on 
			bank_addr = 0x40 + (i << 1);

			// Now get the information on the erroring bank 
			pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, bank_addr, 2, &bank_info);

			// Parse the error location and error type 
			page = (bank_info & 0xFF80) << 4;
			bits = (((ecc_status >> 8) &3) == 2)?1:2;

			// Report the error 
			print_ecc_err(page, 0, bits==1?1:0, 0, 0);

		}

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0x58, 2, 0);
	}
}

// Still waiting for the CORRECT intel datasheet
static void setup_i85x(void)
{
	unsigned long drc;
	ctrl.cap = ECC_CORRECT;

	pci_conf_read(ctrl.bus, ctrl.dev, 1, 0x70, 4, &drc);
	ctrl.mode = ((drc>>20)&1)?ECC_CORRECT:ECC_NONE;

}
*/

static void setup_amd76x(void)
{
	static const int ddim[] = { ECC_NONE, ECC_DETECT, ECC_CORRECT, ECC_CORRECT };
	unsigned long ecc_mode_status;

	/* Fill in the correct memory capabilites */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x48, 4, &ecc_mode_status);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(ecc_mode_status >> 10)&3];
}

/*
static void poll_amd76x(void)
{
	unsigned long ecc_mode_status;
	unsigned long bank_addr;
	unsigned long bank_info;
	unsigned long page;

	// Read the error status
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x48, 4, &ecc_mode_status);
	// Multibit error
	if (ecc_mode_status & (1 << 9)) {
		// Find the bank the error occured on 
		bank_addr = 0xC0 + (((ecc_mode_status >> 4) & 0xf) << 2);

		// Now get the information on the erroring bank 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, bank_addr, 4, &bank_info);

		// Parse the error location and error type 
		page = (bank_info & 0xFF800000) >> 12;

		// Report the error 
		print_ecc_err(page, 0, 1, 0, 0);

	}
	// Singlebit error 
	if (ecc_mode_status & (1 << 8)) {
		// Find the bank the error occured on 
		bank_addr = 0xC0 + (((ecc_mode_status >> 0) & 0xf) << 2);

		// Now get the information on the erroring bank 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, bank_addr, 4, &bank_info);

		// Parse the error location and error type 
		page = (bank_info & 0xFF800000) >> 12;

		// Report the error 
		print_ecc_err(page, 0, 0, 0, 0);

	}
	// Clear the error status 
	if (ecc_mode_status & (3 << 8)) {
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0x48, 4, ecc_mode_status);
	}
}
*/

static void setup_cnb20(void)
{
	/* Fill in the correct memory capabilites */
	ctrl.cap = ECC_CORRECT;

	/* FIXME add ECC error polling.  I don't have the documentation
	 * do it right now.
	 */
}

static void setup_E5400(void)
{
	unsigned long mcs;


	/* Read the hardware capabilities */
	pci_conf_read(ctrl.bus, 16, 1, 0x40, 4, &mcs);

	/* Fill in the correct memory capabilities */
	ctrl.mode = 0;
	ctrl.cap = ECC_SCRUB;

	/* Checking and correcting enabled */
	if (((mcs >> 5) & 1) == 1) {
		ctrl.mode |= ECC_CORRECT;
	}

	/* scrub enabled */
	if (((mcs >> 7) & 1) == 1) {
		ctrl.mode |= __ECC_SCRUB;
	}
}


static void setup_iE7xxx(void)
{
	unsigned long mchcfgns;
	unsigned long drc;
	unsigned long device;
	unsigned long dvnp;

	/* Read the hardare capabilities */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x52, 2, &mchcfgns);
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x7C, 4, &drc);

	/* This is a check for E7205 */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x02, 2, &device);

	/* Fill in the correct memory capabilities */
	ctrl.mode = 0;
	ctrl.cap = ECC_CORRECT;

	/* checking and correcting enabled */
	if (((drc >> 20) & 3) == 2) {
		ctrl.mode |= ECC_CORRECT;
	}

	/* E7205 doesn't support scrubbing */
	if (device != 0x255d) {
		/* scrub enabled */
		/* For E7501, valid SCRUB operations is bit 0 / D0:F0:R70-73 */
		ctrl.cap = ECC_SCRUB;
		if (mchcfgns & 1) {
			ctrl.mode |= __ECC_SCRUB;
		}

		/* Now, we can active Dev1/Fun1 */
		/* Thanks to Tyan for providing us the board to solve this */
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE0, 2, &dvnp);
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn , 0xE0, 2, (dvnp & 0xFE));

		/* Clear any routing of ECC errors to interrupts that the BIOS might have set up */
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x88, 1, 0x0);
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x8A, 1, 0x0);
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x8C, 1, 0x0);
	

	}

	/* Clear any prexisting error reports */
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 1, 3);
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 1, 3);


}

static void setup_iE7520(void)
{
	unsigned long mchscrb;
	unsigned long drc;
	unsigned long dvnp1;

	/* Read the hardare capabilities */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x52, 2, &mchscrb);
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x7C, 4, &drc);

	/* Fill in the correct memory capabilities */
	ctrl.mode = 0;
	ctrl.cap = ECC_CORRECT;

	/* Checking and correcting enabled */
	if (((drc >> 20) & 3) != 0) {
		ctrl.mode |= ECC_CORRECT;
	}

	/* scrub enabled */
	ctrl.cap = ECC_SCRUB;
	if ((mchscrb & 3) == 2) {
		ctrl.mode |= __ECC_SCRUB;
	}

	/* Now, we can activate Fun1 */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xF4, 1, &dvnp1);
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn , 0xF4, 1, (dvnp1 | 0x20));

	/* Clear any prexisting error reports */
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 2, 0x4747);
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 2, 0x4747);

}

/*
static void poll_iE7xxx(void)
{
	unsigned long ferr;
	unsigned long nerr;

	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 1, &ferr);
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 1, &nerr);

	if (ferr & 1) {
		// Find out about the first correctable error 
		unsigned long celog_add;
		unsigned long celog_syndrome;
		unsigned long page;

		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xA0, 4, &celog_add);
		// Read the syndrome 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xD0, 2, &celog_syndrome);

		// Parse the error location 
		page = (celog_add & 0x0FFFFFC0) >> 6;

		// Report the error 
		print_ecc_err(page, 0, 1, celog_syndrome, 0);

		// Clear Bit 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 1, ferr & 3);
	}

	if (ferr & 2) {
		// Found out about the first uncorrectable error 
		unsigned long uccelog_add;
		unsigned long page;

		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xB0, 4, &uccelog_add);

		// Parse the error location 
		page = (uccelog_add & 0x0FFFFFC0) >> 6;

		// Report the error 
		print_ecc_err(page, 0, 0, 0, 0);

		// Clear Bit 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 1, ferr & 3);
	}

	// Check if DRAM_NERR contains data 
	if (nerr & 3) {
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 1, nerr & 3);
	}

}
*/

static void setup_i440gx(void)
{
	static const int ddim[] = { ECC_NONE, ECC_DETECT, ECC_CORRECT, ECC_CORRECT };
	unsigned long nbxcfg;

	/* Fill in the correct memory capabilites */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x50, 4, &nbxcfg);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(nbxcfg >> 7)&3];
}

/*
static void poll_i440gx(void)
{
	unsigned long errsts;
	unsigned long page;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x91, 2, &errsts);
	if (errsts & 0x11) {
		unsigned long eap;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x80, 4, &eap);

		// Parse the error location and error type 
		page = (eap & 0xFFFFF000) >> 12;
		bits = 0;
		if (eap &3) {
			bits = ((eap & 3) == 1)?1:2;
		}

		if (bits) {
			// Report the error 
			print_ecc_err(page, 0, bits==1?1:0, 0, 0);
		}

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0x91, 2, 0x11);
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0x80, 4, 3);
	}

}
*/


static void setup_i840(void)
{
	static const int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_CORRECT };
	unsigned long mchcfg;

	/* Fill in the correct memory capabilites */
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x50, 2, &mchcfg);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(mchcfg >> 7)&3];
}

/*
static void poll_i840(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned long syndrome;
	int channel;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 3) {
		unsigned long eap;
		unsigned long derrctl_sts;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE4, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE2, 2, &derrctl_sts);

		// Parse the error location and error type 
		page = (eap & 0xFFFFF800) >> 11;
		channel = eap & 1;
		syndrome = derrctl_sts & 0xFF;
		bits = ((errsts & 3) == 1)?1:2;

		// Report the error 
		print_ecc_err(page, 0, bits==1?1:0, syndrome, channel);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xE2, 2, 3 << 10);
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, 3);
	}
}
*/


static void setup_i875(void)
{

	long *ptr;
	ulong dev0, dev6 ;

	/* Fill in the correct memory capabilites */

	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ECC_NONE;

	/* From my article : http://www.x86-secret.com/articles/tweak/pat/patsecrets-2.htm */
	/* Activate Device 6 */
	pci_conf_read( 0, 0, 0, 0xF4, 1, &dev0);
	pci_conf_write( 0, 0, 0, 0xF4, 1, (dev0 | 0x2));

	/* Activate Device 6 MMR */
	pci_conf_read( 0, 6, 0, 0x04, 2, &dev6);
	pci_conf_write( 0, 6, 0, 0x04, 2, (dev6 | 0x2));

	/* Read the MMR Base Address & Define the pointer*/
	pci_conf_read( 0, 6, 0, 0x10, 4, &dev6);
	ptr=(long*)(dev6+0x68);

	if (((*ptr >> 18)&1) == 1) { ctrl.mode = ECC_CORRECT; }

	/* Reseting state */
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2,  0x81);
}

static void setup_i925(void)
{

	// Activate MMR I/O
	ulong dev0, drc;
	unsigned long tolm;
	long *ptr;
	
	pci_conf_read( 0, 0, 0, 0x54, 4, &dev0);
	dev0 = dev0 | 0x10000000;
	pci_conf_write( 0, 0, 0, 0x54, 4, dev0);
	
	// CDH start
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	if (!(dev0 & 0xFFFFC000)) {
		pci_conf_read( 0, 0, 0, 0x9C, 1, &tolm);
		pci_conf_write( 0, 0, 0, 0x47, 1, tolm & 0xF8);
	}
	// CDH end

	// ECC Checking
	ctrl.cap = ECC_CORRECT;

	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0x120);
	drc = *ptr & 0xFFFFFFFF;
	
	if (((drc >> 20) & 3) == 2) { 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, 3);
		ctrl.mode = ECC_CORRECT; 
	} else { 
		ctrl.mode = ECC_NONE; 
	}

}

static void setup_p35(void)
{

	// Activate MMR I/O
	ulong dev0, capid0;
	
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	if (!(dev0 & 0x1)) {
		pci_conf_write( 0, 0, 0, 0x48, 1, dev0 | 1);
	}

	// ECC Checking (No poll on X38/48 for now)
	pci_conf_read( 0, 0, 0, 0xE4, 4, &capid0);
	if ((capid0 >> 8) & 1) {
		ctrl.cap = ECC_NONE;
	} else {
		ctrl.cap = ECC_CORRECT;	
	}

	ctrl.mode = ECC_NONE; 
	
	/*
	ulong toto;
	pci_conf_write(0, 31, 3, 0x40, 1,  0x1);
	pci_conf_read(0, 31, 3, 0x0, 4, &toto);
	hprint(11,0,toto);
	pci_conf_read(0, 31, 3, 0x10, 4, &toto);
	hprint(11,10,toto)	;
	pci_conf_read(0, 31, 3, 0x20, 4, &toto);
	hprint(11,20,toto)	;
	pci_conf_read(0, 28, 0, 0x0, 4, &toto);
	hprint(11,30,toto);
	pci_conf_read(0, 31, 0, 0x0, 4, &toto);
	hprint(11,40,toto)	;
	pci_conf_read(0, 31, 1, 0x0, 4, &toto);
	hprint(11,50,toto)	;
	pci_conf_read(0, 31, 2, 0x0, 4, &toto);
	hprint(11,60,toto)	;	
	*/
}

/*
static void poll_i875(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned long des;
	unsigned long syndrome;
	int channel;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 0x81)  {
		unsigned long eap;
		unsigned long derrsyn;
		// Read the error location, syndrome and channel 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x58, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x5C, 1, &derrsyn);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x5D, 1, &des);

		// Parse the error location and error type 
		page = (eap & 0xFFFFF000) >> 12;
		syndrome = derrsyn;
		channel = des & 1;
		bits = (errsts & 0x80)?0:1;

		// Report the error 
		print_ecc_err(page, 0, bits, syndrome, channel);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2,  0x81);
	}
}
*/

static void setup_i845(void)
{
	static const int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
	unsigned long drc;

	// Fill in the correct memory capabilites 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x7C, 4, &drc);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(drc >> 20)&3];
}

/*
static void poll_i845(void)
{
	unsigned long errsts;
	unsigned long page, offset;
	unsigned long syndrome;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 3) {
		unsigned long eap;
		unsigned long derrsyn;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x8C, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x86, 1, &derrsyn);

		// Parse the error location and error type 
		offset = (eap & 0xFE) << 4;
		page = (eap & 0x3FFFFFFE) >> 8;
		syndrome = derrsyn;
		bits = ((errsts & 3) == 1)?1:2;

		// Report the error 
		print_ecc_err(page, offset, bits==1?1:0, syndrome, 0);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, 3);
	}
}
*/


static void setup_i820(void)
{
	static const int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_CORRECT };
	unsigned long mchcfg;

	// Fill in the correct memory capabilites 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xbe, 2, &mchcfg);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(mchcfg >> 7)&3];
}

/*
static void poll_i820(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned long syndrome;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 3) {
		unsigned long eap;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xc4, 4, &eap);

		// Parse the error location and error type 
		page = (eap & 0xFFFFF000) >> 4;
		syndrome = eap & 0xFF;
		bits = ((errsts & 3) == 1)?1:2;

		// Report the error 
		print_ecc_err(page, 0, bits==1?1:0, syndrome, 0);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, 3);
	}
}
*/

static void setup_i850(void)
{
	static const int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
	unsigned long mchcfg;

	// Fill in the correct memory capabilites 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x50, 2, &mchcfg);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(mchcfg >> 7)&3];
}

/*
static void poll_i850(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned long syndrome;
	int channel;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 3) {
		unsigned long eap;
		unsigned long derrctl_sts;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE4, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE2, 2, &derrctl_sts);

		// Parse the error location and error type 
		page = (eap & 0xFFFFF800) >> 11;
		channel = eap & 1;
		syndrome = derrctl_sts & 0xFF;
		bits = ((errsts & 3) == 1)?1:2;

		// Report the error 
		print_ecc_err(page, 0, bits==1?1:0, syndrome, channel);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, errsts & 3);
	}
}
*/

static void setup_i860(void)
{
	static const int ddim[] = { ECC_NONE, ECC_RESERVED, ECC_CORRECT, ECC_RESERVED };
	unsigned long mchcfg;
	unsigned long errsts;

	// Fill in the correct memory capabilites 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x50, 2, &mchcfg);
	ctrl.cap = ECC_CORRECT;
	ctrl.mode = ddim[(mchcfg >> 7)&3];

	// Clear any prexisting error reports 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, errsts & 3);
}

/*
static void poll_i860(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned char syndrome;
	int channel;
	int bits;
	// Read the error status 
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	if (errsts & 3) {
		unsigned long eap;
		unsigned long derrctl_sts;
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE4, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xE2, 2, &derrctl_sts);

		// Parse the error location and error type 
		page = (eap & 0xFFFFFE00) >> 9;
		channel = eap & 1;
		syndrome = derrctl_sts & 0xFF;
		bits = ((errsts & 3) == 1)?1:2;

		// Report the error 
		print_ecc_err(page, 0, bits==1?1:0, syndrome, channel);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, errsts & 3);
	}
}


static void poll_iE7221(void)
{
	unsigned long errsts;
	unsigned long page;
	unsigned char syndrome;
	int channel;
	int bits;
	int errocc;
	
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, &errsts);
	
	errocc = errsts & 3;
	
	if ((errocc == 1) || (errocc == 2)) {
		unsigned long eap, offset;
		unsigned long derrctl_sts;		
		
		// Read the error location 
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x58, 4, &eap);
		pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, 0x5C, 1, &derrctl_sts);		
		
		// Parse the error location and error type 
		channel = eap & 1;
		eap = eap & 0xFFFFFF80;
		page = eap >> 12;
		offset = eap & 0xFFF;
		syndrome = derrctl_sts & 0xFF;		
		bits = errocc & 1;

		// Report the error 
		print_ecc_err(page, offset, bits, syndrome, channel);

		// Clear the error status 
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, errsts & 3);
	} 
	
	else if (errocc == 3) {
	
		pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn, 0xC8, 2, errsts & 3);	
	
	}
}


static void poll_iE7520(void)
{
	unsigned long ferr;
	unsigned long nerr;

	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 2, &ferr);
	pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 2, &nerr);

	if (ferr & 0x0101) {
			// Find out about the first correctable error 
			unsigned long celog_add;
			unsigned long celog_syndrome;
			unsigned long page;

			// Read the error location 
			pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xA0, 4,&celog_add);
			// Read the syndrome 
			pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xC4, 2, &celog_syndrome);

			// Parse the error location 
			page = (celog_add & 0x7FFFFFFC) >> 2;

			// Report the error 
			print_ecc_err(page, 0, 1, celog_syndrome, 0);

			// Clear Bit 
			pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 2, ferr& 0x0101);
	}

	if (ferr & 0x4646) {
			// Found out about the first uncorrectable error 
			unsigned long uccelog_add;
			unsigned long page;

			// Read the error location 
			pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn +1, 0xA4, 4, &uccelog_add);

			// Parse the error location 
			page = (uccelog_add & 0x7FFFFFFC) >> 2;

			// Report the error 
			print_ecc_err(page, 0, 0, 0, 0);

			// Clear Bit 
			pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x80, 2, ferr & 0x4646);
	}

	// Check if DRAM_NERR contains data 
	if (nerr & 0x4747) {
			 pci_conf_write(ctrl.bus, ctrl.dev, ctrl.fn +1, 0x82, 2, nerr & 0x4747);
	}
}
*/



/* ----------------- Here's the code for FSB detection ----------------- */
/* --------------------------------------------------------------------- */

static float athloncoef[] = {11, 11.5, 12.0, 12.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0, 10.5};
static float athloncoef2[] = {12, 19.0, 12.0, 20.0, 13.0, 13.5, 14.0, 21.0, 15.0, 22, 16.0, 16.5, 17.0, 18.0, 23.0, 24.0};
static float p4model1ratios[] = {16, 17, 18, 19, 20, 21, 22, 23, 8, 9, 10, 11, 12, 13, 14, 15};

static float getP4PMmultiplier(void)
{
	//unsigned int msr_lo, msr_hi;
	int msr_lo, msr_hi;
	float coef;

	
	/* Find multiplier (by MSR) */
	if (cpu_id.vers.bits.family == 6) 
	{
		if(cpu_id.fid.bits.eist & 1) 
		{
			rdmsr(0x198, msr_lo, msr_hi);
			coef = ((msr_lo) >> 8) & 0x1F;							
			if ((msr_lo >> 14) & 0x1) { coef += 0.5f; }		
			// Atom Fix
			if(coef == 6)
			{
				coef = ((msr_hi) >> 8) & 0x1F;							
				if ((msr_hi >> 14) & 0x1) { coef += 0.5f; }							
			}
		} else {
			rdmsr(0x2A, msr_lo, msr_hi);
			coef = (msr_lo >> 22) & 0x1F;
		}
	}
	else
	{
		if (cpu_id.vers.bits.model < 2)
		{
			rdmsr(0x2A, msr_lo, msr_hi);
			coef = (msr_lo >> 8) & 0xF;
			coef = p4model1ratios[(int)coef];
		}
		else
		{
			rdmsr(0x2C, msr_lo, msr_hi);
			coef = (msr_lo >> 24) & 0x1F;
		}
	}
	
	return coef;
}

static float getNHMmultiplier(void)
{
	unsigned int msr_lo, msr_hi;
	float coef;
	
	/* Find multiplier (by MSR) */
	/* First, check if Flexible Ratio is Enabled */
	rdmsr(0x194, msr_lo, msr_hi);
	if((msr_lo >> 16) & 1){
		coef = (msr_lo >> 8) & 0xFF;
	 } else {
		rdmsr(0xCE, msr_lo, msr_hi);
		coef = (msr_lo >> 8) & 0xFF;
	 }

	return coef;
}
static float getSNBmultiplier(void)
{
	unsigned int msr_lo, msr_hi;
	float coef;
	
	rdmsr(0xCE, msr_lo, msr_hi);
	coef = (msr_lo >> 8) & 0xFF;		

	return coef;
}

static void poll_fsb_ct(void) 
{
	unsigned long mcr, mdr;
	double dramratio, dramclock, fsb;
	float coef = getP4PMmultiplier();

	/* Build the MCR Message*/
	mcr = (0x10 << 24); // 10h = Read - 11h = Write
	mcr += (0x01 << 16); // DRAM Registers located on port 01h
	mcr += (0x01 << 8); // DRP = 00h, DTR0 = 01h, DTR1 = 02h, DTR2 = 03h
	mcr &= 0xFFFFFFF0; // bit 03:00 RSVD	
	
	/* Send Message to GMCH */
	pci_conf_write(0, 0, 0, 0xD0, 4, mcr);	
	
	/* Read Answer from Sideband bus */
	pci_conf_read(0, 0, 0, 0xD4, 4, &mdr);			
	
	/* Get RAM ratio */
	switch (mdr & 0x3) {
		default: 
		case 0:	dramratio = 3.0f; break;
		case 1:	dramratio = 4.0f; break;
		case 2:	dramratio = 5.0f; break;
		case 3:	dramratio = 6.0f; break;
	}

	// Compute FSB & RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	// Print'em all. Whoa !
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_amd64(void) {

	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long fid, temp2;
	unsigned long dramchr;
	float clockratio;
	double dramclock;
	unsigned int dummy[3];
	int ram_type;

	float coef = 10;

	cpuid(0x80000007, &dummy[0], &dummy[1], &dummy[2], &dummy[3]);

	/* First, got the FID by MSR */
	/* First look if Cool 'n Quiet is supported to choose the best msr */
	if (((dummy[3] >> 1) & 1) == 1) {
		rdmsr(0xc0010042, mcgsrl, mcgsth);
		fid = (mcgsrl & 0x3F);
	} else {
		rdmsr(0xc0010015, mcgsrl, mcgsth);
		fid = ((mcgsrl >> 24)& 0x3F);
	}
	
	/* Extreme simplification. */
	coef = ( fid / 2 ) + 4.0;

	/* Support for .5 coef */
	if (fid & 1) { coef = coef + 0.5; }

	/* Next, we need the clock ratio */
	if (cpu_id.vers.bits.extendedModel >= 4) {
	/* K8 0FH */
		pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
		temp2 = (dramchr & 0x7);
		clockratio = coef;
		ram_type = 2;
	
		switch (temp2) {
			case 0x0:
				clockratio = (int)(coef);
				break;
			case 0x1:
				clockratio = (int)(coef * 3.0f/4.0f);
				break;
			case 0x2:
				clockratio = (int)(coef * 3.0f/5.0f);
				break;
			case 0x3:
				clockratio = (int)(coef * 3.0f/6.0f);
				break;
			}	
	
	 } else {
	 /* OLD K8 */
		pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
		temp2 = (dramchr >> 20) & 0x7;
		ram_type = 1;
		clockratio = coef;
	
		switch (temp2) {
			case 0x0:
				clockratio = (int)(coef * 2.0f);
				break;
			case 0x2:
				clockratio = (int)((coef * 3.0f/2.0f) + 0.81f);
				break;
			case 0x4:
				clockratio = (int)((coef * 4.0f/3.0f) + 0.81f);
				break;
			case 0x5:
				clockratio = (int)((coef * 6.0f/5.0f) + 0.81f);
				break;
			case 0x6:
				clockratio = (int)((coef * 10.0f/9.0f) + 0.81f);
				break;
			case 0x7:
				clockratio = (int)(coef + 0.81f);
				break;
			}
	}

	/* Compute the final DRAM Clock */
	dramclock = (extclock / 1000) / clockratio;

	/* ...and print */
	print_cpu_line(dramclock, (extclock / 1000 / coef), ram_type);

}

static void poll_fsb_k10(void) {

	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long temp2;
	unsigned long dramchr;
	unsigned long mainPllId;
	double dramclock;
	ulong offset = 0;
	int ram_type = 2;
	
		/* First, we need the clock ratio */
		pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
		temp2 = (dramchr & 0x7);

		switch (temp2) {
			case 0x7: temp2++;
			case 0x6: temp2++;
			case 0x5: temp2++;
			case 0x4: temp2++;
			default:  temp2 += 3;
		}	

	/* Compute the final DRAM Clock */
	if (((cpu_id.vers.bits.extendedModel >> 4) & 0xFF) == 1) {
		dramclock = ((temp2 * 200) / 3.0) + 0.25;
	} else {
		unsigned long target;
		unsigned long dx;
		unsigned      divisor;

		target = temp2 * 400;

		/* Get the FID by MSR */
		rdmsr(0xc0010071, mcgsrl, mcgsth);

		pci_conf_read(0, 24, 3, 0xD4, 4, &mainPllId);

		if ( mainPllId & 0x40 )
			mainPllId &= 0x3F;
		else
			mainPllId = 8;	/* FID for 1600 */

		mcgsth = (mcgsth >> 17) & 0x3F;
		if ( mcgsth ) {
			if ( mainPllId > mcgsth )
				mainPllId = mcgsth;
		}

		dx = (mainPllId + 8) * 1200;
		for ( divisor = 3; divisor < 100; divisor++ )
			if ( (dx / divisor) <= target )
				break;

		
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
	
	// If Channel A not enabled, switch to channel B
	if(((dramchr>>14) & 0x1))
	{
		offset = 0x100;
		pci_conf_read(0, 24, 2, 0x94+offset, 4, &dramchr);	
	}
	
	//DDR2 or DDR3
	if ((dramchr >> 8)&1) {
		ram_type = 3;
	} else {
		ram_type = 2;;
	}
		
		dramclock = ((dx / divisor) / 6.0) + 0.25;
}

	/* ...and print */
	print_cpu_line(dramclock, 0, ram_type);

}

static void poll_fsb_k12(void) {

	unsigned long temp2;
	unsigned long dramchr;
	double dramratio, dramclock, fsb, did;
	unsigned int mcgsrl,mcgsth, fid, did_raw;
	 
	// Get current FID & DID
 	rdmsr(0xc0010071, mcgsrl, mcgsth);
 	did_raw = mcgsrl & 0xF;
 	fid = (mcgsrl >> 4) & 0xF;
  
	switch(did_raw)
	{
		default:
		case 0x0:
			did = 1.0f;
			break;
		case 0x1:
			did = 1.5f;
			break;
		case 0x2:
			did = 2.0f;
			break;					
		case 0x3:
			did = 3.0f;
			break;		
		case 0x4:
			did = 4.0f;
			break;
		case 0x5:
			did = 6.0f;
			break;
		case 0x6:
			did = 8.0f;
			break;			
		case 0x7:
			did = 12.0f;
			break;			
		case 0x8:
			did = 16.0f;
			break;	
	}

  fsb = ((extclock / 1000.0f) / ((fid + 16.0f) / did));
		
	/* Finaly, we need the clock ratio */
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
	
	if(((dramchr >> 14) & 0x1) == 1)
	{
		pci_conf_read(0, 24, 2, 0x194, 4, &dramchr);				
	}
	
	temp2 = (dramchr & 0x1F);

	switch (temp2) {
		default:
		case 0x06: 
			dramratio = 4.0f; 
			break;
		case 0x0A: 
			dramratio = 16.0f / 3.0f; 
			break;
		case 0x0E: 
			dramratio = 20.0f / 3.0f; 
			break;
		case 0x12: 
			dramratio = 8.0f; 
			break;
		case 0x16: 
			dramratio = 28.0f / 3.0f; 
			break;						
	}	
	
	dramclock = fsb * dramratio;
	
	/* print */
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_k16(void) 
{

	unsigned long dramchr;
	double dramratio, dramclock, fsb;

	// FIXME: Unable to find a real way to detect multiplier. 
	fsb = 100.0f;
		
	/* Clock ratio */
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);

	switch (dramchr & 0x1F) {
		default:
		case 0x04: /* 333 */
			dramratio = 10.0f / 3.0f; 
			break;			
		case 0x06: /* 400 */
			dramratio = 4.0f; 
			break;
		case 0x0A: /* 533 */
			dramratio = 16.0f / 3.0f; 
			break;
		case 0x0E: /* 667 */
			dramratio = 20.0f / 3.0f; 
			break;
		case 0x12: /* 800 */
			dramratio = 8.0f; 
			break;	
		case 0x16: /* 933 */
			dramratio = 28.0f / 3.0f;
			break;
		case 0x19: /* 1050 */
			dramratio = 21.0f / 2.0f;
			break;
		case 0x1A: /* 1066 */
			dramratio = 32.0f / 3.0f;
			break;	
	}	
	
	dramclock = fsb * dramratio;
	
	/* print */
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_k15(void) {

	unsigned long temp2;
	unsigned long dramchr;
	double dramratio, dramclock, fsb;
	unsigned int mcgsrl,mcgsth, fid, did;
	 
	// Get current FID & DID
 	rdmsr(0xc0010071, mcgsrl, mcgsth);
 	fid = mcgsrl & 0x3F;
 	did = (mcgsrl >> 6) & 0x7;
  
  fsb = ((extclock / 1000.0f) / ((fid + 16.0f) / (2^did)) / 2);
	
	/* Finaly, we need the clock ratio */
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
	
	if(((dramchr >> 14) & 0x1) == 1)
	{
		pci_conf_read(0, 24, 2, 0x194, 4, &dramchr);				
	}
	
	temp2 = (dramchr & 0x1F);

	switch (temp2) {
		case 0x04: 
			dramratio = 10.0f / 3.0f; 
			break;
		default:
		case 0x06: 
			dramratio = 4.0f; 
			break;
		case 0x0A: 
			dramratio = 16.0f / 3.0f; 
			break;
		case 0x0E: 
			dramratio = 20.0f / 3.0f; 
			break;
		case 0x12: 
			dramratio = 8.0f; 
			break;
		case 0x16: 
			dramratio = 28.0f / 3.0f; 
			break;						
		case 0x1A: 
			dramratio = 32.0f / 3.0f; 
			break;	
		case 0x1F: 
			dramratio = 36.0f / 3.0f; 
			break;				
	}	
	
	dramclock = fsb * dramratio;
	
	/* print */
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_k14(void) 
{

	unsigned long dramchr;
	double dramratio, dramclock, fsb;

	// FIXME: Unable to find a real way to detect multiplier. 
  fsb = 100.0f;
		
	/* Clock ratio */
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);

	switch (dramchr & 0x1F) {
		default:
		case 0x06: 
			dramratio = 4.0f; 
			break;
		case 0x0A: 
			dramratio = 16.0f / 3.0f; 
			break;
		case 0x0E: 
			dramratio = 20.0f / 3.0f; 
			break;
		case 0x12: 
			dramratio = 8.0f; 
			break;					
	}	
	
	dramclock = fsb * dramratio;
	
	/* print */
	print_cpu_line(dramclock, fsb, 3);

}


static void poll_fsb_i925(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, mchcfg2, dev0, drc, idetect;
	float coef = getP4PMmultiplier();
	long *ptr;
	int ddr_type;
	
	pci_conf_read( 0, 0, 0, 0x02, 2, &idetect);
	
	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	dev0 = dev0 & 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	ptr=(long*)(dev0+0x120);
	drc = *ptr & 0xFFFF;
	dramratio = 1;

	mchcfg2 = (mchcfg >> 4)&3;
	
	if ((drc&3) != 2) {
		// We are in DDR1 Mode
		if (mchcfg2 == 1) { dramratio = 0.8; } else { dramratio = 1; }
		ddr_type = 1;
	} else {
		// We are in DDR2 Mode
		ddr_type = 2;
		if ((mchcfg >> 2)&1) {
			// We are in FSB1066 Mode
			if (mchcfg2 == 2) { dramratio = 0.75; } else { dramratio = 1; }
		} else {
			switch (mchcfg2) {
				case 1:
					dramratio = 0.66667;
					break;
				case 2:
					if (idetect != 0x2590) { dramratio = 1; } else { dramratio = 1.5; }
					break;
				case 3:
						// Checking for FSB533 Mode & Alviso
						if ((mchcfg & 1) == 0) { dramratio = 1.33334; }
						else if (idetect == 0x2590) { dramratio = 2; }
						else { dramratio = 1.5; }
			}
		}
	}
	// Compute RAM Frequency 
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	
	print_cpu_line(dramclock, fsb, ddr_type);
	
}

static void poll_fsb_i945(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0;
	float coef = getP4PMmultiplier();
	long *ptr;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch ((mchcfg >> 4)&7) {
		case 1:	dramratio = 1.0; break;
		case 2:	dramratio = 1.33334; break;
		case 3:	dramratio = 1.66667; break;
		case 4:	dramratio = 2.0; break;
	}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);

	dramclock = fsb * dramratio;

	// Print
	print_cpu_line(dramclock, fsb, 2);

}

static void poll_fsb_i945gme(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0, fsb_mch;
	float coef = getP4PMmultiplier();
	long *ptr;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch (mchcfg & 7) {
		case 0: fsb_mch = 400; break;
		default:
		case 1: fsb_mch = 533; break;
		case 2:	fsb_mch = 667; break;	
	}


	switch (fsb_mch) {
	case 400:
		switch ((mchcfg >> 4)&7) {
			case 2:	dramratio = 1.0f; break;
			case 3:	dramratio = 4.0f/3.0f; break;
			case 4:	dramratio = 5.0f/3.0f; break;
		}
		break;
		
	default:
	case 533:
		switch ((mchcfg >> 4)&7) {
			case 2:	dramratio = 3.0f/4.0f; break;
			case 3:	dramratio = 1.0f; break;
			case 4:	dramratio = 5.0f/4.0f; break;
		}
		break;

	case 667:
		switch ((mchcfg >> 4)&7) {
			case 2:	dramratio = 3.0f/5.0f; break;
			case 3:	dramratio = 4.0f/5.0f; break;
			case 4:	dramratio = 1.0f; break;
		}
		break;
	}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio * 2;

	print_cpu_line(dramclock, fsb, 2);

}


static void poll_fsb_i975(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0, fsb_mch;
	float coef = getP4PMmultiplier();
	long *ptr;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch (mchcfg & 7) {
		case 1: fsb_mch = 533; break;
		case 2:	fsb_mch = 800; break;
		case 3:	fsb_mch = 667; break;				
		default: fsb_mch = 1066; break;
	}


	switch (fsb_mch) {
	case 533:
		switch ((mchcfg >> 4)&7) {
			case 0:	dramratio = 1.25; break;
			case 1:	dramratio = 1.5; break;
			case 2:	dramratio = 2.0; break;
		}
		break;
		
	default:
	case 800:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 1.0; break;
			case 2:	dramratio = 1.33334; break;
			case 3:	dramratio = 1.66667; break;
			case 4:	dramratio = 2.0; break;
		}
		break;

	case 1066:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 0.75; break;
			case 2:	dramratio = 1.0; break;
			case 3:	dramratio = 1.25; break;
			case 4:	dramratio = 1.5; break;
		}
		break;
	}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	print_cpu_line(dramclock, fsb, 2);

}

static void poll_fsb_i965(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0, fsb_mch;
	float coef = getP4PMmultiplier();
	long *ptr;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch (mchcfg & 7) {
		case 0: fsb_mch = 1066; break;
		case 1: fsb_mch = 533; break;
		default: case 2:	fsb_mch = 800; break;
		case 3:	fsb_mch = 667; break;		
		case 4: fsb_mch = 1333; break;
		case 6: fsb_mch = 1600; break;					
	}


	switch (fsb_mch) {
	case 533:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 2.0; break;
			case 2:	dramratio = 2.5; break;
			case 3:	dramratio = 3.0; break;
		}
		break;
		
	default:
	case 800:
		switch ((mchcfg >> 4)&7) {
			case 0:	dramratio = 1.0; break;
			case 1:	dramratio = 5.0f/4.0f; break;
			case 2:	dramratio = 5.0f/3.0f; break;
			case 3:	dramratio = 2.0f; break;
			case 4:	dramratio = 8.0f/3.0f; break;
			case 5:	dramratio = 10.0f/3.0f; break;
		}
		break;

	case 1066:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 1.0f; break;
			case 2:	dramratio = 5.0f/4.0f; break;
			case 3:	dramratio = 3.0f/2.0f; break;
			case 4:	dramratio = 2.0f; break;
			case 5:	dramratio = 5.0f/2.0f; break;
		}
		break;
	
	case 1333:
		switch ((mchcfg >> 4)&7) {
			case 2:	dramratio = 1.0f; break;
			case 3:	dramratio = 6.0f/5.0f; break;
			case 4:	dramratio = 8.0f/5.0f; break;
			case 5:	dramratio = 2.0f; break;
		}
		break;

	case 1600:
		switch ((mchcfg >> 4)&7) {
			case 3:	dramratio = 1.0f; break;
			case 4:	dramratio = 4.0f/3.0f; break;
			case 5:	dramratio = 3.0f/2.0f; break;
			case 6:	dramratio = 2.0f; break;
		}
		break;

}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 2);

}

static void poll_fsb_p35(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0, fsb_mch, Device_ID, Memory_Check,	c0ckectrl, offset;
	float coef = getP4PMmultiplier();
	long *ptr;
	int ram_type;

	pci_conf_read( 0, 0, 0, 0x02, 2, &Device_ID);
	Device_ID &= 0xFFFF;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	
	ptr = (long*)(dev0+0x260);
	c0ckectrl = *ptr & 0xFFFFFFFF;	

	
	// If DIMM 0 not populated, check DIMM 1
	((c0ckectrl) >> 20 & 0xF)?(offset = 0):(offset = 0x400);
	
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch (mchcfg & 7) {
		case 0: fsb_mch = 1066; break;
		case 1: fsb_mch = 533; break;
		default: case 2:	fsb_mch = 800; break;
		case 3:	fsb_mch = 667; break;		
		case 4: fsb_mch = 1333; break;
		case 6: fsb_mch = 1600; break;					
	}


	switch (fsb_mch) {
	case 533:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 2.0; break;
			case 2:	dramratio = 2.5; break;
			case 3:	dramratio = 3.0; break;
		}
		break;
		
	default:
	case 800:
		switch ((mchcfg >> 4)&7) {
			case 0:	dramratio = 1.0; break;
			case 1:	dramratio = 5.0f/4.0f; break;
			case 2:	dramratio = 5.0f/3.0f; break;
			case 3:	dramratio = 2.0; break;
			case 4:	dramratio = 8.0f/3.0f; break;
			case 5:	dramratio = 10.0f/3.0f; break;
		}
		break;

	case 1066:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 1.0f; break;
			case 2:	dramratio = 5.0f/4.0f; break;
			case 3:	dramratio = 3.0f/2.0f; break;
			case 4:	dramratio = 2.0f; break;
			case 5:	dramratio = 5.0f/2.0f; break;
		}
		break;
	
	case 1333:
		switch ((mchcfg >> 4)&7) {
			case 2:	dramratio = 1.0f; break;
			case 3:	dramratio = 6.0f/5.0f; break;
			case 4:	dramratio = 8.0f/5.0f; break;
			case 5:	dramratio = 2.0f; break;
		}
		break;

	case 1600:
		switch ((mchcfg >> 4)&7) {
			case 3:	dramratio = 1.0f; break;
			case 4:	dramratio = 4.0f/3.0f; break;
			case 5:	dramratio = 3.0f/2.0f; break;
			case 6:	dramratio = 2.0f; break;
		}
		break;

	}

	// On P45, check 1A8
	if(Device_ID > 0x2E00 && imc_type != 8) {
		ptr = (long*)(dev0+offset+0x1A8);
		Memory_Check = *ptr & 0xFFFFFFFF;	
		Memory_Check >>= 2;
		Memory_Check &= 1;
		Memory_Check = !Memory_Check;
	} else if (imc_type == 8) {
		ptr = (long*)(dev0+offset+0x224);
		Memory_Check = *ptr & 0xFFFFFFFF;	
		Memory_Check &= 1;
		Memory_Check = !Memory_Check;		
	} else {
		ptr = (long*)(dev0+offset+0x1E8);
		Memory_Check = *ptr & 0xFFFFFFFF;		
	}

	//Determine DDR-II or DDR-III
	if (Memory_Check & 1) {
		ram_type = 2;
	} else {
		ram_type = 3;
	}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, ram_type);

}

static void poll_fsb_im965(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, dev0, fsb_mch;
	float coef = getP4PMmultiplier();
	long *ptr;

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0xC00);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;

	switch (mchcfg & 7) {
		case 1: fsb_mch = 533; break;
		default: case 2:	fsb_mch = 800; break;
		case 3:	fsb_mch = 667; break;				
		case 6:	fsb_mch = 1066; break;			
	}


	switch (fsb_mch) {
	case 533:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 5.0f/4.0f; break;
			case 2:	dramratio = 3.0f/2.0f; break;
			case 3:	dramratio = 2.0f; break;
		}
		break;

	case 667:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 1.0f; break;
			case 2:	dramratio = 6.0f/5.0f; break;
			case 3:	dramratio = 8.0f/5.0f; break;
			case 4:	dramratio = 2.0f; break;
			case 5:	dramratio = 12.0f/5.0f; break;
		}
		break;
	default:
	case 800:
		switch ((mchcfg >> 4)&7) {
			case 1:	dramratio = 5.0f/6.0f; break;
			case 2:	dramratio = 1.0f; break;
			case 3:	dramratio = 4.0f/3.0f; break;
			case 4:	dramratio = 5.0f/3.0f; break;
			case 5:	dramratio = 2.0f; break;
		}
		break;
	case 1066:
		switch ((mchcfg >> 4)&7) {
			case 5:	dramratio = 3.0f/2.0f; break;
			case 6:	dramratio = 2.0f; break;
		}
		break;
}

	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 2);

}


static void poll_fsb_5400(void) {

	double dramclock, dramratio, fsb;
	unsigned long ambase_low, ambase_high, ddrfrq;
	float coef = getP4PMmultiplier();

	/* Find dramratio */
	pci_conf_read( 0, 16, 0, 0x48, 4, &ambase_low);
	ambase_low &= 0xFFFE0000;
	pci_conf_read( 0, 16, 0, 0x4C, 4, &ambase_high);
	ambase_high &= 0xFF;
	pci_conf_read( 0, 16, 1, 0x56, 1, &ddrfrq);
  ddrfrq &= 7;
  dramratio = 1;

	switch (ddrfrq) {
			case 0:	
			case 1:	
			case 4:					
				dramratio = 1.0; 
				break;
			case 2:	
				dramratio = 5.0f/4.0f; 
				break;
			case 3:	
			case 7:	
				dramratio = 4.0f/5.0f; 
				break;
		}


	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 2);

}


static void poll_fsb_nf4ie(void) {

	double dramclock, dramratio, fsb;
	float mratio, nratio;
	unsigned long reg74, reg60;
	float coef = getP4PMmultiplier();
	
	/* Find dramratio */
	pci_conf_read(0, 0, 2, 0x74, 2, &reg74);
	pci_conf_read(0, 0, 2, 0x60, 4, &reg60);
	mratio = reg74 & 0xF;
	nratio = (reg74 >> 4) & 0xF;

	// If M or N = 0, then M or N = 16
	if (mratio == 0) { mratio = 16; }
	if (nratio == 0) { nratio = 16; }
	
	// Check if synchro or pseudo-synchro mode
	if((reg60 >> 22) & 1) {
		dramratio = 1;
	} else {
		dramratio = nratio / mratio;
	}

	/* Compute RAM Frequency */
	fsb = ((extclock /1000) / coef);
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 2);
	
}

static void poll_fsb_i875(void) {

	double dramclock, dramratio, fsb;
	unsigned long mchcfg, smfs;
	float coef = getP4PMmultiplier();

	/* Find dramratio */
	pci_conf_read(0, 0, 0, 0xC6, 2, &mchcfg);
	smfs = (mchcfg >> 10)&3;
	dramratio = 1;

	if ((mchcfg&3) == 3) { dramratio = 1; }
	if ((mchcfg&3) == 2) {
		if (smfs == 2) { dramratio = 1; }
		if (smfs == 1) { dramratio = 1.25; }
		if (smfs == 0) { dramratio = 1.5; }
	}
	if ((mchcfg&3) == 1) {
		if (smfs == 2) { dramratio = 0.6666666666; }
		if (smfs == 1) { dramratio = 0.8; }
		if (smfs == 0) { dramratio = 1; }
	}
	if ((mchcfg&3) == 0) { dramratio = 0.75; }


	/* Compute RAM Frequency */
	dramclock = ((extclock /1000) / coef) / dramratio;
	fsb = ((extclock /1000) / coef);

	/* Print DRAM Freq */
	print_cpu_line(dramclock, fsb, 2);
}

static void poll_fsb_p4(void) {

	ulong fsb, idetect;
	float coef = getP4PMmultiplier();
	char *name;
	int col,temp;

	fsb = ((extclock /1000) / coef);

	/* For synchro only chipsets */
	pci_conf_read( 0, 0, 0, 0x02, 2, &idetect);
	if (idetect == 0x2540 || idetect == 0x254C) 
	{
		print_cpu_line(fsb, fsb, 1);
	} else {
		/* Print the controller name */
		col = COL_SPEC;
		cprint(LINE_CPU, col, "Chipset:               ");	
		col += 9;
		/* Print the controller name */
		name = controllers[ctrl.index].name;
		cprint(LINE_CPU, col, name);
		/* Now figure out how much I just printed */
		temp = 20;
		while(name[temp - 20] != '\0') {
			col++;
			temp++;
		}
		
		if(temp < 36){
			cprint(LINE_CPU, col +1, "- FSB : ");
			col += 9;
			dprint(LINE_CPU, col, fsb, 3,0);
			col += 3;
		}
	
	}
}

static void poll_fsb_i855(void) {


	double dramclock, dramratio, fsb ;
	unsigned int msr_lo, msr_hi;
	ulong mchcfg, idetect;
	int coef;

	pci_conf_read( 0, 0, 0, 0x02, 2, &idetect);

	/* Find multiplier (by MSR) */

	/* Is it a Pentium M ? */
	if (cpu_id.vers.bits.family == 6) {
		rdmsr(0x2A, msr_lo, msr_hi);
		coef = (msr_lo >> 22) & 0x1F;
	} else {
		rdmsr(0x2C, msr_lo, msr_hi);
		coef = (msr_lo >> 24) & 0x1F;
	}

	fsb = ((extclock /1000) / coef);


	/* Compute DRAM Clock */

	dramratio = 1;
	if (idetect == 0x3580) {
		pci_conf_read( 0, 0, 3, 0xC0, 2, &mchcfg);
		mchcfg = mchcfg & 0x7;

		if (mchcfg == 1 || mchcfg == 2 || mchcfg == 4 || mchcfg == 5) {	dramratio = 1; }
		if (mchcfg == 0 || mchcfg == 3) { dramratio = 1.333333333; }
		if (mchcfg == 6) { dramratio = 1.25; }
		if (mchcfg == 7) { dramratio = 1.666666667; }

	} else {
		pci_conf_read( 0, 0, 0, 0xC6, 2, &mchcfg);
		if (((mchcfg >> 10)&3) == 0) { dramratio = 1; }
		else if (((mchcfg >> 10)&3) == 1) { dramratio = 1.666667; }
		else { dramratio = 1.333333333; }
	}


	dramclock = fsb * dramratio;

	/* ...and print */
	print_cpu_line(dramclock, fsb, 1);

}

static void poll_fsb_amd32(void) {

	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long temp;
	double dramclock;
	double coef2;
	int col;
	char *name;

	/* First, got the FID */
	rdmsr(0x0c0010015, mcgsrl, mcgsth);
	temp = (mcgsrl >> 24)&0x0F;

	if ((mcgsrl >> 19)&1) { coef2 = athloncoef2[temp]; }
	else { coef2 = athloncoef[temp]; }

	if (coef2 == 0) { coef2 = 1; };

	/* Compute the final FSB Clock */
	dramclock = (extclock /1000) / coef2;

	/* Print the controller name */
	col = COL_SPEC;
	cprint(LINE_CPU, col, "Chipset:               ");	
	col += 9;
	/* Print the controller name */
	name = controllers[ctrl.index].name;
	cprint(LINE_CPU, col, name);
	/* Now figure out how much I just printed */
	temp = 20;
	while(name[temp - 20] != '\0') {
		col++;
		temp++;
	}
	
	if(temp < 36){
		cprint(LINE_CPU, col +1, "- FSB : ");
		col += 9;
		dprint(LINE_CPU, col, dramclock, 3,0);
		col += 3;
	}


}

static void poll_fsb_nf2(void) {

	unsigned int mcgsrl;
	unsigned int mcgsth;
	unsigned long temp, mempll;
	double dramclock, fsb;
	double mem_m, mem_n;
	float coef;
	coef = 10;

	/* First, got the FID */
	rdmsr(0x0c0010015, mcgsrl, mcgsth);
	temp = (mcgsrl >> 24)&0x0F;

	if ((mcgsrl >> 19)&1) { coef = athloncoef2[temp]; }
	else { coef = athloncoef[temp]; }

	/* Get the coef (COEF = N/M) - Here is for Crush17 */
	pci_conf_read(0, 0, 3, 0x70, 4, &mempll);
	mem_m = (mempll&0x0F);
	mem_n = ((mempll >> 4) & 0x0F);

	/* If something goes wrong, the chipset is probably a Crush18 */
	if ( mem_m == 0 || mem_n == 0 ) {
		pci_conf_read(0, 0, 3, 0x7C, 4, &mempll);
		mem_m = (mempll&0x0F);
		mem_n = ((mempll >> 4) & 0x0F);
	}

	/* Compute the final FSB Clock */
	dramclock = ((extclock /1000) / coef) * (mem_n/mem_m);
	fsb = ((extclock /1000) / coef);

	/* ...and print */
	print_cpu_line(dramclock, fsb, 1);

}

static void poll_fsb_us15w(void) {

	double dramclock, dramratio, fsb;
	unsigned long msr;

	/* Find dramratio */
	/* D0 MsgRd, 05 Zunit, 03 MSR */
	pci_conf_write(0, 0, 0, 0xD0, 4, 0xD0050300 );		
	pci_conf_read(0, 0, 0, 0xD4, 4, &msr );		
	fsb = ( msr >> 3 ) & 1;

	dramratio = 0.5; 

	// Compute RAM Frequency
	if (( msr >> 3 ) & 1) {
		fsb = 533;
	} else {
		fsb = 400;
	}
	
/*
	switch (( msr >> 0 ) & 7) {
		case 0:
			gfx = 100;
			break;
		case 1:
			gfx = 133;
			break;
		case 2:
			gfx = 150;
			break;
		case 3:
			gfx = 178;
			break;
		case 4:
			gfx = 200;
			break;
		case 5:
			gfx = 266;
			break;
		default:
			gfx = 0;
			break;
	}	
	*/
	
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 1);

}

static void poll_fsb_nhm(void) {

	double dramclock, dramratio, fsb;
	unsigned long mc_dimm_clk_ratio;
	float coef = getNHMmultiplier();
	//unsigned long qpi_pll_status;
	//float qpi_speed;


	fsb = ((extclock /1000) / coef);
	
	/* Print QPI Speed (if ECC not supported) */
	/*
	if(ctrl.mode == ECC_NONE && cpu_id.vers.bits.model == 10) {
		pci_conf_read(nhm_bus, 2, 1, 0x50, 2, &qpi_pll_status);
		qpi_speed = (qpi_pll_status & 0x7F) * ((extclock / 1000) / coef) * 2;
		cprint(LINE_CPU+5, col +1, "/ QPI : ");
		col += 9;
		dprint(LINE_CPU+5, col, qpi_speed/1000, 1,0);
		col += 1;
		cprint(LINE_CPU+5, col, ".");
		col += 1;		
		qpi_speed = ((qpi_speed / 1000) - (int)(qpi_speed / 1000)) * 10;
		dprint(LINE_CPU+5, col, qpi_speed, 1,0);
		col += 1;		
		cprint(LINE_CPU+5, col +1, "GT/s");
		col += 5;	
	}
	*/
	
	/* Get the clock ratio */
	
	pci_conf_read(nhm_bus, 3, 4, 0x54, 2, &mc_dimm_clk_ratio);
	dramratio = (mc_dimm_clk_ratio & 0x1F);
	
	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio / 2;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_nhm32(void) {

	double dramclock, dramratio, fsb;
	unsigned long mc_dimm_clk_ratio;
	float coef = getNHMmultiplier();
	//unsigned long qpi_pll_status;
	//float qpi_speed;

	fsb = ((extclock /1000) / coef);

	/* Print QPI Speed (if ECC not supported) */
	/*
	if(ctrl.mode == ECC_NONE && cpu_id.vers.bits.model == 12) {
		pci_conf_read(nhm_bus, 2, 1, 0x50, 2, &qpi_pll_status);
		qpi_speed = (qpi_pll_status & 0x7F) * ((extclock / 1000) / coef) * 2;
		cprint(LINE_CPU+5, col +1, "/ QPI : ");
		col += 9;
		dprint(LINE_CPU+5, col, qpi_speed/1000, 1,0);
		col += 1;
		cprint(LINE_CPU+5, col, ".");
		col += 1;		
		qpi_speed = ((qpi_speed / 1000) - (int)(qpi_speed / 1000)) * 10;
		dprint(LINE_CPU+5, col, qpi_speed, 1,0);
		col += 1;		
		cprint(LINE_CPU+5, col +1, "GT/s");
		col += 5;	
	}
	*/
	
	/* Get the clock ratio */
	
	pci_conf_read(nhm_bus, 3, 4, 0x50, 2, &mc_dimm_clk_ratio);
	dramratio = (mc_dimm_clk_ratio & 0x1F);
	
	// Compute RAM Frequency
	fsb = ((extclock / 1000) / coef);
	dramclock = fsb * dramratio / 2;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_wmr(void) {

	double dramclock, dramratio, fsb;
	unsigned long dev0;
	float coef = getNHMmultiplier();
	long *ptr;
	
	fsb = ((extclock / 1000) / coef);

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0x2C20);
	dramratio = 1;
	
	/* Get the clock ratio */
	dramratio = 0.25 * (float)(*ptr & 0x1F);
	
	// Compute RAM Frequency
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);

}

static void poll_fsb_snb(void) {

	double dramclock, dramratio, fsb;
	unsigned long dev0;
	float coef = getSNBmultiplier();
	long *ptr;
	
	fsb = ((extclock / 1000) / coef);

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0x5E04);
	dramratio = 1;
	
	/* Get the clock ratio */
	dramratio = (float)(*ptr & 0x1F) * (133.34f / 100.0f);
	
	// Compute RAM Frequency
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);
	
}

static void poll_fsb_ivb(void) {

	double dramclock, dramratio, fsb;
	unsigned long dev0, mchcfg;
	float coef = getSNBmultiplier();
	long *ptr;
	
	fsb = ((extclock / 1000) / coef);

	/* Find dramratio */
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	ptr=(long*)(dev0+0x5E04);
	mchcfg = *ptr & 0xFFFF;
	dramratio = 1;
	
	/* Get the clock ratio */
	switch((mchcfg >> 8) & 0x01)
	{
		case 0x0:
			dramratio = (float)(*ptr & 0x1F) * (133.34f / 100.0f);
			break;
		case 0x1:
			dramratio = (float)(*ptr & 0x1F) * (100.0f / 100.0f);	
			break;
	}
	
	// Compute RAM Frequency
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);
	
}

static void poll_fsb_snbe(void) {

	double dramclock, dramratio, fsb;
	unsigned long dev0;
	float coef = getSNBmultiplier();
	
	fsb = ((extclock / 1000) / coef);

	/* Find dramratio */
	pci_conf_read( 0xFF, 10, 1, 0x98, 4, &dev0);
	dev0 &= 0xFFFFFFFF;
	dramratio = 1;
	
	/* Get the clock ratio */
	dramratio = (float)(dev0 & 0x3F) * (66.67f / 100.0f);
	
	// Compute RAM Frequency
	dramclock = fsb * dramratio;

	// Print DRAM Freq
	print_cpu_line(dramclock, fsb, 3);
	
	

}

/* ------------------ Here the code for Timings detection ------------------ */
/* ------------------------------------------------------------------------- */

static void poll_timings_nf4ie(void) {


	ulong regd0, reg8c, reg9c, reg80;
	int cas, rcd, rp, ras, chan;

	//Now, read Registers
	pci_conf_read( 0, 1, 1, 0xD0, 4, &regd0);
	pci_conf_read( 0, 1, 1, 0x80, 1, &reg80);
	pci_conf_read( 0, 1, 0, 0x8C, 4, &reg8c);
	pci_conf_read( 0, 1, 0, 0x9C, 4, &reg9c);

	// Then, detect timings
	cas = (regd0 >> 4) & 0x7;
	rcd = (reg8c >> 24) & 0xF;
	rp = (reg9c >> 8) & 0xF;
	ras = (reg8c >> 16) & 0x3F;
	
	if (reg80 & 0x3) {
		chan = 2;
	} else {
		chan = 1;
	}
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_i875(void) {

	ulong dev6, dev62;
	ulong temp;
	float cas;
	int rcd, rp, ras, chan;
	long *ptr, *ptr2;

	pci_conf_read( 0, 6, 0, 0x40, 4, &dev62);
	ptr2=(long*)(dev6+0x68);

	/* Read the MMR Base Address & Define the pointer */
	pci_conf_read( 0, 6, 0, 0x10, 4, &dev6);

	/* Now, we could check some additionnals timings infos) */
	ptr=(long*)(dev6+0x60);
	// CAS Latency (tCAS)
	temp = ((*ptr >> 5)& 0x3);
	if (temp == 0x0) { cas = 2.5; } else if (temp == 0x1) { cas = 2; } else { cas = 3; }

	// RAS-To-CAS (tRCD)
	temp = ((*ptr >> 2)& 0x3);
	if (temp == 0x0) { rcd = 4; } else if (temp == 0x1) { rcd = 3; } else { rcd = 2; }

	// RAS Precharge (tRP)
	temp = (*ptr&0x3);
	if (temp == 0x0) { rp = 4; } else if (temp == 0x1) { rp = 3; } else { rp = 2; }

	// RAS Active to precharge (tRAS)
	temp = ((*ptr >> 7)& 0x7);
	ras = 10 - temp;

	// Print 64 or 128 bits mode
	if (((*ptr2 >> 21)&3) > 0) { 
		chan = 2;
	} else {
		chan = 1;
	}
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_i925(void) {

	// Thanks for CDH optis
	float cas;
	int rcd,rp,ras,chan;
	ulong dev0, drt, drc, dcc, idetect, temp;
	long *ptr;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x44, 4, &dev0);
	pci_conf_read( 0, 0, 0, 0x02, 2, &idetect);
	dev0 &= 0xFFFFC000;

	//Set pointer for DRT
	ptr=(long*)(dev0+0x114);
	drt = *ptr & 0xFFFFFFFF;

	//Set pointer for DRC
	ptr=(long*)(dev0+0x120);
	drc = *ptr & 0xFFFFFFFF;

	//Set pointer for DCC
	ptr=(long*)(dev0+0x200);
	dcc = *ptr & 0xFFFFFFFF;

	// CAS Latency (tCAS)
	temp = ((drt >> 8)& 0x3);

	if ((drc & 3) == 2){
		// Timings DDR-II
		if      (temp == 0x0) { cas = 5; }
		else if (temp == 0x1) { cas = 4; }
		else if (temp == 0x2) { cas = 3; }
		else		      { cas = 6; }
	} else {
		// Timings DDR-I
		if      (temp == 0x0) { cas = 3; }
		else if (temp == 0x1) { cas = 2.5f;}
		else		      { cas = 2; }
	}

	// RAS-To-CAS (tRCD)
	rcd = ((drt >> 4)& 0x3)+2;

	// RAS Precharge (tRP)
	rp = (drt&0x3)+2;

	// RAS Active to precharge (tRAS)
	// If Lakeport, than change tRAS computation (Thanks to CDH, again)
	if (idetect > 0x2700)
		ras = ((drt >> 19)& 0x1F);
	else
		ras = ((drt >> 20)& 0x0F);


	temp = (dcc&0x3);
	if      (temp == 1) { chan = 2; }
	else if (temp == 2) { chan = 2; }
	else		    { chan = 1; }
		
	print_ram_line(cas, rcd, rp, ras, chan);

}

static void poll_timings_i965(void) {

	// Thanks for CDH optis
	ulong dev0, c0ckectrl, c1ckectrl, offset;
	ulong ODT_Control_Register, Precharge_Register, ACT_Register, Read_Register;
	long *ptr;
	int rcd,rp,ras,chan;
	float cas;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;

	ptr = (long*)(dev0+0x260);
	c0ckectrl = *ptr & 0xFFFFFFFF;	

	ptr = (long*)(dev0+0x660);
	c1ckectrl = *ptr & 0xFFFFFFFF;
	
	// If DIMM 0 not populated, check DIMM 1
	((c0ckectrl) >> 20 & 0xF)?(offset = 0):(offset = 0x400);

	ptr = (long*)(dev0+offset+0x29C);
	ODT_Control_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x250);	
	Precharge_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x252);
	ACT_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x258);
	Read_Register = *ptr & 0xFFFFFFFF;


	// CAS Latency (tCAS)
	cas = ((ODT_Control_Register >> 17)& 7) + 3.0f;

	// RAS-To-CAS (tRCD)
	rcd = (Read_Register >> 16) & 0xF;

	// RAS Precharge (tRP)
	rp = (ACT_Register >> 13) & 0xF;

	// RAS Active to precharge (tRAS)
	ras = (Precharge_Register >> 11) & 0x1F;

	if ((c0ckectrl >> 20 & 0xF) && (c1ckectrl >> 20 & 0xF)) { 
		chan = 2; 
	}	else {
		chan = 1; 
	}

	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_im965(void) {

	// Thanks for CDH optis
	ulong dev0, c0ckectrl, c1ckectrl, offset;
	ulong ODT_Control_Register, Precharge_Register;
	long *ptr;
	int rcd,rp,ras,chan;
	float cas;
	
	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;

	ptr = (long*)(dev0+0x1200);
	c0ckectrl = *ptr & 0xFFFFFFFF;	

	ptr = (long*)(dev0+0x1300);
	c1ckectrl = *ptr & 0xFFFFFFFF;
	
	// If DIMM 0 not populated, check DIMM 1
	((c0ckectrl) >> 20 & 0xF)?(offset = 0):(offset = 0x100);

	ptr = (long*)(dev0+offset+0x121C);
	ODT_Control_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x1214);	
	Precharge_Register = *ptr & 0xFFFFFFFF;

	// CAS Latency (tCAS)
	cas = ((ODT_Control_Register >> 23)& 7) + 3.0f;

	// RAS-To-CAS (tRCD)
	rcd = ((Precharge_Register >> 5)& 7) + 2.0f;

	// RAS Precharge (tRP)
	rp = (Precharge_Register & 7) + 2.0f;

	// RAS Active to precharge (tRAS)
	ras = (Precharge_Register >> 21) & 0x1F;


	if ((c0ckectrl >> 20 & 0xF) && (c1ckectrl >> 20 & 0xF)) { 
		chan = 2;
	}	else {
		chan = 1;
	}
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_p35(void) {

	// Thanks for CDH optis
	float cas;
	int rcd, rp, ras, chan;
	ulong dev0, Device_ID, c0ckectrl, c1ckectrl, offset;
	ulong ODT_Control_Register, Precharge_Register, ACT_Register, Read_Register;
	long *ptr;
	
	pci_conf_read( 0, 0, 0, 0x02, 2, &Device_ID);
	Device_ID &= 0xFFFF;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;

	ptr = (long*)(dev0+0x260);
	c0ckectrl = *ptr & 0xFFFFFFFF;	

	ptr = (long*)(dev0+0x660);
	c1ckectrl = *ptr & 0xFFFFFFFF;
	
	// If DIMM 0 not populated, check DIMM 1
	((c0ckectrl) >> 20 & 0xF)?(offset = 0):(offset = 0x400);

	ptr = (long*)(dev0+offset+0x265);
	ODT_Control_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x25D);	
	Precharge_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x252);
	ACT_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x258);
	Read_Register = *ptr & 0xFFFFFFFF;


	// CAS Latency (tCAS)
	if(Device_ID > 0x2E00 && imc_type != 8) {
		cas = ((ODT_Control_Register >> 8)& 0x3F) - 6.0f;
	} else {
		cas = ((ODT_Control_Register >> 8)& 0x3F) - 9.0f;
	}

	// RAS-To-CAS (tRCD)
	rcd = (Read_Register >> 17) & 0xF;

	// RAS Precharge (tRP)
	rp = (ACT_Register >> 13) & 0xF;

	// RAS Active to precharge (tRAS)
	ras = Precharge_Register & 0x3F;
	
	if ((c0ckectrl >> 20 & 0xF) && (c1ckectrl >> 20 & 0xF)) { 
		chan = 2; 
	}	else {
		chan = 1; 
	}
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_wmr(void) {

	float cas;
	int rcd, rp, ras, chan;
	ulong dev0, c0ckectrl, c1ckectrl, offset;
	ulong ODT_Control_Register, Precharge_Register, ACT_Register, Read_Register, MRC_Register;
	long *ptr;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;

	ptr = (long*)(dev0+0x260);
	c0ckectrl = *ptr & 0xFFFFFFFF;	

	ptr = (long*)(dev0+0x660);
	c1ckectrl = *ptr & 0xFFFFFFFF;
	
	// If DIMM 0 not populated, check DIMM 1
	((c0ckectrl) >> 20 & 0xF)?(offset = 0):(offset = 0x400);

	ptr = (long*)(dev0+offset+0x265);
	ODT_Control_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x25D);	
	Precharge_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x252);
	ACT_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x258);
	Read_Register = *ptr & 0xFFFFFFFF;

	ptr = (long*)(dev0+offset+0x240);
	MRC_Register = *ptr & 0xFFFFFFFF;

	// CAS Latency (tCAS)
	if(MRC_Register & 0xF) {
		cas = (MRC_Register & 0xF) + 3.0f;
	} else {
		cas = ((ODT_Control_Register >> 8)& 0x3F) - 5.0f;
	}

	// RAS-To-CAS (tRCD)
	rcd = (Read_Register >> 17) & 0xF;

	// RAS Precharge (tRP)
	rp = (ACT_Register >> 13) & 0xF;

	// RAS Active to precharge (tRAS)
	ras = Precharge_Register & 0x3F;
	
	if ((c0ckectrl >> 20 & 0xF) && (c1ckectrl >> 20 & 0xF)) { 
		chan = 2; 
	}	else {
		chan = 1; 
	}

	print_ram_line(cas, rcd, rp, ras, chan);

}

static void poll_timings_snb(void) {

	float cas;
	int rcd, rp, ras, chan;
	ulong dev0, offset;
	ulong IMC_Register, MCMain0_Register, MCMain1_Register;
	long *ptr;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;
	
	offset = 0x0000;

	ptr = (long*)(dev0+offset+0x4000);
	IMC_Register = *ptr & 0xFFFFFFFF;

	// CAS Latency (tCAS)
	cas = (float)((IMC_Register >> 8) & 0x0F);

	// RAS-To-CAS (tRCD)
	rcd = IMC_Register & 0x0F;

	// RAS Precharge (tRP)
	rp = (IMC_Register >> 4) & 0x0F;

	// RAS Active to precharge (tRAS)
	ras = (IMC_Register >> 16) & 0xFF;
	
	// Channels
	ptr = (long*)(dev0+offset+0x5004);
	MCMain0_Register = *ptr & 0xFFFF;
	ptr = (long*)(dev0+offset+0x5008);
	MCMain1_Register = *ptr & 0xFFFF;
	
	if(MCMain0_Register == 0 || MCMain1_Register == 0) {
		chan = 1; 
	} else {
		chan = 2; 
	}
	
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_hsw(void) {

	float cas;
	int rcd, rp, ras, chan;
	ulong dev0, offset = 0;
	ulong IMC_Register, MCMain0_Register, MCMain1_Register;
	long *ptr;

	//Now, read MMR Base Address
	pci_conf_read( 0, 0, 0, 0x48, 4, &dev0);
	dev0 &= 0xFFFFC000;

	// Channels
	ptr = (long*)(dev0+offset+0x5004);
	MCMain0_Register = *ptr & 0xFFFF;
	
	ptr = (long*)(dev0+offset+0x5008);
	MCMain1_Register = *ptr & 0xFFFF;
	
	if(MCMain0_Register && MCMain1_Register) {
		chan = 2; 
	} else {
		chan = 1;
	}
	
	if(MCMain0_Register) { offset = 0x0000; } else {	offset = 0x0400; }
	
	// CAS Latency (tCAS)
	ptr = (long*)(dev0+offset+0x4014);
	IMC_Register = *ptr & 0xFFFFFFFF;
	cas = (float)(IMC_Register & 0x1F);

	ptr = (long*)(dev0+offset+0x4000);
	IMC_Register = *ptr & 0xFFFFFFFF;

	// RAS-To-CAS (tRCD)
	rcd = IMC_Register & 0x1F;

	// RAS Precharge (tRP)
	rp = (IMC_Register >> 5) & 0x1F;

	// RAS Active to precharge (tRAS)
	ras = (IMC_Register >> 10) & 0x3F;
	

	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_snbe(void) {

	float cas;
	int rcd, rp, ras;
	int nb_channel = 0, current_channel = 0;
	ulong temp, IMC_Register;
	long *ptr;
	
	//Read Channel #1
	pci_conf_read(0xFF, 16, 2, 0x80, 4, &temp);
	temp &= 0x3F;
	if(temp != 0xB) { current_channel = 0; nb_channel++; }

	//Read Channel #2
	pci_conf_read(0xFF, 16, 3, 0x80, 4, &temp);
	temp &= 0x3F;
	if(temp != 0xB) { current_channel = 1; nb_channel++; }

	//Read Channel #3
	pci_conf_read(0xFF, 16, 6, 0x80, 4, &temp);
	temp &= 0x3F;
	if(temp != 0xB) { current_channel = 4; nb_channel++; }
	
	//Read Channel #4
	pci_conf_read(0xFF, 16, 7, 0x80, 4, &temp);
	temp &= 0x3F;
	if(temp != 0xB) { current_channel = 5; nb_channel++; }


	pci_conf_read(0, 5, 0, 0x84, 4, &temp);
	ptr = (long*)((temp & 0xFC000000) + (MAKE_PCIE_ADDRESS(0xFF,16,current_channel) | 0x200));
	IMC_Register = *ptr & 0xFFFFFFFF;

	// CAS Latency (tCAS)
	cas = (float)((IMC_Register  >> 9) & 0x1F);

	// RAS-To-CAS (tRCD)
	rcd = IMC_Register & 0x1F;

	// RAS Precharge (tRP)
	rp = (IMC_Register >> 5) & 0x0F;

	// RAS Active to precharge (tRAS)
	ras = (IMC_Register >> 19) & 0x3F;
	

	print_ram_line(cas, rcd, rp, ras, nb_channel);

}

static void poll_timings_5400(void) {

	// Thanks for CDH optis
	ulong ambase, mtr1, mtr2, offset, mca;
	long *ptr;
	float cas;
	int rcd, rp, ras, chan;
	
	//Hard-coded Ambase value (should not be realocated by software when using Memtest86+
	ambase = 0xFE000000;
  offset = mtr1 = mtr2 = 0;

  // Will loop until a valid populated channel is found
  // Bug  : DIMM 0 must be populated or it will fall in an endless loop  
  while(((mtr2 & 0xF) < 3) || ((mtr2 & 0xF) > 6)) {
		ptr = (long*)(ambase+0x378+offset);
		mtr1 = *ptr & 0xFFFFFFFF;
	
		ptr = (long*)(ambase+0x37C+offset);	
		mtr2 = *ptr & 0xFFFFFFFF;
		offset += 0x8000;
	}

	pci_conf_read( 0, 16, 1, 0x58, 4, &mca);

	//This chipset only supports FB-DIMM (Removed => too long)
	//cprint(LINE_CPU+5, col +1, "- Type : FBD");

	// Now, detect timings

	// CAS Latency (tCAS)
	cas = mtr2 & 0xF;

	// RAS-To-CAS (tRCD)
	rcd = 6 - ((mtr1 >> 10) & 3);

	// RAS Precharge (tRP)
	rp = 6 - ((mtr1 >> 8) & 3);

	// RAS Active to precharge (tRAS)
	ras = 16 - (3 * ((mtr1 >> 29) & 3)) + ((mtr1 >> 12) & 3);
  if(((mtr1 >> 12) & 3) == 3 && ((mtr1 >> 29) & 3) == 2) { ras = 9; }


	if ((mca >> 14) & 1) { 
		chan = 1; 
	}	else {
		chan = 2; 
	}

	print_ram_line(cas, rcd, rp, ras, chan);

}

static void poll_timings_E7520(void) {

	ulong drt, ddrcsr;
	float cas;
	int rcd, rp, ras, chan;

	pci_conf_read( 0, 0, 0, 0x78, 4, &drt);
	pci_conf_read( 0, 0, 0, 0x9A, 2, &ddrcsr);

	cas = ((drt >> 2) & 3) + 2;
	rcd = ((drt >> 10) & 1) + 3;
	rp = ((drt >> 9) & 1) + 3;
	ras = ((drt >> 14) & 3) + 11;
	
	if ((ddrcsr & 0xF) >= 0xC) {
		chan = 2;
	} else {
		chan = 1;
	}
	
	print_ram_line(cas, rcd, rp, ras, chan);
}


static void poll_timings_i855(void) {

	ulong drt, temp;
	float cas;
	int rcd, rp, ras;
	
	pci_conf_read( 0, 0, 0, 0x78, 4, &drt);

	/* Now, we could print some additionnals timings infos) */
	cprint(LINE_CPU+6, col2 +1, "/ CAS : ");
	col2 += 9;

	// CAS Latency (tCAS)
	temp = ((drt >> 4)&0x1);
	if (temp == 0x0) { cas = 2.5;  }
	else { cas = 2; }

	// RAS-To-CAS (tRCD)
	temp = ((drt >> 2)& 0x1);
	if (temp == 0x0) { rcd = 3; }
	else { rcd = 2; }

	// RAS Precharge (tRP)
	temp = (drt&0x1);
	if (temp == 0x0) { rp = 3 ; }
	else { rp = 2; }

	// RAS Active to precharge (tRAS)
	temp = 7-((drt >> 9)& 0x3);
	if (temp == 0x0) { ras = 7; }
	if (temp == 0x1) { ras = 6; }
	if (temp == 0x2) { ras = 5; }
	
	print_ram_line(cas, rcd, rp, ras, 1);

}

static void poll_timings_E750x(void) {

	ulong drt, drc, temp;
	float cas;
	int rcd, rp, ras, chan;

	pci_conf_read( 0, 0, 0, 0x78, 4, &drt);
	pci_conf_read( 0, 0, 0, 0x7C, 4, &drc);

	if ((drt >> 4) & 1) { cas = 2; } else { cas = 2.5; };
	if ((drt >> 1) & 1) { rcd = 2; } else { rcd = 3; };
	if (drt & 1) { rp = 2; } else { rp = 3; };

	temp = ((drt >> 9) & 3);
	if (temp == 2) { ras = 5; } else if (temp == 1) { ras = 6; } else { ras = 7; }

	if (((drc >> 22)&1) == 1) {
		chan = 2;
	} else {
		chan = 1;
	}

	print_ram_line(cas, rcd, rp, ras, chan);

}

static void poll_timings_i852(void) {

	ulong drt, temp;
	float cas;
	int rcd, rp, ras;
	
	pci_conf_read( 0, 0, 1, 0x60, 4, &drt);

	/* Now, we could print some additionnals timings infos) */
	cprint(LINE_CPU+6, col2 +1, "/ CAS : ");
	col2 += 9;

	// CAS Latency (tCAS)
	temp = ((drt >> 5)&0x1);
	if (temp == 0x0) { cas = 2.5;  }
	else { cas = 2; }

	// RAS-To-CAS (tRCD)
	temp = ((drt >> 2)& 0x3);
	if (temp == 0x0) { rcd = 4; }
	if (temp == 0x1) { rcd = 3; }
	else { rcd = 2; }

	// RAS Precharge (tRP)
	temp = (drt&0x3);
	if (temp == 0x0) { rp = 4; }
	if (temp == 0x1) { rp = 3; }
	else { rp = 2; }

	// RAS Active to precharge (tRAS)
	temp = ((drt >> 9)& 0x3);
	if (temp == 0x0) { ras = 8; }
	if (temp == 0x1) { ras = 7; }
	if (temp == 0x2) { ras = 6; }
	if (temp == 0x3) { ras = 5; }

	print_ram_line(cas, rcd, rp, ras, 1);

}

static void poll_timings_amd64(void) {

	ulong dramtlr, dramclr;
	int temp, chan;
	float tcas;
	int trcd, trp, tras ;

	pci_conf_read(0, 24, 2, 0x88, 4, &dramtlr);
	pci_conf_read(0, 24, 2, 0x90, 4, &dramclr);
	
	if (cpu_id.vers.bits.extendedModel >= 4) {
		/* NEW K8 0Fh Family 90 nm (DDR2) */

			// CAS Latency (tCAS)
			tcas = (dramtlr & 0x7) + 1;
		
			// RAS-To-CAS (tRCD)
			trcd = ((dramtlr >> 4) & 0x3) + 3;
		
			// RAS Precharge (tRP)
			trp = ((dramtlr >> 8) & 0x3) + 3;
		
			// RAS Active to precharge (tRAS)
			tras = ((dramtlr >> 12) & 0xF) + 3;
		
			// Print 64 or 128 bits mode
			if ((dramclr >> 11)&1) {
				chan = 2;
			} else {
				chan = 1;
			}

	} else {
		/* OLD K8 (DDR1) */

			// CAS Latency (tCAS)
			temp = (dramtlr & 0x7);
			if (temp == 0x1) { tcas = 2; }
			if (temp == 0x2) { tcas = 3; }
			if (temp == 0x5) { tcas = 2.5; }
		
			// RAS-To-CAS (tRCD)
			trcd = ((dramtlr >> 12) & 0x7);
		
			// RAS Precharge (tRP)
			trp = ((dramtlr >> 24) & 0x7);
		
			// RAS Active to precharge (tRAS)
			tras = ((dramtlr >> 20) & 0xF);
		
			// Print 64 or 128 bits mode
			if (((dramclr >> 16)&1) == 1) {
				chan = 2;
			} else {
				chan = 1;
			}
	}
	
	print_ram_line(tcas, trcd, trp, tras, chan);
	
}

static void poll_timings_k10(void) {

	ulong dramtlr, dramclr, dramchr, dramchrb;
	ulong offset = 0;
	int cas, rcd, rp, ras, chan;
	
	pci_conf_read(0, 24, 2, 0x94, 4, &dramchr);
	pci_conf_read(0, 24, 2, 0x194, 4, &dramchrb);
	
	if(((dramchr>>14) & 0x1) || ((dramchr>>14) & 0x1)) { chan = 1; } else { chan = 2; }
	
	// If Channel A not enabled, switch to channel B
	if(((dramchr>>14) & 0x1))
	{
		offset = 0x100;
		pci_conf_read(0, 24, 2, 0x94+offset, 4, &dramchr);	
	}

	pci_conf_read(0, 24, 2, 0x88+offset, 4, &dramtlr);
	pci_conf_read(0, 24, 2, 0x110, 4, &dramclr);
	
	// CAS Latency (tCAS)
	if(((dramchr >> 8)&1) || ((dramchr & 0x7) == 0x4)){
		// DDR3 or DDR2-1066
		cas = (dramtlr & 0xF) + 4;
		rcd = ((dramtlr >> 4) & 0x7) + 5;
		rp = ((dramtlr >> 7) & 0x7) + 5;
	  ras = ((dramtlr >> 12) & 0xF) + 15;	
	} else {
	// DDR2-800 or less
		cas = (dramtlr & 0xF) + 1;
		rcd = ((dramtlr >> 4) & 0x3) + 3;
		rp = ((dramtlr >> 8) & 0x3) + 3;
	  ras = ((dramtlr >> 12) & 0xF) + 3;
	}
		
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_k12(void) {

	ulong dramt0, dramlow, dimma, dimmb;
	int cas, rcd, rp, ras, chan = 0;
	
	pci_conf_read(0, 24, 2, 0x94, 4, &dimma);
	pci_conf_read(0, 24, 2, 0x194, 4, &dimmb);

	if(((dimma >> 14) & 0x1) == 0) 
	{ 
		chan++; 
		pci_conf_read(0, 24, 2, 0x88, 4, &dramlow); 
		pci_conf_write(0, 24, 2, 0xF0, 4, 0x00000040);
		pci_conf_read(0, 24, 2, 0xF4, 4, &dramt0);	
	}
	
	if(((dimmb >> 14) & 0x1) == 0) 
	{ 
		chan++; 
		pci_conf_read(0, 24, 2, 0x188, 4, &dramlow); 
		pci_conf_write(0, 24, 2, 0x1F0, 4, 0x00000040);
		pci_conf_read(0, 24, 2, 0x1F4, 4, &dramt0);	
	}

	cas = (dramlow & 0xF) + 4;
	rcd = (dramt0 & 0xF) + 5;
	rp = ((dramt0 >> 8) & 0xF) + 5;
  ras = ((dramt0 >> 16) & 0x1F) + 15;
	
	print_ram_line(cas, rcd, rp, ras, chan);
}


static void poll_timings_k14(void) {

	ulong dramt0, dramlow;
	int cas, rcd, rp, ras;
	
	pci_conf_read(0, 24, 2, 0x88, 4, &dramlow); 
	pci_conf_write(0, 24, 2, 0xF0, 4, 0x00000040);
	pci_conf_read(0, 24, 2, 0xF4, 4, &dramt0);	

	cas = (dramlow & 0xF) + 4;
	rcd = (dramt0 & 0xF) + 5;
	rp = ((dramt0 >> 8) & 0xF) + 5;
  ras = ((dramt0 >> 16) & 0x1F) + 15;
	
	print_ram_line(cas, rcd, rp, ras, 1);
}

static void poll_timings_k15(void) {

	ulong dramp1, dramp2, dimma, dimmb;
	int cas, rcd, rp, ras, chan = 0;
	
	pci_conf_read(0, 24, 2, 0x94, 4, &dimma);
	pci_conf_read(0, 24, 2, 0x194, 4, &dimmb);
	if(((dimma>>14) & 0x1) || ((dimmb>>14) & 0x1)) { chan = 1; } else { chan = 2; }
		
	pci_conf_read(0, 24, 2, 0x200, 4, &dramp1); 
	pci_conf_read(0, 24, 2, 0x204, 4, &dramp2); 
	
	cas = dramp1 & 0x1F;
	rcd = (dramp1 >> 8) & 0x1F;
	rp = (dramp1 >> 16) & 0x1F;
  ras = (dramp1 >> 24) & 0x3F;
	
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_k16(void) {

	ulong dramt0, dramt1;
	int cas, rcd, rp, rc, ras;
	
	pci_conf_read(0, 24, 2, 0x200, 4, &dramt0);
	pci_conf_read(0, 24, 2, 0x204, 4, &dramt1);	

	cas = (dramt0 & 0x1F);
	rcd = ((dramt0 >> 8) & 0x1F);
	rp = ((dramt0 >> 16) & 0x1F);
	ras = ((dramt0 >> 24) & 0x3F);
	
	rc = (dramt1 & 0x3F);	
	
	print_ram_line(cas, rcd, rp, ras, 1);
}

static void poll_timings_EP80579(void) {

	ulong drt1, drt2;
	float cas;
	int rcd, rp, ras;

	pci_conf_read( 0, 0, 0, 0x78, 4, &drt1);
	pci_conf_read( 0, 0, 0, 0x64, 4, &drt2);

	cas = ((drt1 >> 3) & 0x7) + 3;
	rcd = ((drt1 >> 9) & 0x7) + 3;
	rp = ((drt1 >> 6) & 0x7) + 3;
	ras = ((drt2 >> 28) & 0xF) + 8;

	print_ram_line(cas, rcd, rp, ras, 0);
}

static void poll_timings_nf2(void) {

	ulong dramtlr, dramtlr2, dramtlr3, temp;
	ulong dimm1p, dimm2p, dimm3p;
	float cas;
	int rcd, rp, ras, chan;
	
	pci_conf_read(0, 0, 1, 0x90, 4, &dramtlr);
	pci_conf_read(0, 0, 1, 0xA0, 4, &dramtlr2);
	pci_conf_read(0, 0, 1, 0x84, 4, &dramtlr3);
	pci_conf_read(0, 0, 2, 0x40, 4, &dimm1p);
	pci_conf_read(0, 0, 2, 0x44, 4, &dimm2p);
	pci_conf_read(0, 0, 2, 0x48, 4, &dimm3p);

	// CAS Latency (tCAS)
	temp = ((dramtlr2 >> 4) & 0x7);
	if (temp == 0x2) { cas = 2; }
	if (temp == 0x3) { cas = 3; }
	if (temp == 0x6) { cas = 2.5; }
		
	// RAS-To-CAS (tRCD)
	rcd = ((dramtlr >> 20) & 0xF);

	// RAS Precharge (tRP)
	rp = ((dramtlr >> 28) & 0xF);

	// RAS Active to precharge (tRAS)
	ras = ((dramtlr >> 15) & 0xF);

	// Print 64 or 128 bits mode
	// If DIMM1 & DIMM3 or DIMM1 & DIMM2 populated, than Dual Channel.

	if ((dimm3p&1) + (dimm2p&1) == 2 || (dimm3p&1) + (dimm1p&1) == 2 ) {
		chan = 2;
	} else {
		chan = 1;
	}
	print_ram_line(cas, rcd, rp, ras, chan);
}

static void poll_timings_us15w(void) {

	// Thanks for CDH optis
	ulong dtr;
	float cas;
	int rcd, rp;
	
	/* Find dramratio */
	/* D0 MsgRd, 01 Dunit, 01 DTR */
	pci_conf_write(0, 0, 0, 0xD0, 4, 0xD0010100 );		
	pci_conf_read(0, 0, 0, 0xD4, 4, &dtr );		

	// CAS Latency (tCAS)
	cas = ((dtr >> 4) & 0x3) + 3;

	// RAS-To-CAS (tRCD)
	rcd = ((dtr >> 2) & 0x3) + 3;

	// RAS Precharge (tRP)
	rp = ((dtr >> 0) & 0x3) + 3;
	
	print_ram_line(cas, rcd, rp, 9, 1);

}

static void poll_timings_nhm(void) {

	ulong mc_channel_bank_timing, mc_control, mc_channel_mrs_value;
	float cas; 
	int rcd, rp, ras, chan;
	int fvc_bn = 4;

	/* Find which channels are populated */
	pci_conf_read(nhm_bus, 3, 0, 0x48, 2, &mc_control);		
	mc_control = (mc_control >> 8) & 0x7;
	
	/* Get the first valid channel */
	if(mc_control & 1) { 
		fvc_bn = 4; 
	} else if(mc_control & 2) { 
		fvc_bn = 5; 
	}	else if(mc_control & 4) { 
		fvc_bn = 6; 
	}

	// Now, detect timings
	// CAS Latency (tCAS) / RAS-To-CAS (tRCD) / RAS Precharge (tRP) / RAS Active to precharge (tRAS)
	pci_conf_read(nhm_bus, fvc_bn, 0, 0x88, 4, &mc_channel_bank_timing);	
	pci_conf_read(nhm_bus, fvc_bn, 0, 0x70, 4, &mc_channel_mrs_value);	
	cas = ((mc_channel_mrs_value >> 4) & 0xF ) + 4.0f;
	rcd = (mc_channel_bank_timing >> 9) & 0xF; 
	ras = (mc_channel_bank_timing >> 4) & 0x1F; 
	rp = mc_channel_bank_timing & 0xF;

	// Print 1, 2 or 3 Channels
	if (mc_control == 1 || mc_control == 2 || mc_control == 4 ) {
		chan = 1;
	} else if (mc_control == 7) {
		chan = 3;
	} else {
		chan = 2;	
	}
	print_ram_line(cas, rcd, rp, ras, chan);
	
}

static void poll_timings_ct(void) 
{

	unsigned long mcr,mdr;
	float cas; 
	int rcd, rp, ras;
	
	/* Build the MCR Message*/
	mcr = (0x10 << 24); // 10h = Read - 11h = Write
	mcr += (0x01 << 16); // DRAM Registers located on port 01h
	mcr += (0x01 << 8); // DRP = 00h, DTR0 = 01h, DTR1 = 02h, DTR2 = 03h
	mcr &= 0xFFFFFFF0; // bit 03:00 RSVD
	
	/* Send Message to GMCH */
	pci_conf_write(0, 0, 0, 0xD0, 4, mcr);	
	
	/* Read Answer from Sideband bus */
	pci_conf_read(0, 0, 0, 0xD4, 4, &mdr);			

	// CAS Latency (tCAS)
	cas = ((mdr >> 12)& 0x7) + 5.0f;

	// RAS-To-CAS (tRCD)
	rcd = ((mdr >> 8)& 0x7) + 5;

	// RAS Precharge (tRP)
	rp = ((mdr >> 4)& 0x7) + 5;

	// RAS is in DTR1. Read Again.
	mcr = 0x10010200; // Quick Mode ! Awesome !
	pci_conf_write(0, 0, 0, 0xD0, 4, mcr);	
	pci_conf_read(0, 0, 0, 0xD4, 4, &mdr);	
			
	// RAS Active to precharge (tRAS)		
	ras = (mdr >> 20) & 0xF;

	// Print
	print_ram_line(cas, rcd, rp, ras, 1);

}

/* ------------------ Let's continue ------------------ */
/* ---------------------------------------------------- */

struct pci_memory_controller controllers[] = {
	/* Default unknown chipset */
	{ 0, 0, "","",                    0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },

	/* AMD */
	{ 0x1022, 0x7006, "AMD 751","SDRAM PC-100",   0, poll_fsb_nothing, poll_timings_nothing, setup_amd751, poll_nothing },
	{ 0x1022, 0x700c, "AMD 762","DDR-SDRAM", 		  0, poll_fsb_nothing, poll_timings_nothing, setup_amd76x, poll_nothing },
	{ 0x1022, 0x700e, "AMD 761","DDR-SDRAM", 		  0, poll_fsb_nothing, poll_timings_nothing, setup_amd76x, poll_nothing },

	/* SiS */
	{ 0x1039, 0x0600, "SiS 600","EDO/SDRAM",   			0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0620, "SiS 620","SDRAM",   					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x5600, "SiS 5600","SDRAM",  					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0645, "SiS 645","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0646, "SiS 645DX","DDR-SDRAM", 			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0630, "SiS 630","SDRAM",   					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0650, "SiS 650","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0651, "SiS 651","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0730, "SiS 730","SDRAM",   					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0735, "SiS 735","DDR-SDRAM",   			0, poll_fsb_amd32, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0740, "SiS 740","DDR-SDRAM",   			0, poll_fsb_amd32, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0745, "SiS 745","DDR-SDRAM",   			0, poll_fsb_amd32, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0748, "SiS 748","DDR-SDRAM",   			0, poll_fsb_amd32, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0655, "SiS 655","DDR-SDRAM",				0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0656, "SiS 656","DDR/DDR2-SDRAM",		0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0648, "SiS 648","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0649, "SiS 649","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0661, "SiS 661","DDR-SDRAM",   			0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0671, "SiS 671","DDR2-SDRAM",   		0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1039, 0x0672, "SiS 672","DDR2-SDRAM",   		0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },	

	/* ALi */
	{ 0x10b9, 0x1531, "ALi Aladdin 4","EDO/SDRAM", 0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x10b9, 0x1541, "ALi Aladdin 5","EDO/SDRAM", 0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x10b9, 0x1644, "ALi Aladdin M1644","SDRAM", 0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },

	/* ATi */
	{ 0x1002, 0x5830, "ATi Radeon 9100 IGP","DDR-SDRAM", 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1002, 0x5831, "ATi Radeon 9100 IGP","DDR-SDRAM", 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1002, 0x5832, "ATi Radeon 9100 IGP","DDR-SDRAM", 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1002, 0x5833, "ATi Radeon 9100 IGP","DDR-SDRAM", 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1002, 0x5954, "ATi Xpress 200","DDR-SDRAM", 		 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1002, 0x5A41, "ATi Xpress 200","DDR-SDRAM",			 0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },

	/* nVidia */
	{ 0x10de, 0x01A4, "nVidia nForce","DDR-SDRAM", 0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x10de, 0x01E0, "nVidia nForce2 SPP","",		 0, poll_fsb_nf2, poll_timings_nf2, setup_nothing, poll_nothing },
	{ 0x10de, 0x0071, "nForce4 SLI","", 					 0, poll_fsb_nf4ie, poll_timings_nf4ie, setup_nothing, poll_nothing },

	/* VIA */
	{ 0x1106, 0x0305, "VIA KT133/KT133A","SDRAM",    			0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0391, "VIA KX133","SDRAM",    						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0501, "VIA MVP4","EDO/SDRAM",    					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0585, "VIA VP/VPX","EDO/SDRAM", 				  0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0595, "VIA VP2","EDO/SDRAM",  						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0597, "VIA VP3","EDO/SDRAM",  						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0598, "VIA MVP3","EDO/SDRAM",  						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0691, "VIA Apollo Pro 133(A)","SDRAM",  	0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0693, "VIA Apollo Pro+","SDRAM",  				0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0601, "VIA PLE133","SDRAM",  							0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3099, "VIA KT266(A)/KT333","DDR-SDRAM",		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3189, "VIA KT400(A)/600","DDR-SDRAM",			0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0269, "VIA KT880","DDR-SDRAM", 						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3205, "VIA KM400","DDR-SDRAM", 						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3116, "VIA KM266","DDR-SDRAM", 						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3156, "VIA KN266","DDR-SDRAM", 						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3123, "VIA CLE266","DDR-SDRAM", 					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x0198, "VIA PT800","DDR-SDRAM", 						0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x1106, 0x3258, "VIA PT880","DDR2-SDRAM", 					0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },

	/* Serverworks */
	{ 0x1166, 0x0008, "CNB20HE","SDRAM",   0, poll_fsb_nothing, poll_timings_nothing, setup_cnb20, poll_nothing },
	{ 0x1166, 0x0009, "CNB20LE","SDRAM",   0, poll_fsb_nothing, poll_timings_nothing, setup_cnb20, poll_nothing },

	/* Intel */
	{ 0x8086, 0x1130, "Intel i815","SDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x122d, "Intel i430FX","EDO DRAM",0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x1235, "Intel i430MX","EDO DRAM",0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x1237, "Intel i440FX","EDO DRAM",0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x1250, "Intel i430HX","EDO DRAM",0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x1A21, "Intel i840","RDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_i840, poll_nothing },
	{ 0x8086, 0x1A30, "Intel i845","SDR/DDR",		0, poll_fsb_p4, poll_timings_nothing, setup_i845, poll_nothing },
	{ 0x8086, 0x2560, "Intel i845E/G/PE/GE","",	0, poll_fsb_p4, poll_timings_nothing, setup_i845, poll_nothing },
	{ 0x8086, 0x2500, "Intel i820","RDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_i820, poll_nothing },
	{ 0x8086, 0x2530, "Intel i850","RDRAM",  		0, poll_fsb_p4, poll_timings_nothing, setup_i850, poll_nothing },
	{ 0x8086, 0x2531, "Intel i860","RDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_i860, poll_nothing },
	{ 0x8086, 0x7030, "Intel i430VX","SDRAM", 	0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7100, "Intel i430TX","SDRAM", 	0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7120, "Intel i810","SDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7122, "Intel i810","SDRAM",  		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7124, "Intel i810E","SDRAM", 		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7180, "Intel i440[LE]X","SDRAM",0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7190, "Intel i440BX","SDRAM",		0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x7192, "Intel i440BX","SDRAM", 	0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x71A0, "Intel i440GX","SDRAM", 	0, poll_fsb_nothing, poll_timings_nothing, setup_i440gx, poll_nothing },
	{ 0x8086, 0x71A2, "Intel i440GX","SDRAM",  	0, poll_fsb_nothing, poll_timings_nothing, setup_i440gx, poll_nothing },
	{ 0x8086, 0x84C5, "Intel i450GX","SDRAM", 	0, poll_fsb_nothing, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x2540, "Intel E7500","DDR-SDRAM",0, poll_fsb_p4, poll_timings_E750x, setup_iE7xxx, poll_nothing },
	{ 0x8086, 0x254C, "Intel E7501","DDR-SDRAM",0, poll_fsb_p4, poll_timings_E750x, setup_iE7xxx, poll_nothing },
	{ 0x8086, 0x255d, "Intel E7205","DDR-SDRAM",0, poll_fsb_p4, poll_timings_nothing, setup_iE7xxx, poll_nothing },
  { 0x8086, 0x3592, "Intel E7320","DDR-SDRAM",0, poll_fsb_p4, poll_timings_E7520, setup_iE7520, poll_nothing },
  { 0x8086, 0x2588, "Intel E7221","DDR-SDRAM",0, poll_fsb_i925, poll_timings_i925, setup_i925, poll_nothing },
  { 0x8086, 0x3590, "Intel E7520","DDR-SDRAM",0, poll_fsb_p4, poll_timings_E7520, setup_iE7520, poll_nothing },
  { 0x8086, 0x2600, "Intel E8500","DDR-SDRAM",0, poll_fsb_p4, poll_timings_nothing, setup_nothing, poll_nothing },
	{ 0x8086, 0x2570, "Intel i848/i865","", 		0, poll_fsb_i875, poll_timings_i875, setup_i875, poll_nothing },
	{ 0x8086, 0x2578, "Intel i875P","",     		0, poll_fsb_i875, poll_timings_i875, setup_i875, poll_nothing },
	{ 0x8086, 0x2550, "Intel E7505","DDR-SDRAM",0, poll_fsb_p4, poll_timings_nothing, setup_iE7xxx, poll_nothing },
	{ 0x8086, 0x3580, "Intel i852P/i855G","",		0, poll_fsb_i855, poll_timings_i852, setup_nothing, poll_nothing },
	{ 0x8086, 0x3340, "Intel i855PM","",    		0, poll_fsb_i855, poll_timings_i855, setup_nothing, poll_nothing },
	{ 0x8086, 0x2580, "Intel i915P/G","",   		0, poll_fsb_i925, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x2590, "Intel i915PM/GM","", 		0, poll_fsb_i925, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x2584, "Intel i925X/XE","",  		0, poll_fsb_i925, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x2770, "Intel i945P/G","", 	 		0, poll_fsb_i945, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x27A0, "Intel i945GM/PM","", 		0, poll_fsb_i945, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x27AC, "Intel i945GME","", 	 		0, poll_fsb_i945gme, poll_timings_i925, setup_i925, poll_nothing },
	{ 0x8086, 0x2774, "Intel i955X","", 		 		0, poll_fsb_i945, poll_timings_i925, setup_i925, poll_nothing},
	{ 0x8086, 0x277C, "Intel i975X","", 		 		0, poll_fsb_i975, poll_timings_i925, setup_i925, poll_nothing},
	{ 0x8086, 0x2970, "Intel i946PL/GZ","", 		0, poll_fsb_i965, poll_timings_i965, setup_p35, poll_nothing},
	{ 0x8086, 0x2990, "Intel Q963/Q965","", 		0, poll_fsb_i965, poll_timings_i965, setup_p35, poll_nothing},
	{ 0x8086, 0x29A0, "Intel P965/G965","", 		0, poll_fsb_i965, poll_timings_i965, setup_p35, poll_nothing},
	{ 0x8086, 0x2A00, "Intel GM965/GL960","", 	0, poll_fsb_im965, poll_timings_im965, setup_p35, poll_nothing},
	{ 0x8086, 0x2A10, "Intel GME965/GLE960","",	0, poll_fsb_im965, poll_timings_im965, setup_p35, poll_nothing},	
	{ 0x8086, 0x2A40, "Intel PM/GM45/47","",		0, poll_fsb_im965, poll_timings_im965, setup_p35, poll_nothing},	
	{ 0x8086, 0x29B0, "Intel Q35","", 	 		 		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x29C0, "Intel P35/G33","", 	 		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x29D0, "Intel Q33","",	  	 			0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x29E0, "Intel X38/X48","", 	 		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},			
	{ 0x8086, 0x29F0, "Intel 3200/3210","", 		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x2E10, "Intel Q45/Q43","", 	 		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x2E20, "Intel P45/G45","",	  		0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x2E30, "Intel G41","", 	 				0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},	
	{ 0x8086, 0x4001, "Intel 5400A","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},		
	{ 0x8086, 0x4003, "Intel 5400B","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},		
	{ 0x8086, 0x25D8, "Intel 5000P","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},		
	{ 0x8086, 0x25D4, "Intel 5000V","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},	
	{ 0x8086, 0x25C0, "Intel 5000X","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},		
	{ 0x8086, 0x25D0, "Intel 5000Z","", 		 		0, poll_fsb_5400, poll_timings_5400, setup_E5400, poll_nothing},	
	{ 0x8086, 0x5020, "Intel EP80579","",    		0, poll_fsb_p4, 	poll_timings_EP80579, setup_nothing, poll_nothing },
	{ 0x8086, 0x8100, "Intel US15W","",					0, poll_fsb_us15w, poll_timings_us15w, setup_nothing, poll_nothing},
	{ 0x8086, 0x8101, "Intel UL11L/US15L","", 	0, poll_fsb_us15w, poll_timings_us15w, setup_nothing, poll_nothing},

	/* INTEL IMC (Integrated Memory Controllers) */
	{ 0xFFFF, 0x0001, "Core IMC","", 	 				0, poll_fsb_nhm, 	poll_timings_nhm, setup_nhm, poll_nothing},
	{ 0xFFFF, 0x0002, "Core IMC","", 	 				0, poll_fsb_nhm32, 	poll_timings_nhm, setup_nhm32, poll_nothing},
	{ 0xFFFF, 0x0003, "Core IMC","", 	 				0, poll_fsb_wmr, 	poll_timings_wmr, setup_wmr, poll_nothing},
	{ 0xFFFF, 0x0004, "SNB IMC","", 	 				0, poll_fsb_snb, 	poll_timings_snb, setup_wmr, poll_nothing},
	{ 0xFFFF, 0x0005, "SNB-E IMC","",		 	 		0, poll_fsb_snbe, poll_timings_snbe, setup_wmr, poll_nothing},
	{ 0xFFFF, 0x0006, "IVB IMC","",			 	 		0, poll_fsb_ivb, 	poll_timings_snb, setup_wmr, poll_nothing},
	{ 0xFFFF, 0x0007, "HSW IMC","",			 	 		0, poll_fsb_ivb, 	poll_timings_hsw, setup_wmr, poll_nothing},		
	{ 0xFFFF, 0x0008, "PineView IMC","", 			0, poll_fsb_p35, poll_timings_p35, setup_p35, poll_nothing},
	{ 0xFFFF, 0x0009, "CedarTrail IMC","",		0, poll_fsb_ct, poll_timings_ct, setup_nothing, poll_nothing},
	
	/* AMD IMC (Integrated Memory Controllers) */
	{ 0xFFFF, 0x0100, "AMD K8 IMC","",				0, poll_fsb_amd64, poll_timings_amd64, setup_amd64, poll_nothing },
	{ 0xFFFF, 0x0101, "AMD K10 IMC","",			  0, poll_fsb_k10, poll_timings_k10, setup_k10, poll_nothing },
	{ 0xFFFF, 0x0102, "AMD K12 IMC","",			  0, poll_fsb_k12, poll_timings_k12, setup_apu, poll_nothing },
	{ 0xFFFF, 0x0103, "AMD K14 IMC","",				0, poll_fsb_k14, poll_timings_k14, setup_apu, poll_nothing },
	{ 0xFFFF, 0x0104, "AMD K15 IMC","",				0, poll_fsb_k15, poll_timings_k15, setup_apu, poll_nothing },
	{ 0xFFFF, 0x0105, "AMD K16 IMC","",				0, poll_fsb_k16, poll_timings_k16, setup_apu, poll_nothing }
};	

static void print_memory_controller(void)
{

	/* Print memory controller info */
	if (ctrl.index == 0) {
		return;
	}

	/* Now print the memory controller capabilities */
	/*
	cprint(LINE_CPU+5, col, " "); col++;
	if (ctrl.cap == ECC_UNKNOWN) {
		return;
	}
	if (ctrl.cap & __ECC_DETECT) {
		int on;
		on = ctrl.mode & __ECC_DETECT;
		cprint(LINE_CPU+5, col, "(ECC : ");
		cprint(LINE_CPU+5, col +7, on?"Detect":"Disabled)");
		on?(col += 13):(col += 16);
	}
	if (ctrl.mode & __ECC_CORRECT) {
		int on;
		on = ctrl.mode & __ECC_CORRECT;
		cprint(LINE_CPU+5, col, " / ");
		if (ctrl.cap & __ECC_CHIPKILL) {
		cprint(LINE_CPU+5, col +3, on?"Correct -":"");
		on?(col += 12):(col +=3);
		} else {
			cprint(LINE_CPU+5, col +3, on?"Correct)":"");
			on?(col += 11):(col +=3);
		}
	}
	if (ctrl.mode & __ECC_DETECT) {
	if (ctrl.cap & __ECC_CHIPKILL) {
		int on;
		on = ctrl.mode & __ECC_CHIPKILL;
		cprint(LINE_CPU+5, col, " Chipkill : ");
		cprint(LINE_CPU+5, col +12, on?"On)":"Off)");
		on?(col += 15):(col +=16);
	}}
	if (ctrl.mode & __ECC_SCRUB) {
		int on;
		on = ctrl.mode & __ECC_SCRUB;
		cprint(LINE_CPU+5, col, " Scrub");
		cprint(LINE_CPU+5, col +6, on?"+ ":"- ");
		col += 7;
	}
	if (ctrl.cap & __ECC_UNEXPECTED) {
		int on;
		on = ctrl.mode & __ECC_UNEXPECTED;
		cprint(LINE_CPU+5, col, "Unknown");
		cprint(LINE_CPU+5, col +7, on?"+ ":"- ");
		col += 9;
	}
	*/
	
	
	
	/* Print advanced caracteristics  */
	col2 = 0;

	controllers[ctrl.index].poll_fsb();
	controllers[ctrl.index].poll_timings();

}


void find_controller(void)
{
	unsigned long vendor;
	unsigned long device;
	int i;
	int result;
	result = pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, PCI_VENDOR_ID, 2, &vendor);
	result = pci_conf_read(ctrl.bus, ctrl.dev, ctrl.fn, PCI_DEVICE_ID, 2, &device);
	
	// Detect IMC by CPUID
	if(imc_type) { vendor = 0xFFFF; device = imc_type; }
	if(v->fail_safe & 1) { vendor = 0xFFFF; device = 0xFFFF; }
		
	//hprint(11,0,vendor); hprint(11,10,device);
		
	ctrl.index = 0;	
		if (result == 0 || imc_type) {
			for(i = 1; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
				if ((controllers[i].vendor == vendor) && (controllers[i].device == device)) {
					ctrl.index = i;
					break;
				}
			}
		}
	
	controllers[ctrl.index].setup_ecc();
	/* Don't enable ECC polling by default unless it has
	 * been well tested.
	 */
	//set_ecc_polling(-1);
	print_memory_controller();
	
	if(imc_type) { print_dmi_startup_info(); }

}

void poll_errors(void)
{
	if (ctrl.poll) {
		controllers[ctrl.index].poll_errors();
	}
}

/*
void set_ecc_polling(int val)
{
	int tested = controllers[ctrl.index].tested;
	if (val == -1) {
		val = tested;
	}
	if (val && (ctrl.mode & __ECC_DETECT)) {
		ctrl.poll = 1;
		cprint(LINE_INFO, COL_ECC, tested? " on": " ON");
	} else {
		ctrl.poll = 0;
		cprint(LINE_INFO, COL_ECC, "off");
	}
}
*/

