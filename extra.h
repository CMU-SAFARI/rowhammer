// This is the extra stuff added to the memtest+ from memtest.org
// Code from Eric Nelson and Wee
/* extra.c
 *
 * Released under version 2 of the Gnu Public License.
 *
 */

#ifndef MEMTEST_EXTRA_H
#define MEMTEST_EXTRA_H

void change_timing(int cas, int rcd, int rp, int ras);
void find_memctr(void);
void disclaimer(void); 
void get_option(void);
void get_menu(void);
void a64_parameter(void);
int get_cas(void);
void change_timing_i852(int cas, int rcd, int rp, int ras);
void change_timing_i925(int cas, int rcd, int rp, int ras);
void change_timing_i875(int cas, int rcd, int rp, int ras);
void change_timing_nf2(int cas, int rcd, int rp, int ras);
void change_timing_amd64(int cas, int rcd, int rp, int ras);
void amd64_tweak(int rwt, int wrt, int ref, int en2t, int rct, int rrd, int rwqb, int wr);
void __delay(ulong loops);

#endif /* MEMTEST_EXTRA_H */
