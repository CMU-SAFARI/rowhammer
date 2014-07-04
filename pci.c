/* pci.c - MemTest-86  Version 3.2
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 * ----------------------------------------------------
 * MemTest86+ V5.00 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.x86-secret.com - http://www.memtest.org
 */

#include "io.h"
#include "pci.h"
#include "test.h"
#include "stdint.h"
#include "cpuid.h"

#define PCI_CONF_TYPE_NONE 0
#define PCI_CONF_TYPE_1    1
#define PCI_CONF_TYPE_2    2

extern struct cpu_ident cpu_id;

static unsigned char pci_conf_type = PCI_CONF_TYPE_NONE;

#define PCI_CONF1_ADDRESS(bus, dev, fn, reg) \
	(0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

#define PCI_CONF2_ADDRESS(dev, reg)	(unsigned short)(0xC000 | (dev << 8) | reg)

#define PCI_CONF3_ADDRESS(bus, dev, fn, reg) \
	(0x80000000 | (((reg >> 8) & 0xF) << 24) | (bus << 16) | ((dev & 0x1F) << 11) | (fn << 8) | (reg & 0xFF))

int pci_conf_read(unsigned bus, unsigned dev, unsigned fn, unsigned reg, unsigned len, unsigned long *value)
{
	int result;

	if (!value || (bus > 255) || (dev > 31) || (fn > 7) || (reg > 255 && pci_conf_type != PCI_CONF_TYPE_1))
		return -1;

	result = -1;
	switch(pci_conf_type) {
	case PCI_CONF_TYPE_1:
		if(reg < 256){
			outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);
		}else{
			outl(PCI_CONF3_ADDRESS(bus, dev, fn, reg), 0xCF8);		
		}
		switch(len) {
		case 1:  *value = inb(0xCFC + (reg & 3)); result = 0; break;
		case 2:  *value = inw(0xCFC + (reg & 2)); result = 0; break;
		case 4:  *value = inl(0xCFC); result = 0; break;
		}
		break;
	case PCI_CONF_TYPE_2:
		outb(0xF0 | (fn << 1), 0xCF8);
		outb(bus, 0xCFA);

		switch(len) {
		case 1:  *value = inb(PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
		case 2:  *value = inw(PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
		case 4:  *value = inl(PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
		}
		outb(0, 0xCF8);
		break;
	}
	return result;
}

int pci_conf_write(unsigned bus, unsigned dev, unsigned fn, unsigned reg, unsigned len, unsigned long value)
{
	int result;

	if (!value || (bus > 255) || (dev > 31) || (fn > 7) || (reg > 255 && pci_conf_type != PCI_CONF_TYPE_1))
		return -1;

	result = -1;
	
	switch(pci_conf_type) 
	{
		case PCI_CONF_TYPE_1:
			if(reg < 256){
				outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);
			}else{
				outl(PCI_CONF3_ADDRESS(bus, dev, fn, reg), 0xCF8);		
			}
			switch(len) {
			case 1:  outb(value, 0xCFC + (reg & 3)); result = 0; break;
			case 2:  outw(value, 0xCFC + (reg & 2)); result = 0; break;
			case 4:  outl(value, 0xCFC); result = 0; break;
			}
			break;
		case PCI_CONF_TYPE_2:
			outb(0xF0 | (fn << 1), 0xCF8);
			outb(bus, 0xCFA);
	
			switch(len) {
			case 1: outb(value, PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
			case 2: outw(value, PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
			case 4: outl(value, PCI_CONF2_ADDRESS(dev, reg)); result = 0; break;
			}
			outb(0, 0xCF8);
			break;
	}
	return result;
}

static int pci_sanity_check(void)
{
	unsigned long value;
	int result;
	/* Do a trivial check to make certain we can see a host bridge.
	 * There are reportedly some buggy chipsets from intel and
	 * compaq where this test does not work, I will worry about
	 * that when we support them.
	 */
	result = pci_conf_read(0, 0, 0, PCI_CLASS_DEVICE, 2, &value);
	if (result == 0) {
		result = -1;
		if (value == PCI_CLASS_BRIDGE_HOST) {
			result = 0;
		}
	}
	return result;
}

static int pci_check_direct(void)
{
	unsigned char tmpCFB;
	unsigned int  tmpCF8;
	
	if (cpu_id.vend_id.char_array[0] == 'A' && cpu_id.vers.bits.family == 0xF) {
			pci_conf_type = PCI_CONF_TYPE_1;
			return 0;		
	} else {
			/* Check if configuration type 1 works. */
			pci_conf_type = PCI_CONF_TYPE_1;
			tmpCFB = inb(0xCFB);
			outb(0x01, 0xCFB);
			tmpCF8 = inl(0xCF8);
			outl(0x80000000, 0xCF8);
			if ((inl(0xCF8) == 0x80000000) && (pci_sanity_check() == 0)) {
				outl(tmpCF8, 0xCF8);
				outb(tmpCFB, 0xCFB);
				return 0;
			}
			outl(tmpCF8, 0xCF8);
		
			/* Check if configuration type 2 works. */
			
			pci_conf_type = PCI_CONF_TYPE_2;
			outb(0x00, 0xCFB);
			outb(0x00, 0xCF8);
			outb(0x00, 0xCFA);
			if (inb(0xCF8) == 0x00 && inb(0xCFA) == 0x00 && (pci_sanity_check() == 0)) {
				outb(tmpCFB, 0xCFB);
				return 0;
				
	}

	outb(tmpCFB, 0xCFB);

	/* Nothing worked return an error */
	pci_conf_type = PCI_CONF_TYPE_NONE;
	return -1;
	
	}
}

int pci_init(void)
{
	int result;
	/* For now just make certain we can directly
	 * use the pci functions.
	 */
	result = pci_check_direct();
	return result;
}
