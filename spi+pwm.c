//******************************************************************************
//   MSP430G2xx3 Demo - USCI_A0, SPI 3-Wire Slave Data Echo
//
//   Description: SPI slave talks to SPI master using 3-wire mode. Data received
//   from master is echoed back.  USCI RX ISR is used to handle communication,
//   CPU normally in LPM4.  Prior to initial data exchange, master pulses
//   slaves RST for complete reset.
//   ACLK = n/a, MCLK = SMCLK = DCO ~1.2MHz
//
//   Use with SPI Master Incremented Data code example.  If the slave is in
//   debug mode, the reset signal from the master will conflict with slave's
//   JTAG; to work around, use IAR's "Release JTAG on Go" on slave device.  If
//   breakpoints are set in slave RX ISR, master must stopped also to avoid
//   overrunning slave RXBUF.
//
//                MSP430G2xx3
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          | |             XOUT|-
// Master---+-|RST              |
//            |             P1.2|<- Data Out (UCA0SOMI)
//            |                 |
//            |             P1.1|-> Data In (UCA0SIMO)
//            |                 |
//            |             P1.4|<- Serial Clock In (UCA0CLK)
//
//   D. Dang
//   Texas Instruments Inc.
//   February 2011
//   Built with CCS Version 4.2.0 and IAR Embedded Workbench Version: 5.10
//
//   msp430-gcc -Os -mmcu=msp430g2553 -o spi+pwm.elf spi+pwm.c
//******************************************************************************
#include "msp430g2553.h"
#include "spi+pwm.h"
#include <string.h>

#define MCU_CLOCK           1100000
//#define PWM_FREQUENCY       46      // In Hertz, ideally 50Hz.
#define PWM_FREQUENCY       50      // In Hertz, ideally 50Hz.
 
#define SERVO_MIN           720     // The minimum duty cycle for this servo
#define SERVO_MAX           2600    // The maximum duty cycle
 
unsigned int PWM_Period     = (MCU_CLOCK / PWM_FREQUENCY);  // PWM Period
unsigned int PWM_Duty       = 0;                            // %



/** Delay function. **/
void delay(unsigned int d) {
  int i;
  for (i = 0; i<d; i++) {
    nop();
  }
}

void flash_spi_detected(void) {
    int i=0;
    P1OUT = 0;
    for (i=0; i < 6; ++i) {
        P1OUT = ~P1OUT;
        delay(0x4fff);
        delay(0x4fff);
    }
}

void setup_led1(void) {
    P1DIR |= BIT0;
}

void setup_spi(void) {
    P1SEL |= BIT1 + BIT2 + BIT4;			// Select P1.1 P1.2 P1.4
    P1SEL2 |= BIT1 + BIT2 + BIT4;			// Alternate mode
    UCA0CTL1 = UCSWRST;                       // **Put state machine in reset**
    UCA0CTL0 |= UCMSB + UCSYNC;               // 3-pin, 8-bit SPI master
    UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;                          // Enable USCI0 RX interrupt
}

unsigned int servo_lut[ SERVO_STEPS+1 ];
void setup_pwm(void) {

    unsigned int servo_stepval, servo_stepnow;
    unsigned int i;
 
    // Calculate the step value and define the current step, defaults to minimum.
    servo_stepval   = ( (SERVO_MAX - SERVO_MIN) / SERVO_STEPS );
    servo_stepnow   = SERVO_MIN;
 
    // Fill up the LUT
    for (i = 0; i < SERVO_STEPS; i++) {
        servo_stepnow += servo_stepval;
        servo_lut[i] = servo_stepnow;
    }

    P2DIR   |= BIT1 | BIT4;             // P2.1, P2.4 = output
    P2SEL   |= BIT1 | BIT4;             // P2.1, P2.4 use alternate functions
    
    TA1CCTL1 = OUTMOD_7;                // Timer1, TACCR1 reset/set
    TA1CCTL2 = OUTMOD_7;                // Timer1, TACCR2 toggle, interrupt enabled

    TA1CCR0  = PWM_Period-1;        // PWM Period
    TA1CCR1  = PWM_Duty;            
    TA1CCR2  = PWM_Duty;            

    TA1CTL   = TASSEL_2 + MC_1;     // SMCLK, upmode
}

#define CMD_BEGIN	0
#define CMD_DEVICE  1
#define CMD_X       2
#define CMD_Y       3
#define CMD_END		4
volatile unsigned char cmd[5];
volatile unsigned char flag_set_servos = 0;

void main(void)
{
    int x, y;
    int i=0;
    WDTCTL = WDTPW + WDTHOLD;           // Stop watchdog timer
    setup_led1();
    P1OUT = 0;
    setup_pwm(); while (P1IN & BIT4);	// If clock sig from mstr stays low,
                                       	// it is not yet in SPI mode
    flash_spi_detected();               // Blink 3 times
    setup_spi();
  
    __bis_SR_register(GIE);
	while (1);
	/*
    while (1) {
		if (flag_set_servos) {
			// Assign a new pwm, effectively, moving the motors
			TA1CCR2 = servo_lut[cmd[CMD_X]];
			TA1CCR1 = servo_lut[cmd[CMD_Y]];
			flag_set_servos = 0;
			// Invert the onboard MSP LED
			P1OUT = ~P1OUT;
		}
	}
	*/
/*
	i = 0;
	while (1) {
		TA1CCR1 = servo_lut[i];
		TA1CCR2 = servo_lut[i];
		i++;
		if (i >= SERVO_STEPS) {
			i = 0;
		}
        delay(0x0fff);
	}
	*/
}

volatile unsigned char cmd_index = CMD_BEGIN;
__attribute__((interrupt(USCIAB0RX_VECTOR))) void USCI0RX_ISR (void)
{
    volatile unsigned char stat = UCA0STAT;
	/*
	if (stat) {
		P1OUT = ~P1OUT;
	}
	*/

	switch (cmd_index) {
		case CMD_BEGIN:
			cmd[CMD_BEGIN] = UCA0RXBUF;
			if (cmd[CMD_BEGIN] == 0x02) {
				cmd_index = CMD_X;
			}
			break;
			/*
		case CMD_DEVICE:
			cmd[CMD_DEVICE] = UCA0RXBUF;

			cmd_index = CMD_X;
			break;
			*/
		case CMD_X:
			cmd[CMD_X] = UCA0RXBUF;

			if (cmd[CMD_X] >= SERVO_STEPS) {
				cmd[CMD_X] = SERVO_STEPS - 1;
			}
			cmd_index = CMD_Y;
			break;
		case CMD_Y:
			cmd[CMD_Y] = UCA0RXBUF;

			if (cmd[CMD_Y] >= SERVO_STEPS) {
				cmd[CMD_Y] = SERVO_STEPS - 1;
			}
			cmd_index = CMD_END;
			break;
		case CMD_END:
			cmd[CMD_END] = UCA0RXBUF;

			if (cmd[CMD_END] == 0x18) {
				TA1CCR2 = servo_lut[cmd[CMD_X]];
				TA1CCR1 = servo_lut[cmd[CMD_Y]];
			}

			cmd_index = CMD_BEGIN;
			break;
	}
}
