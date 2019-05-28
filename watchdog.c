#include <reg420.h>

// clear watchdog timer flag
void clr_watchdog() {  
	TA = 0xAA;						// timed access
	TA = 0x55;					   
	POR = 0;  						// clear power on reset flag for next time
	TA = 0xAA;						// timed access
	TA = 0x55;
	WTRF = 0; 						// clear watchdog timer reset flag for next time
}

// enable watchdog timer reset
void enable_watchdog() {
	TA = 0xAA;						// timed access
	TA = 0x55;
    EWT =1;            				// enable watchdog timer reset
}

// disable watchdog timer
void disable_watchdog() {
	TA = 0xAA;						// timed access
	TA = 0x55;
    EWT = 0;           				// disable watchdog timer reset
}

// reset watchdog timer
void reset_watchdog() {
	TA = 0xAA;						// timed access
	TA = 0x55;
    RWT = 1;						// reset the watchdog timer
}

// initialize watchdog timer, set watchdog interval
void init_watchdog(unsigned char interval) {
    switch (interval) {
        case 0:
            CKCON = (CKCON & 0x3F);   		// WD1:0 = 00  WD interval = (1/11.059MHz)*2^17 =   11.8 milliseconds
            break;
        case 1:
            CKCON = (CKCON & 0x3F) | 0x40;	// WD1:0 = 01  WD interval = (1/11.059MHz)*2^20 =   94.8 milliseconds
            break;
        case 2:
            CKCON = (CKCON & 0x3F) | 0x80;	// WD1:0 = 10  WD interval = (1/11.059MHz)*2^23 =  758.5 milliseconds
            break;
        case 3:
            CKCON = (CKCON | 0xC0);   		// WD1:0 = 11  WD interval = (1/11.059MHz)*2^26 = 6068.1 milliseconds
            break;
        default:
            CKCON = (CKCON | 0xC0);   		// WD1:0 = 11  WD interval = (1/11.059MHz)*2^26 = 6068.1 milliseconds
   }
  reset_watchdog();                	// reset watchdog timer
  enable_watchdog();               	// enable watchdog reset
}
