//************************************************************************//
//                                                                        //
//                       For use with 12MHZ crystal                       //
//                                                                        //
//************************************************************************//

// Interrupt driven serial 0 and serial 1 functions.
// Both serial 0 (for the console) and serial 1 (for the Wheelwriter) use 
// receive buffers in internal MOVX SRAM. Serial 0 in mode 1 uses timer 1 
// for baud rate generation. Serial 1 in mode 2 uses the system clock for 
// baud rate generation. init_serial0() and init_serial1() must be called 
// before using UARTs. No syntax error handling. No handshaking.

#include <reg420.h>

#define FALSE 0
#define TRUE  1

//////////////////////////////////////// Serial 0 /////////////////////////////////////
#define RBUFSIZE0 256							// size of the receive buffer in bytes

volatile unsigned char rx0_head;  	    	    // receive interrupt index for serial 0
volatile unsigned char rx0_tail;  	    	    // receive read index for serial 0
volatile unsigned char xdata rx0_buf[RBUFSIZE0]; // receive buffer for serial 0 in internal MOVX RAM
volatile bit tx0_ready;

// ---------------------------------------------------------------------------
// Serial 0 interrupt service routine
// ---------------------------------------------------------------------------
void serial0_isr(void) interrupt 4 using 2 {
   // serial 0 transmit interrupt
   if (TI) {                                        // transmit interrupt?
	   TI = FALSE;                                  // clear transmit interrupt flag
	   tx0_ready = TRUE;							// transmit buffer is ready for a new character
    }

    // serial 0 receive interrupt
    if(RI) {                                	    // serial 0 Receive character?
        RI = 0;                             	    // clear serial receive interrupt flag
        rx0_buf[rx0_head] = SBUF0;              	// Get character from serial port and put into serial 0 fifo.
		if (++rx0_head == RBUFSIZE0) rx0_head = 0;	// wrap pointer around to the beginning
    }
}

// ---------------------------------------------------------------------------
//  Initialize serial 0 for mode 1, standard full-duplex asynchronous communication, 
//  using timer 1 for baud rate generation.
// ---------------------------------------------------------------------------
void init_serial0(unsigned int baudrate) {
    rx0_head = 0;                   		// initialize head/tail pointers.
    rx0_tail = 0;

    SCON0 = 0x50;                  			// Serial 0 for mode 1.
    TMOD = (TMOD & 0x0F) | 0x20;   			// Timer 1, mode 2, 8-bit reload.
	CKMOD |= 0x10;				   			// Make timer 1 clocked by OSC/1 instead of the default OSC/12
    switch (baudrate) {                     // for 12MHz clock...
        case 28800:
            TH1 = 0xF3;                     // 28800 bps
            break;
        case 14400:
            TH1 = 0xE6;                     // 14400 bps
            break;
        case 9600:
            TH1 = 0xD9;                     // 9600 bps
            break;
        case 4800:
            TH1 = 0xB2;                     // 4800 bps
            break;
        case 2400:
            TH1 = 0x64;                     // 2400 bps
            break;
        default:
            TH1 = 0xD9;                     // 9600 bps
    }

    TR1 = TRUE;                    			// Run timer 1.
    REN = TRUE;                    			// Enable receive characters.
    TI = TRUE;                     			// Set TI of SCON to Get Ready to Send
    RI  = FALSE;                   			// Clear RI of SCON to Get Ready to Receive
    ES0 = TRUE;                    			// enable serial interrupt.
    EA = TRUE;                     			// enable global interrupt
}

// ---------------------------------------------------------------------------
// returns 1 if there are character waiting in the serial 0 receive buffer
// ---------------------------------------------------------------------------
char char_avail(void) {
   return (rx0_head != rx0_tail);
}

//-----------------------------------------------------------
// waits until a character is available in the serial 0 receive
// buffer. returns the character. does not echo the character.
//-----------------------------------------------------------
char getchar(void) {
    unsigned char buf;

    while (rx0_head == rx0_tail);     			// wait until a character is available
    buf = rx0_buf[rx0_tail];
	if (++rx0_tail == RBUFSIZE0) rx0_tail = 0; 
    return(buf);
}

// ---------------------------------------------------------------------------
// sends one character out to serial 0.
// ---------------------------------------------------------------------------
char putchar(char c)  {
   while (!tx0_ready);          // wait here for transmit ready
   SBUF0 = c;
   tx0_ready = 0;
   return (c);
}

///////////////////////////// Serial 1 interface to Wheelwriter ////////////////////////////
#define RBUFSIZE1 32                            // receive buffer for 32 integers (64 bytes)
volatile unsigned char data rx1_head;       	// receive interrupt index for serial 1
volatile unsigned char data rx1_tail;       	// receive read index for serial 1
volatile unsigned int xdata rx1_buf[RBUFSIZE1]; // receive buffer for serial 1 in internal MOVX RAM
volatile bit tx1_ready;                         // set when ready to transmit
sbit WWbus = P1^2;		  			            // P1.2, (RXD1, pin 3) used to monitor the Wheelwriter BUS
volatile bit waitingForAcknowledge = 0;         // TRUE when expecting the acknowledge pulse from Wheelwriter

// ---------------------------------------------------------------------------
// Serial 1 interrupt service routine
// ---------------------------------------------------------------------------
void serial1_isr(void) interrupt 7 using 3 {
	unsigned int wwBusData;
	static char count = 0;

    // serial 1 transmit interrupt
    if (TI1) {                                  // transmit interrupt?
	   TI1 = FALSE;                             // clear transmit interrupt flag
	   tx1_ready = TRUE;						// transmit buffer is ready for a new character
    }
    
    //serial 1 receive interrupt
    if(RI1) {                                	// receive interrupt?
       RI1 = 0;                             	// clear receive interrupt flag
       wwBusData = SBUF1;                       // retrieve the lower 8 bits
       if (RB81) wwBusData |= 0x0100;           // ninth bit is in RB81

       // discard the acknowledge pulse (all zeros)
       if (waitingForAcknowledge) {             // just transmitted a command, waiting for acknowledge...
          waitingForAcknowledge = FALSE;        // clear the flag
          if (wwBusData) {                      // if it's not acknowledge (all zeros) ...
             rx1_buf[rx1_head] = wwBusData;     // save it in the buffer
	         if (++rx1_head == RBUFSIZE1) rx1_head = 0;
          }
	   }
       else {                                   // not waiting for acknowledge...
          if (wwBusData == 0x121) count = 1; else ++count;
	      if (wwBusData || (count%2)) {         // if wwBusData is not zero or if it's the second zero...
             rx1_buf[rx1_head] = wwBusData;     // save it in the buffer
	         if (++rx1_head == RBUFSIZE1) rx1_head = 0;
	      }
       }
    }   
}

// ---------------------------------------------------------------------------
//  Initialize serial 1 for mode 2. Mode 2 is an asynchronous mode that transmits and receives
//  a total of 11 bits: 1 start bit, 9 data bits, and 1 stop bit. The ninth bit to be 
//  transmitted is determined by the value in TB8 (SCON1.3). When the ninth bit is received, 
//  it is stored in RB8 (SCON1.2). the baud rate for mode 2 is a function only of the oscillator
//  frequency. It is either the oscillator input divided by 32 or 64 as programmed by the SMOD 
//  doubler bit for the associated UART. The SMOD_1 baud-rate doubler bit for serial port 1 
//  is located at WDCON.7. In this case, the SMOD_1 baud-rate doubler bit is zero. Thus, the
//  12MHz clock divided by 64 gives a bit rate for serial 1 of 187500 bps.
// ---------------------------------------------------------------------------
void init_serial1(void) {
    rx1_head = 0;                   			// initialize serial 1 head/tail pointers.
    rx1_tail = 0;

    SMOD_1 = FALSE;                             // SMOD_1=0 therefor Serial 1 baud rate is oscillator freq (12MHz) divided by 64 (187,500 bps)
    SM01 = TRUE;                                // SM01=1, SM11=0, SM21=0 sets serial mode 2
    SM11 = FALSE;
    SM21 = FALSE;
    REN1 = TRUE;                    			// enable receive characters.
    TI1 = TRUE;                     			// set TI of SCON1 to Get Ready to Send
    RI1  = FALSE;                   			// clear RI of SCON1 to Get Ready to Receive
    ES1 = TRUE;                    				// enable serial interrupt.
    EA = TRUE;                     				// enable global interrupt
}

// ---------------------------------------------------------------------------
// sends an unsigned integer as 11 bits (start bit, 9 data bits, stop bit)
// ---------------------------------------------------------------------------
void send_WWdata(unsigned int wwCommand) {
   while (!tx1_ready);		 					// wait until transmit buffer is empty
   tx1_ready = 0;                               // clear flag   
   while(!WWbus);                               // wait until the Wheelwriter bus goes high
   REN1 = FALSE;                                // disable reception
   TB8_1 = (wwCommand & 0x100);                 // ninth bit
   SBUF1 = wwCommand & 0xFF;                    // lower 8 bits
   while(!tx1_ready);                           // wait until finished transmitting
   REN1 = TRUE;                                 // enable reception
   waitingForAcknowledge = TRUE;                // just transmitted a command, now waiting for acknowledge
   while(!WWbus);                               // wait until the Wheelwriter bus goes high
   while(WWbus);                                // wait until the Wheelwriter bus goes low (acknowledge)
   while(!WWbus);                               // wait until the Wheelwriter bus goes high again
}

// ---------------------------------------------------------------------------
// returns 1 if there is an unsigned integer from the Wheelwriter waiting in the serial 1 receive buffer.
// ---------------------------------------------------------------------------
char WWdata_avail(void) {
   return (rx1_head != rx1_tail);               // not equal means there's something in the buffer
}

//----------------------------------------------------------------------------
// returns the next unsigned integer from the Wheelwriter in the serial 1 receive buffer.
// waits for an integer to become available if necessary.
//----------------------------------------------------------------------------
unsigned int get_WWdata(void) {
    unsigned int buf;

    while (rx1_head == rx1_tail);     			// wait until a word is available
    buf = rx1_buf[rx1_tail];                    // retrieve the word from the buffer
	if (++rx1_tail == RBUFSIZE1) rx1_tail = 0;  // update the buffer pointer
    return(buf);
}

