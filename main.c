/* Olympus PTT follower  uses 16F628A v1.0
 * Copyright (C) 2022  Richard Mudhar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 * 
 * V1.0 3 May 2022 RM          
 * Compile with MPLABX with XC8.
 * 
 * C Code for MPLAB XC8 compiler.
 * Minidisc time delay RLM 06 Jan 2005 (MPASM) ...time passes...
 * 24 Oct 2016 change to a LS10 controller. Power off LS10 3V3, remote rests H. Pull low thru 100k to start recording (line ends up ~1.5V) and pull fully low to stop
 * 30 Mar 2017 v18 use OD output for pull 100k (= REC)
 * (JAL) v19 successfully tested on LS10, PIC powered via two NiMH batteries, stop is pulled down via diode to keep LS10 3V3 (via 100k) from forward biasing protection diodes
 * use regular output via diode RA2 to pull to GND for stop, seems to work OK with 0.6V diode drop
 * LED is wired via 270 ohm resistor to MONI, other end to GND
 * LS10 cannot power this, 3v3 output on tip is unserviceable or never existed. 
 * 
 * 16F628A
 * PIC PINOUT
 *          o---- output to recorder
 *          |                              ------------------------------
 *          +------->|-------------- STOP | 1 RA2          RA1        18 |
 *          |                        MONI | 2 RA3          RA0        17 |
 *          +---/\/\/\/\-----  REC (O.D.) | 3 RA4          RA7        16 | 
 *               100k                     | 4 RA5(MCLR)    RA6        15 | 
 *                                    GND | 5 GND          VCC        14 | +3V
 *                                    BTN | 6 RB0          RB7        13 | PGD
 *                                        | 7 RB1          RB6        12 | PGC
 *                                        | 8 RB2          RB5        11 |
 *                                        | 9 RB3          RB4        12 | PGM
 *                                         ------------------------------
 * 
 * 
 * 
 * 
 * 
 */

// CONFIG
#pragma config FOSC = INTOSCCLK  // Oscillator Selection bits (INTOSC oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = ON       // Watchdog Timer Enable bit (WDT enabled)
#pragma config MCLRE = ON       // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is MCLR)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <stdlib.h>
#define _XTAL_FREQ 4000000
// probably worth checking that I can comment out the below two as already defined in XC8
//#define __delay_us(x) _delay((unsigned long)((x)*(_XTAL_FREQ/4000000.0)))
//#define __delay_ms(x) _delay((unsigned long)((x)*(_XTAL_FREQ/4000.0)))
#define BTN  PORTBbits.RB0

#define ASTOP 0b00000100
#define MONI 0b00001000
#define REC 0b00010000

// Globals


__bit recording=0;
__bit btnstat=0;
char portashadow;


void Clear_WDT (void) {
    asm("clrwdt");
 }


void rec_start (void)
{
    portashadow=portashadow & ~REC; // clear the bit
    PORTA=portashadow;
    __delay_ms(100);
    recording=1;
    portashadow=portashadow | REC; // set the bit
    PORTA=portashadow;
}

void rec_stop (void)
{
    portashadow=portashadow & ~ASTOP; // clear the bit
    PORTA=portashadow;
    __delay_ms(100);
    recording=0;
    portashadow=portashadow | ASTOP; // set the bit
    PORTA=portashadow;
}




void main(void)
{
    // initialise all the things
    CMCON = 0b111;		//comparator off
	TRISB = 0b11111111;			 // all inputs
	TRISA = 0b11100000;		//RA0-RA3 outputs 
    OPTION_REG = 0b00001111; // port B pull ups on, PSA to WDT (which isn't used)
    T1OSCEN=0;   // shut down int low power osc T1 to forestall this problem. 
                 // https://www.microchip.com/forums/FindPost/308594 Should be default anyway
    portashadow=0xff; // set this to all ones
    
    // now do a LAMP TEST on startup
    for (uint8_t i=0; i<2; i++)
    {
    portashadow=portashadow | MONI; // set the bit
    PORTA=portashadow;
        __delay_ms(150); // lamp test
    portashadow=portashadow & ~MONI; // clear the bit
    PORTA=portashadow;
        __delay_ms(150); // lamp test
    }
    btnstat=BTN; // initialise this as a copy of where the button really is
    
    // main loop follows here. BTN is active low - ie switch closure to ground to record, port B pullups set the high condition
    while(1){
        if (BTN != btnstat) {
            __delay_ms(50); // vague debounce
            if (BTN != btnstat) { // is this still true
                btnstat=BTN; // copy over
                // now do it
                if (!btnstat) {
                    rec_start();
                    __delay_ms(1000); // persist recording for 1 sec so even a 
                                      // slow Olympus LS10 doesn't get confused by a short stop
                } else 
                {
                    // wait for 0.5 sec before stop, helps using PTT
                    __delay_ms(500);
                    rec_stop();
                }
            if (recording) {
                portashadow=portashadow | MONI; // set the bit
                PORTA=portashadow;                
            } else {
                portashadow=portashadow & ~MONI; // clear the bit
                PORTA=portashadow;               
            }

            }
        }  
        Clear_WDT();            // kick the Dog
    }
}
