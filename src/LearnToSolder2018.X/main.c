/*
 * Learn To Solder 2018 board software
 * 
 * Written by Brian Schmalz of Schmalz Haus LLC
 * brian@schmalzhaus.com
 * 
 * Copyright 2018
 * All of this code is in the public domain
 * 
 * Versions:
 * 4/10/18 0.1 First debugging code available for new (Charliplexed) board
 *             Simply lights LEDs on Left/Right side in sequence when button
 *             pushed
 * 4/11/18 0.2 Fixed LED's off state to be outputs all low. This improved sleep
 *             current draw from about 30 uA to 211 nA. Also, in v0.1 when all
 *             LED outputs were set to be inputs when sleeping, the current drawn
 *             from the battery would vary from 500 nA to 30 uA based on ambient
 *             light levels. (!!)
 */

#include "mcc_generated_files/mcc.h"

#define SLOW_DELAY 500

/* Switch inputs :  (pressed = low)
 *   Left = S2 = GP2
 *   Right = S1 = GP3
 * 
 * LEDs:
 * Right : D1 (blue), D2 (yellow), D3, (red), D4, (green)
 * Left : D5 (red), D6 (green), D7, (blue) D8 (yellow)
 *
 * GP0, GP1, GP4 and GP5 are the I/O pins connected to the 8 LEDs
 *
 * LED table 
 * 
 *          GP0 GP1 GP4 GP5       TRISA           PORTA
 * D1 on     X   X   1   0   0b11001111 0xCF  0b00010000 0x10  Right blue
 * D2 on     X   X   0   1   0b11001111 0xCF  0b00100000 0x20  Right yellow
 * D3 on     1   0   X   X   0b11111100 0xFC  0b00000001 0x01  Right red
 * D4 on     0   1   X   X   0b11111100 0xFC  0b00000010 0x02  Right green
 * D5 on     X   1   0   X   0b11101101 0xED  0b00000010 0x02  Left red
 * D6 on     X   0   1   X   0b11101101 0xED  0b00010000 0x10  Left green
 * D7 on     0   X   X   1   0b11011110 0xDE  0b00100000 0x01  Left blue
 * D8 on     1   X   X   0   0b11011110 0xDE  0b00000001 0x20  Left yellow
 * all off   0   0   0   0   0b11001100 0xCC  0b00000000 0x00  All off
 *  
 */

/* Named indexes into LED_Array */
typedef enum
{
  LED_R_BLUE = 0,
  LED_R_YELLOW,
  LED_R_RED,
  LED_R_GREEN,
  LED_L_RED,
  LED_L_GREEN,
  LED_L_BLUE,
  LED_L_YELLOW,
  LED_OFF,
  LED_LAST
}
LEDBitPatternIndex_t;

uint8_t LED_Array_TRIS[] = {
  0xCF,
  0xCF,
  0xFC,
  0xFC,
  0xED,
  0xED,
  0xDE,
  0xDE,
  0xCC
};

uint8_t LED_Array_PORT[] = {
  0x10,
  0x20,
  0x01,
  0x02,
  0x02,
  0x10,
  0x01,
  0x20,
  0x00
};

void SetLEDs(LEDBitPatternIndex_t newPattern)
{
  if (newPattern < LED_LAST)
  {
    TRISA = LED_Array_TRIS[newPattern];
    PORTA = LED_Array_PORT[newPattern];
  }
}


/*
                         Main application
 */
void main(void)
{
  uint8_t i;
  
  // initialize the device
  SYSTEM_Initialize();

  // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
  // Use the following macros to:

  // Enable the Global Interrupts
  INTERRUPT_GlobalInterruptEnable();

  // Enable the Peripheral Interrupts
  INTERRUPT_PeripheralInterruptEnable();

  // Disable the Global Interrupts
  //INTERRUPT_GlobalInterruptDisable();

  // Disable the Peripheral Interrupts
  //INTERRUPT_PeripheralInterruptDisable();

  // 29.5 mV (1mV/uA) = 29 uA
  // 1.168V  (1mV/nA) = 1168  
    
  while (1)
  {
    if (PORTAbits.RA2 == 0)
    {
      // Left button pushed
      for (i=0; i < 1; i++)
      {
        SetLEDs(LED_L_BLUE);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_L_YELLOW);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_L_RED);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_L_GREEN);
        __delay_ms(SLOW_DELAY);
      }
    }
    else if (PORTAbits.RA3 == 0)
    {
      // Right button pushed
      for (i=0; i < 1; i++)
      {
        SetLEDs(LED_R_BLUE);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_R_YELLOW);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_R_RED);
        __delay_ms(SLOW_DELAY);
        SetLEDs(LED_R_GREEN);
        __delay_ms(SLOW_DELAY);
      }
    }

    SetLEDs(LED_OFF);
    
    // Hit the VREGPM bit to put us in low power sleep mode
    VREGCONbits.VREGPM = 1;

    SLEEP();
  }
}
/**
 End of File
*/