// Read Wheelwriter BUS using serial mode 2 and a 12Mhz clock.

#include <stdio.h>
#include <reg420.h>
#include "uart12.h"
#include "watchdog.h"

#define CR    0x0D
#define LF    0x0A
#define BS    0x08
#define TAB   0x09
#define SPACE 0x20
#define DEL   0x7F

// 12,000,000 Hz /12 = 1,000,000 Hz = 1.0 microsecond clock period
// 50 milliseconds / 1.0 microseconds per clock = 50,000 clocks
#define RELOADHI (65536-50000)/256
#define RELOADLO (65536-50000)&255

#define FIFTEENPITCH 8                  // number of microspaces for each character on the 15P printwheel
#define TWELVEPITCH 10                  // number of microspaces for each character on the 12P printwheel
#define TENPITCH 12                     // number of microspaces for each character on the 10P printwheel

sbit switch1 = P0^0;                    // dip switch connected to pin 5 (ASCII/HEX mode)
sbit switch2 = P0^1;                    // dip switch connected to pin 6 (debug mode)  
sbit switch3 = P0^2;                    // dip switch connected to pin 7 (add linefeeds)
sbit switch4 = P0^3;                    // dip switch connected to pin 8 (not used)  

sbit redLED =   P0^4;                   // red LED connected to pin 35 0=on, 1=off
sbit amberLED = P0^5;                   // amber LED connected to pin 34 0=on, 1=off
sbit greenLED = P0^6;                   // green LED connected to pin 33 0=on, 1=off

code char title[]     = "DS89C440 Serial Mode 2 Read Version 1.1.0";
code char compiled[]  = "Compiled " __DATE__ " at " __TIME__;
code char copyright[] = "Copyright 2018 Jim Loos";

char microSpacesPerCharacter = 0;
volatile unsigned char tickcount = 0;


// ======================= timer0 ISR =======================
// every 50 milliseconds, 20 times per second
// ==========================================================
void Timer0_ISR() interrupt 1 {

    TL0 = RELOADLO;     			    //load timer 0 low byte
    TH0 = RELOADHI;     			    //load timer 0 high byte

    if (tickcount) {
       tickcount--;
    }
}

//------------------------------------------------------------
// initialize timer0
//------------------------------------------------------------
void init_timer0(){
	tickcount = 0;
	TL0 = RELOADLO;              	    //load timer 0 low byte
	TH0 = RELOADHI;              	    //load timer 0 high byte
	TMOD = (TMOD & 0xF0) | 0x01; 	    // set timer 0 to mode 1
	ET0 = 1;                     	    // enable timer 0 interrupt
	EA = 1;                      	    // global interrupt enable
	TR0 = 1;                     	    // run timer 0
}

// convert printwheel character to ASCII
code const char printwheel[96] = {
// a    n    r    m    c    s    d    h    l    f    k    ,    V    _    G    U  
  0x61,0x6E,0x72,0x6D,0x63,0x73,0x64,0x68,0x6C,0x66,0x6B,0x2C,0x56,0x2D,0x47,0x55,
// F    B    Z    H    P    )    R    L    S    N    C    T    D    E    I    A       
  0x46,0x42,0x5A,0x48,0x50,0x29,0x52,0x4C,0x53,0x4E,0x43,0x54,0x44,0x45,0x49,0x41,
// J    O    (    M    .    Y    ,    /    W    9    K    3    X    1    2    0 
  0x4A,0x4F,0x28,0x4D,0x2E,0x59,0x2C,0x2F,0x57,0x39,0x4B,0x33,0x58,0x31,0x32,0x30,
// 5    4    6    8    7    *    $    #    %    ¢    +    ±    @    Q    &    [
  0x35,0x34,0x36,0x38,0x37,0x2A,0x24,0x23,0x25,0xA2,0x2B,0xB1,0x40,0x51,0x26,0x5B,
// ]    ²    ³    º    §    ¶    ½    ¼    !    ?    "    '    =    :    -    ;   
  0x5D,0xB2,0xB3,0xBA,0xA7,0xB6,0xBD,0xBC,0x21,0x3F,0x22,0x60,0x3D,0x3A,0x5F,0x3B,
// x    q    v    z    w    j    .    y    b    g    u    p    i    t    o    e   
  0x78,0x71,0x76,0x7A,0x77,0x6A,0x2E,0x79,0x62,0x67,0x75,0x70,0x69,0x74,0x6F,0x65};

//------------------------------------------------------------------------------------------
// parses the data stream consisting of the 9 bit words received from the Wheelwriter BUS. 
// if ASCII mode, (switch 1 off), transmits the decoded ASCII character out through Serial0, 
// otherwise (switch 1 on) transmits the hexadecimal value of the word through Serial0.
//------------------------------------------------------------------------------------------
void parseWWdata(unsigned int WWdata) {
    static char state = 0;
    int n;

    if (switch1) {                      // if switch 1 is off (ASCII Mode)
        switch (state) {
            case 0:                     // waiting for first data word from Wheelwriter...
                if (WWdata == 0x121)    // all commands must start with 0x121...
                   state = 1;
                break;
            case 1:                     // 0x121 has been received...
                switch (WWdata) {
                    case 0x003:         // 0x121,0x003 is start of alpha-numeric character sequence
                        state = 2;
                        break;
                    case 0x004:         // 0x121,0x004 is start of erase sequence
                        putchar(SPACE); // overwrite the character with space
                        putchar(BS);   
                        state = 0;
                        break;
                    case 0x005:         // 0x121,0x005 is start of vertical movement sequence
                        state = 6;
                        break;          
                    case 0x006:         // 0x121,0x006 is start of horizontal movement sequence
                        state = 3;
                        break;
                    default:
                        state = 0;
            } // switch (WWdata)
                break;
            case 2:                     // 0x121,0x003 has been received...          
                if (WWdata)             // 0x121,0x003,printwheel code
                   putchar(printwheel[(WWdata-1)]); 
                else
                   putchar(SPACE);      // 0x121,0x003,0x000 is the sequence for SPACE
                state = 0;
                break;
            case 3:                     // 0x121,0x006 has been received...
                if (WWdata & 0x080)     // if bit 7 is set...         
                   state = 4;           // 0x121,0x006,0x080 is horizontal movement to the right...
                else                    // else...
                   state = 5;           // 0x121,0x006,0x000 is horizontal movement to the left...
                break;
           case 4:                     // 0x121,0x006,0x080 has been received, move carrier to the right...
                state = 0;
                if (WWdata>uSpacesPerChar) // if more than one space, must be tab
                    putchar(HT);
                else
                    putchar(SP);
                break;
            case 5:                     // 0x121,0x006,0x000 has been received, move carrier to the left...
                if (WWdata == microSpacesPerCharacter) 
                   putchar(BS);
                state = 0;
                break;
            case 6:                     // 0x121,0x005 has been received...
                if ((WWdata&0x1F) == uLinesPerLine)
                   putchar(CR);                // 0x121,0x005,0x090 is the sequence for paper up one line (for 10P, 12P and PS printwheels)
                state = 0;
        }   // switch (state)
    }  // if (switch1)
    else {                              // not ASCII mode, HEX mode instead
        if (WWdata == 0x121) 
           printf("\n");                // 0x121 starts on a new line
        printf("0x%03X\n",WWdata);      // print the data as three hex digits 
    } 
}


// MAIN =============================================================

void main(){

    unsigned int loopcounter;

    disable_watchdog();

	PMR |= 0x01;					    // enable internal SRAM MOVX memory
	init_serial0(9600);		            // initialize serial 0 for mode 1 at 9600bps
    init_serial1();                     // initialize serial 1 for mode 2
   	init_timer0();                      // timer 0 interrupts every 50 milliseconds

    printf("\r\n\n%s\r\n%s\r\n%s\r\n\n",title,compiled,copyright);
    switch (WDCON & 0x44) {
        case 0x00:
            printf("External reset\r\n\n");
            break;
        case 0x04:
            printf("Watchdog reset\r\n\n");
            break;
        case 0x40:
            printf("Power on reset\r\n\n");
            break;
        default:
            printf("Unknown reset\r\n\n");
    } // switch (WDCON & 0x44)

    microSpacesPerCharacter=TWELVEPITCH;
    init_watchdog(2);                   // change WD interval to (1/12MHz)*2^23 =  699.0 milliseconds
    reset_watchdog();
    amberLED = 1;                       // turn off the amber LED

	while(1){

       reset_watchdog();                // 'pet' the watchdog

       if (++loopcounter==0) {          // every 65536 times through the loop (at about 2Hz)
          greenLED = !greenLED;         // toggle the green LED
       }

	   if (WWdata_avail()) {            // if there's data from the Wheelwriter...
          parseWWdata(get_WWdata());
	   }
	}
}

