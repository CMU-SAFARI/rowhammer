/* config.c - MemTest-86  Version 3.4
 *
 * Released under version 2 of the Gnu Public License.
 * By Chris Brady
 * ----------------------------------------------------
 * MemTest86+ V5.00 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.x86-secret.com - http://www.memtest.org
 */
#include "test.h"
#include "screen_buffer.h"
#include "dmi.h"

extern int bail, beepmode;
extern struct tseq tseq[];
extern short e820_nr;
void performance();
extern volatile short cpu_mode;
extern volatile int test;
extern void find_chunks();
extern volatile short  start_seq;
extern short restart_flag;
extern short onepass;
extern short btflag;

extern void get_list(int x, int y, int len, char *buf);

char save[2][POP_H][POP_W];
char save2[2][POP2_H][POP2_W];

void get_config()
{
	int flag = 0, sflag = 0, i, j, k, n, m, prt = 0;
  int reprint_screen = 0;
  char cp[64];
	ulong page;

	popup();
	wait_keyup();
	while(!flag) {
		cprint(POP_Y+1,  POP_X+2, "Settings:");
		cprint(POP_Y+3,  POP_X+6, "(1) Test Selection");
		cprint(POP_Y+4,  POP_X+6, "(2) Address Range");
		cprint(POP_Y+5,  POP_X+6, "(3) Error Report Mode");
		cprint(POP_Y+6,  POP_X+6, "(4) Core Selection");
		cprint(POP_Y+7,  POP_X+6, "(5) Refresh Screen");
		cprint(POP_Y+8,  POP_X+6, "(6) Display DMI Data");
		cprint(POP_Y+9,  POP_X+6, "(7) Display SPD Data");
		cprint(POP_Y+11, POP_X+6,	"(0) Continue");

		/* Wait for key release */
		/* Fooey! This nuts'es up the serial input. */
		sflag = 0;
		switch(get_key()) {
		case 2:
			/* 1 - Test Selection */
			popclear();
			cprint(POP_Y+1, POP_X+2, "Test Selection:");
			cprint(POP_Y+3, POP_X+6, "(1) Default Tests");
			cprint(POP_Y+4, POP_X+6, "(2) Skip Current Test");
			cprint(POP_Y+5, POP_X+6, "(3) Select Test");
		  cprint(POP_Y+6, POP_X+6, "(4) Enter Test List");
			cprint(POP_Y+7, POP_X+6, "(0) Cancel");
			if (v->testsel < 0) {
				cprint(POP_Y+3, POP_X+5, ">");
			} else {
				cprint(POP_Y+5, POP_X+5, ">");
			}
			wait_keyup();
			while (!sflag) {
				switch(get_key()) {
				case 2:
					/* Default - All tests */
					i = 0;
					while (tseq[i].cpu_sel) {
					    tseq[i].sel = 1;
					    i++;
					}
					find_ticks_for_pass();
					sflag++;
					break;
				case 3:
					/* Skip test */
					bail++;
					sflag++;
					break;
				case 4:
					/* Select test */
					popclear();
					cprint(POP_Y+1, POP_X+3,
						"Test Selection:");
					cprint(POP_Y+4, POP_X+5,
						"Test Number [1-11]: ");
					n = getval(POP_Y+4, POP_X+24, 0) - 1;
					if (n <= 11) 
						{
					    /* Deselect all tests */
					    i = 0;
					    while (tseq[i].cpu_sel) {
					        tseq[i].sel = 0;
					        i++;
					    }
					    /* Now set the selection */
					    tseq[n].sel = 1;
					    v->pass = -1;
					    test = n;
					    find_ticks_for_pass();
					    sflag++;
          		bail++;
						}
					break;
				case 5:
					/* Enter a test list */
					popclear();
					cprint(POP_Y+1, POP_X+3,
				"Enter a comma separated list");
					cprint(POP_Y+2, POP_X+3,
				"of tests to execute:");
					cprint(POP_Y+5, POP_X+5, "List: ");
					/* Deselect all tests */
					k = 0;
					while (tseq[k].cpu_sel) {
					    tseq[k].sel = 0;
					    k++;
					}

					/* Get the list */
					for (i=0; i<64; i++) cp[i] = 0;
					get_list(POP_Y+5, POP_X+10, 64, cp);

					/* Now enable all of the tests in the
					 * list */
					i = j = m = 0;
					while (1) {
					    if (isdigit(cp[i])) {
						n = cp[i]-'0';
						j = j*10 + n;
						i++;
						if (cp[i] == ',' || cp[i] == 0){
						    if (j < k) {
							tseq[j].sel = 1;
							m++;
						    }
						    if (cp[i] == 0) break;
						    j = 0;
						    i++;
						}
					    }
					}

					/* If we didn't select at least one
					 * test turn them all back on */
					if (m == 0) {
					    k = 0;
					    while (tseq[k].cpu_sel) {
					        tseq[k].sel = 1;
					        k++;
					    }
					}
					v->pass = -1;
					test = n;
					find_ticks_for_pass();
					sflag++;
          bail++;
					break;
				case 11:
				case 57:
					sflag++;
					break;
				}
			}
			popclear();
			break;
		case 3:
			/* 2 - Address Range */
			popclear();
			cprint(POP_Y+1, POP_X+2, "Test Address Range:");
			cprint(POP_Y+3, POP_X+6, "(1) Set Lower Limit");
			cprint(POP_Y+4, POP_X+6, "(2) Set Upper Limit");
			cprint(POP_Y+5, POP_X+6, "(3) Test All Memory");
			cprint(POP_Y+6, POP_X+6, "(0) Cancel");
			wait_keyup();
			while (!sflag) {
				switch(get_key()) {
				case 2:
					/* Lower Limit */
					popclear();
					cprint(POP_Y+2, POP_X+4,
						"Lower Limit: ");
					cprint(POP_Y+4, POP_X+4,
						"Current: ");
					aprint(POP_Y+4, POP_X+13, v->plim_lower);
					cprint(POP_Y+6, POP_X+4,
						"New: ");
					page = getval(POP_Y+6, POP_X+9, 12);
					if (page + 1 <= v->plim_upper) {
						v->plim_lower = page;
						test--;
						bail++;
					}
					adj_mem();
					find_chunks();
					find_ticks_for_pass();
					sflag++;
					break;
				case 3:
					/* Upper Limit */
					popclear();
					cprint(POP_Y+2, POP_X+4,
						"Upper Limit: ");
					cprint(POP_Y+4, POP_X+4,
						"Current: ");
					aprint(POP_Y+4, POP_X+13, v->plim_upper);
					cprint(POP_Y+6, POP_X+4,
						"New: ");
					page = getval(POP_Y+6, POP_X+9, 12);
					if  (page - 1 >= v->plim_lower) {
						v->plim_upper = page;
						bail++;
						test--;
					}
					adj_mem();
					find_chunks();
					find_ticks_for_pass();
					sflag++;
					break;
				case 4:
					/* All of memory */
					v->plim_lower = 0;
					v->plim_upper =
						v->pmap[v->msegs - 1].end;
					test--;
					bail++;
					adj_mem();
					find_chunks();
					find_ticks_for_pass();
					sflag++;
					break;
				case 11:
				case 57:
					/* 0/CR - Continue */
					sflag++;
					break;
				}
			}
			popclear();
			break;
		case 4:
			/* Error Mode */
			popclear();
			cprint(POP_Y+1, POP_X+2, "Printing Mode:");
			cprint(POP_Y+3, POP_X+6, "(1) Error Summary");
			cprint(POP_Y+4, POP_X+6, "(2) Individual Errors");
			cprint(POP_Y+5, POP_X+6, "(3) BadRAM Patterns");
			cprint(POP_Y+6, POP_X+6, "(4) Error Counts Only");
			cprint(POP_Y+7, POP_X+6, "(5) Beep on Error");
			cprint(POP_Y+8, POP_X+6, "(0) Cancel");
			cprint(POP_Y+3+v->printmode, POP_X+5, ">");
			if (beepmode) { cprint(POP_Y+7, POP_X+5, ">"); }
			wait_keyup();
			while (!sflag) {
				switch(get_key()) {
				case 2:
					/* Error Summary */
					v->printmode=PRINTMODE_SUMMARY;
					v->erri.eadr = 0;
					v->erri.hdr_flag = 0;
					sflag++;
					break;
				case 3:
					/* Separate Addresses */
					v->printmode=PRINTMODE_ADDRESSES;
					v->erri.eadr = 0;
					v->erri.hdr_flag = 0;
					v->msg_line = LINE_SCROLL-1;
					sflag++;
					break;
				case 4:
					/* BadRAM Patterns */
					v->printmode=PRINTMODE_PATTERNS;
					v->erri.hdr_flag = 0;
					sflag++;
					prt++;
					break;
				case 5:
					/* Error Counts Only */
					v->printmode=PRINTMODE_NONE;
					v->erri.hdr_flag = 0;
					sflag++;
					break;
				case 6:
					/* Set Beep On Error mode */
					beepmode = !beepmode;
					sflag++;
					break;						
				case 11:
				case 57:
					/* 0/CR - Continue */
					sflag++;
					break;
				}
			}
			popclear();
			break;
		case 5:
    			/* CPU Mode */
    	reprint_screen = 1;
			popclear();
			cprint(POP_Y+1, POP_X+2, "CPU Selection Mode:");
			cprint(POP_Y+3, POP_X+6, "(1) Parallel (All)");
			cprint(POP_Y+4, POP_X+6, "(2) Round Robin (RRb)");
			cprint(POP_Y+5, POP_X+6, "(3) Sequential (Seq)");
			cprint(POP_Y+6, POP_X+6, "(0) Cancel");
			cprint(POP_Y+2+cpu_mode, POP_X+5, ">");
			wait_keyup();
			while(!sflag) {
				switch(get_key()) {
				case 2:
					if (cpu_mode != CPM_ALL) bail++;
					cpu_mode = CPM_ALL;
					sflag++;
					popdown();
					cprint(9,34,"All");
					popup();
					break;
				case 3:
					if (cpu_mode != CPM_RROBIN) bail++;
					cpu_mode = CPM_RROBIN;
					sflag++;
					popdown();
					cprint(9,34,"RRb");
					popup();
					break;
				case 4:
					if (cpu_mode != CPM_SEQ) bail++;
					cpu_mode = CPM_SEQ;
					sflag++;
					popdown();
					cprint(9,34,"Seq");
					popup();
					break;
				case 11:
				case 57:
					/* 0/CR - Continue */
					sflag++;
					break;
				}
			}
			popclear();
			break;
		case 6:
			reprint_screen = 1;
			flag++;
			break;
		case 7:
			/* Display DMI Memory Info */
			pop2up();
      print_dmi_info();
			pop2down();			
			break;
		case 8:
			/* Display SPD Data */			
			popdown();
			show_spd();
			popup();
			sflag++;
			break;
		case 11:
		case 57:
		case 28:
			/* 0/CR/SP - Continue */
			flag++;
			break;
		}
	}
	popdown();
	if (prt) {
		printpatn();
	}
        if (reprint_screen){
            tty_print_screen();
        }
}

void popup()
{
	int i, j;
	char *pp;
	
	for (i=POP_Y; i<POP_Y + POP_H; i++) { 
		for (j=POP_X; j<POP_X + POP_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
                        save[0][i-POP_Y][j-POP_X] = *pp;  /* Save screen */
                        set_scrn_buf(i, j, ' ');
			*pp = ' ';		/* Clear */                        
			pp++;
                        save[1][i-POP_Y][j-POP_X] = *pp;
			*pp = 0x07;		/* Change Background to black */
		}
	}
        tty_print_region(POP_Y, POP_X, POP_Y+POP_H, POP_X+POP_W);
}

void popdown()
{
	int i, j;
	char *pp;
	
	for (i=POP_Y; i<POP_Y + POP_H; i++) { 
		for (j=POP_X; j<POP_X + POP_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
			*pp = save[0][i-POP_Y][j-POP_X]; /* Restore screen */
                        set_scrn_buf(i, j, save[0][i-POP_Y][j-POP_X]);
			pp++;
			*pp = save[1][i-POP_Y][j-POP_X]; /* Restore color */
		}
	}
        tty_print_region(POP_Y, POP_X, POP_Y+POP_H, POP_X+POP_W);
}

void popclear()
{
	int i, j;
	char *pp;
	
	for (i=POP_Y; i<POP_Y + POP_H; i++) { 
		for (j=POP_X; j<POP_X + POP_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
			*pp = ' ';		/* Clear popup */
                        set_scrn_buf(i, j, ' ');
			pp++;
		}
	}
        tty_print_region(POP_Y, POP_X, POP_Y+POP_H, POP_X+POP_W);
}

void pop2up()
{
	int i, j;
	char *pp;

	for (i=POP2_Y; i<POP2_Y + POP2_H; i++) { 
		for (j=POP2_X; j<POP2_X + POP2_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
			save2[0][i-POP2_Y][j-POP2_X] = *pp;  /* Save screen */
			set_scrn_buf(i, j, ' ');
			*pp = ' ';		/* Clear */
			pp++;
			save2[1][i-POP2_Y][j-POP2_X] = *pp;
			*pp = 0x07;		/* Change Background to black */
		}
	}
        tty_print_region(POP2_Y, POP2_X, POP2_Y+POP2_H, POP2_X+POP2_W);
}

void pop2down()
{
	int i, j;
	char *pp;

	for (i=POP2_Y; i<POP2_Y + POP2_H; i++) { 
		for (j=POP2_X; j<POP2_X + POP2_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
			*pp = save2[0][i-POP2_Y][j-POP2_X]; /* Restore screen */
			set_scrn_buf(i, j, save2[0][i-POP2_Y][j-POP2_X]);
			pp++;
			*pp = save2[1][i-POP2_Y][j-POP2_X]; /* Restore color */
		}
	}
        tty_print_region(POP2_Y, POP2_X, POP2_Y+POP2_H, POP2_X+POP2_W);
}

void pop2clear()
{
	int i, j;
	char *pp;

	for (i=POP2_Y; i<POP2_Y + POP2_H; i++) { 
		for (j=POP2_X; j<POP2_X + POP2_W; j++) { 
			pp = (char *)(SCREEN_ADR + (i * 160) + (j * 2));
			*pp = ' ';		/* Clear popup */
			set_scrn_buf(i, j, ' ');
			pp++;
		}
	}
        tty_print_region(POP2_Y, POP2_X, POP2_Y+POP2_H, POP2_X+POP2_W);
}


void adj_mem(void)
{
	int i;

	v->selected_pages = 0;
	for (i=0; i< v->msegs; i++) {
		/* Segment inside limits ? */
		if (v->pmap[i].start >= v->plim_lower &&
				v->pmap[i].end <= v->plim_upper) {
			v->selected_pages += (v->pmap[i].end - v->pmap[i].start);
			continue;
		}
		/* Segment starts below limit? */
		if (v->pmap[i].start < v->plim_lower) {
			/* Also ends below limit? */
			if (v->pmap[i].end < v->plim_lower) {
				continue;
			}
			
			/* Ends past upper limit? */
			if (v->pmap[i].end > v->plim_upper) {
				v->selected_pages += 
					v->plim_upper - v->plim_lower;
			} else {
				/* Straddles lower limit */
				v->selected_pages += 
					(v->pmap[i].end - v->plim_lower);
			}
			continue;
		}
		/* Segment ends above limit? */
		if (v->pmap[i].end > v->plim_upper) {
			/* Also starts above limit? */
			if (v->pmap[i].start > v->plim_upper) {
				continue;
			}
			/* Straddles upper limit */
			v->selected_pages += 
				(v->plim_upper - v->pmap[i].start);
		}
	}
}
