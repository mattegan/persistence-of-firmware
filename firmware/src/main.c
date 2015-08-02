#include <stdio.h>
#include <stdlib.h>
#include <xc.h>

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

int main(int argc, char** argv) {
    
    setup();
    
    //enable interrupts
    INTCONbits.GIE = 1;     //enable global interrupts
    INTCONbits.PEIE = 1;    //enable peripheral interrupts
    
    //setup a timer
    T0CONbits.T0CS = 0;     //timer 0 is driven by Fosc/4
    T0CONbits.PSA = 0;      //enable timer 0 clock prescaler
    
    TMR0L = 255;            //timer counts to 255 ~ 4ms period
    T0CONbits.TMR0ON = 1;   //turn timer 0 on
    T0CONbits.T0PS = 0b111; //1:256 prescale value for timer 0;
    
    INTCONbits.TMR0IE = 1;  //enable timer 0 overflow interrupts    
       
    int peakFind = 1;
    signed int y;
    signed int previousY = getYAcceleration();
    
    signed int threshold = 19000;
    
    while(1) {
        
        //get the current y acceleration
        y = getYAcceleration();
        
        //need to do a peak find, alternating between "peaks" and "troughs"
        //alternating between searching for these get us the left and right
        //edges of the shake
        if(peakFind && y < previousY || !peakFind && y > previousY) {
            if(peakFind && (y > threshold) || !peakFind && y < -threshold) {
                peakFind = !peakFind;
                //reset timer 0
                T0CONbits.TMR0ON = 0;   //turn timer 0 off
                T0CONbits.TMR0ON = 1;   //turn timer 0 on
                T0CONbits.T0PS = 0b111; //1:256 prescale value for timer 0;
                if(peakFind) {
                    setLEDOutput(0b1111100000);
                }else {
                    setLEDOutput(0b0000011111);
                }
            }
        }
        previousY = y;               
    }
}

void interrupt isr(void) {
    if(INTCONbits.TMR0IF) {
        setLEDOutput(0b0000000000);
        INTCONbits.TMR0IF = 0;
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