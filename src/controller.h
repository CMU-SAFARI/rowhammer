#ifndef MEMTEST_CONTROLLER_H
#define MEMTEST_CONTROLLER_H

struct pci_memory_controller {
	unsigned vendor;
	unsigned device;
	char *name;
	char *ram_type;
	int tested;
	void (*poll_fsb)(void);
	void (*poll_timings)(void);
	void (*setup_ecc)(void);
	void (*poll_errors)(void);
};

void find_controller(void);
void poll_errors(void);
void set_ecc_polling(int val);
void coretemp(void);
extern struct pci_memory_controller controllers[];


#endif /* MEMTEST_CONTROLLER_H */
