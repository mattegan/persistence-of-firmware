#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <limits.h>

////////////////////////////////////////////////////////////////////////////////
// Project Level Includes
////////////////////////////////////////////////////////////////////////////////
#include "config.h"

void setLEDOutput(unsigned int flag);

void setup();
void setupClock();
void setupLEDs();
void setupSPI();
void setupAccelerometer();
void setupUART();

void SPIRead(unsigned char addr, int length, unsigned char *out);
void SPIWrite(unsigned char addr, int length, unsigned char *in);
void UARTPutC(unsigned char data);
void UARTPutStr(unsigned const char *str); //null terminated
char*intToStr(int num);

signed int getYAcceleration();

static unsigned long periodCount = 0;
static unsigned int periodCountOverflow = 0;

static unsigned int columnTimerPeriod;
static unsigned int columnTimerCount = 0;

static unsigned int posPeak = 1;
#define THRESHOLD 19000

static unsigned int currentColumn = 0;
static unsigned int columnCount = 69;
static unsigned int image[69] = {
    0b0000000000,
    0b0000000000,
    0b0000000000,
    0b0000000000,
    0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0001111111,
	0b0000000010,
	0b0000001100,
	0b0000000010,
	0b0001111111,
	0b0000000000,
	0b0000100000,
	0b0001010100,
	0b0001010100,
	0b0001010100,
	0b0001111000,
	0b0000000000,
	0b0000000100,
	0b0000111110,
	0b0001000100,
	0b0001000000,
	0b0000100000,
	0b0000000000,
	0b0000000100,
	0b0000111110,
	0b0001000100,
	0b0001000000,
	0b0000100000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
	0b0001111111,
	0b0001001001,
	0b0001001001,
	0b0001001001,
	0b0001000001,
	0b0000000000,
	0b0000110000,
	0b0101001000,
	0b0101001000,
	0b0101001000,
	0b0011111000,
	0b0000000000,
	0b0000100000,
	0b0001010100,
	0b0001010100,
	0b0001010100,
	0b0001111000,
	0b0000000000,
	0b0001111100,
	0b0000001000,
	0b0000000100,
	0b0000000100,
	0b0001111000,
	0b0000000000,
	0b0000000000,
	0b0000000000,
    0b0000000000,
    0b0000000000,
    0b0000000000,
    0b0000000000,
    0b0000000000,
};

int main(int argc, char** argv) {
    
    setup();
    
    //enable interrupts
    INTCONbits.GIE = 1;     //enable global interrupts
    INTCONbits.PEIE = 1;    //enable peripheral interrupts
    
    //setup timer 0, which times the length of one shake
    T0CONbits.T0CS = 0;     //timer 0 is driven by Fosc/4
    T0CONbits.PSA = 0;      //enable timer 0 clock prescaler

    T0CONbits.TMR0ON = 1;   //turn timer 0 on
    T0CONbits.T0PS = 0b010; //1:8 prescale value for timer 0;
    
    //INFO : timer 0 will overflow at 7.813KHz, meaning that one overflow of periodCount is equal to
    //       128us
    INTCONbits.TMR0IE = 1;  //enable timer 0 overflow interrupts    
    
    //setup timer 1, which times out each column of the display
    T1CONbits.TMR1ON = 1;   //turn on timer 1
    TMR1H = 0xF0;
    TMR1L = 0x00;
    
    //INFO : with a prescaler of 1:1, and Fosc/4 = 16MHz, timer 1 will overflow at 62.5kHz, and
    //       have a overflow period of 16us
    
    
    signed int y;
    signed int previousY = getYAcceleration();
    float shakePeriod;
    float columnPeriod;
    
    while(1) {
        
        //get the current y acceleration
        y = getYAcceleration();
        
        //need to do a peak find, alternating between "peaks" and "troughs"
        //alternating between searching for these get us the left and right
        //edges of the shake
        if(posPeak && y < previousY || !posPeak && y > previousY) {
            if(posPeak && (y > THRESHOLD) || !posPeak && y < -THRESHOLD) {
                //a peak was found, now we need to search for the opposite peak
                posPeak = !posPeak;
                
                //now we need to setup timer 1 to generate `rowCount` pulses during one shake
                shakePeriod = periodCount * 128.0e-6;
                columnPeriod = shakePeriod/((float)columnCount);
                columnTimerPeriod = (int)(columnPeriod/(256.0e-6));
                
                //enable TMR1 interrupts
                PIE1bits.TMR1IE = 1;      
                
                //start a new period
                periodCount = 0;
                columnTimerCount = 0;
                currentColumn = 0;
            }
        }
        previousY = y;               
    }
}

void interrupt isr(void) {
    if(INTCONbits.TMR0IF) {
        //this timer increments a period timer so we can count how long the last shake took
        periodCount++;
        INTCONbits.TMR0IF = 0;
    }
    if(PIR1bits.TMR1IF) {
        TMR1H = 0xF0;
        TMR1L = 0x00;
        if(columnTimerCount >= columnTimerPeriod) {
            currentColumn++;
            if(currentColumn > columnCount) {
                setLEDOutput(0b0000000000);
            } else {
                if(posPeak) {
                    setLEDOutput(image[columnCount - currentColumn - 1]);
                } else {
                    setLEDOutput(image[currentColumn]);
                }
            }
            columnTimerCount = 0;
        } else {
            columnTimerCount++;
        }
        PIR1bits.TMR1IF = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup Code
////////////////////////////////////////////////////////////////////////////////////////////////////

// top level setup, calls all other setup functions in the correct order
void setup() {
    setupClock();
    setupLEDs();
    setupSPI();
    setupAccelerometer();
    setupUART();
}

// Configures the internal oscillator to output 64MHz
void setupClock() {
    OSCCONbits.IRCF = 0b111;    //make internal oscillator run at 16MHz
    OSCTUNEbits.PLLEN = 1;      //enable the x4 PLL, only possible since FOSC is set to 0b1001 (9) 
                                //and IRCF is configured to 16MHz
}

//sets all LEDs as output pins
void setupLEDs() {
    TRISBbits.TRISB4 = 0;
    TRISBbits.TRISB5 = 0;
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;
    TRISAbits.TRISA2 = 0;
    TRISAbits.TRISA3 = 0;
    TRISAbits.TRISA4 = 0;
    TRISAbits.TRISA7 = 0;
    TRISCbits.TRISC0 = 0;
    TRISCbits.TRISC1 = 0;
}

// Configures SPI for communication with the accelerometer
void setupSPI() {
    
    //configure SPI pins
    TRISCbits.TRISC2 = 0;       //set CS as an output
    TRISCbits.TRISC3 = 0;       //set SCK as an output
    TRISCbits.TRISC5 = 0;       //set SDO as an output
    
    //bring CS high
    LATCbits.LATC2 = 1;
    
    //configure the MSSP module
    SSPSTATbits.SMP = 0;        //sample input data at middle of output data time
    SSPSTATbits.CKE = 0;        //data changes on clock transition idle -> active
    SSPCON1bits.CKP = 1;        //idle state for clock is a high level
    SSPCON1bits.SSPM = 0b0001;  //SPI master mode, clock = Fosc/16 (64MHz/16 = 4MHz)
    SSPCON1bits.SSPEN = 1;      //enable serial port
}

void setLEDOutput(unsigned int flag) {
    LATBbits.LATB4 = (flag >> 0) & 1;
    LATBbits.LATB5 = (flag >> 1) & 1;
    LATAbits.LATA0 = (flag >> 2) & 1;
    LATAbits.LATA1 = (flag >> 3) & 1;
    LATAbits.LATA2 = (flag >> 4) & 1;
    LATAbits.LATA3 = (flag >> 5) & 1;
    LATAbits.LATA4 = (flag >> 6) & 1;
    LATAbits.LATA7 = (flag >> 7) & 1;
    LATCbits.LATC0 = (flag >> 8) & 1;
    LATCbits.LATC1 = (flag >> 9) & 1;
}

void setupAccelerometer() {
    unsigned char data[2] = { 
        0b01100111,     //data rate -> 100Hz (ODR = 0b0110)
                        //block update -> continuous (BDU -> 0b1)
                        //axis enable -> X, Y, Z enabled
        0b00100000      //AA filter bandwidth -> 800Hz (BW -> 0b00)
                        //scale selection -> +-16Oh G (FSCALE -> 0b011);
                        //self test disabled (ST -> 0b00)
                        //SPI mode -> 4 wire mode (SIM -> 0);
    };
    SPIWrite(0x20, 1, &data[0]);
    SPIWrite(0x24, 1, &data[1]);
}

void setupUART() {
    //set the baud rate to 115.2k, using Fosc = 64MHz, the value for SPBRG is 
    //taken from the datasheet
    SPBRG = 8;
    SPBRGH = 0;
    
    //configure the TX and RX pins as outputs (the EUSART will reconfigure them
    //automatically
    TRISCbits.TRISC6 = 1;
    TRISCbits.TRISC7 = 1;
    
    //configure the EUSART in async mode
    TXSTAbits.SYNC = 0;
    
    //enable the serial port, and enable transmission
    RCSTAbits.SPEN = 1;
    TXSTAbits.TXEN = 1;   
}

void SPIRead(unsigned char addr, int length, unsigned char *out) {
    //bring CS low to begin the cycle
    LATCbits.LATC2 = 0;
    
    //load and tx the address into the SPI output buffer, with the read flag set
    //[7] is the R/W bit (1 -> read), and [6:0] are the address bits
    SSPBUF = addr | 0b10000000;
    while(!SSPSTATbits.BF);
    
    for(int i = 0; i < length; i++) {
        //write out dummy data to clock the device
        SSPBUF = 0xFF; 
        while(!SSPSTATbits.BF);
        out[i] = SSPBUF;
    }
    //bring CS high to end the cycle
    LATCbits.LATC2 = 1;
}

void SPIWrite(unsigned char addr, int length, unsigned char *in) {
    //bring CS low to begin the cycle
    LATCbits.LATC2 = 0;
    
    //load the address into the SPI output buffer, with the write flag set
    //[7] is the write bit (0 -> write), and [6:0] are the address bits
    //chop off the 7th bit of the address if it was set for some reason
    SSPBUF = addr & 0b01111111;
    while(!SSPSTATbits.BF);
    
    for(int i = 0; i < length; i++) {
        SSPBUF = in[i];
        while(!SSPSTATbits.BF);
    }
    
    LATCbits.LATC2 = 1;
}

void UARTPutC(unsigned char data) {
    TXREG = data;
    while(!TXSTAbits.TRMT);
}

void UARTPutStr(unsigned const char *data) {
    while(*data != '\0') {
        UARTPutC(*data);
        *data++;
    }
}

char *intToStr(int num) {
    static char out[7]; //INT_MIN = -32768, so we'll need at maximum 6
                        //chars and a null termination to represent an integer
    char digits[6];     //a place to store the digits
    int digitIndex = 0; //keep track of where in the digits array we're placing
                        //digits
    
    int offset = 0;
    if(num < 0) {
        out[offset++] = '-';
        num = num * -1;
    }
    
    do {
        digits[digitIndex++] = (num % 10) + '0';
        num /= 10;
    } while(num);
    
    for(int i = 0; i < digitIndex + offset; i++) {
        out[i + offset] = digits[digitIndex - i - 1];
    }
    out[digitIndex + offset] = '\0';
    return out;
}

signed int getYAcceleration() {
    unsigned char yData[2];
    
    //TODO : optimize this to use the automatic address increment feature
    //       of the accelerometer's SPI interface
    SPIRead(0x2B, 1, &yData[0]);    //read the MSB 8 bits of the Y axis
    SPIRead(0x2A, 1, &yData[1]);    //read the LSB 8 bits of the Y axis

    return (yData[0] << 8) | yData[1];
}