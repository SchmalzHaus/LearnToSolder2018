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
 * 4/15/18 0.3 Added real Charliplexing code, arbitrary LEDs can now be on
 *             Added effect state machines so a button press will restart effect
 *               Both effects are now asynchronous
 */

#include "mcc_generated_files/mcc.h"

#define SLOW_DELAY 2000

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
 *  State 0:
 *   Set A0, A1, A5 outputs, A4 input
 *   Set A0 low
 *     If D8 should be on, set A5 high
 *     If D4 should be on, set A1 high
 * State 1:
 *   Set A0, A1, A4 outputs, A5 input
 *   Set A1 low
 *     If D6 should be on, set A4 high
 *     If D3 should be on, set A0 high
 * State 2:
 *   Set A4, A1, A5 outputs, A0 input
 *   Set A4 low
 *     If D5 should be on, set A1 high
 *     If D2 should be on, set A5 high
 * State 3:
 *   Set A0, A4, A5 outputs, A1 input
 *   Set A5 low
 *     If D1 should be on, set A4 high
 *     If D7 should be on, set A0 high
 */

#define TRISA_LEDS_ALL_OUTUPT 0xCC
#define PORTA_LEDS_ALL_LOW    0x00

#define LED_R_BLUE        0x01  // D1 State 3 A4 high
#define LED_R_YELLOW      0x02  // D2 State 2 A5 high
#define LED_R_RED         0x04  // D3 State 1 A0 high
#define LED_R_GREEN       0x08  // D4 State 0 A1 high
#define LED_L_RED         0x10  // D5 State 2 A1 high
#define LED_L_GREEN       0x20  // D6 State 1 A4 high
#define LED_L_BLUE        0x40  // D7 State 3 A0 high
#define LED_L_YELLOW      0x80  // D8 State 0 A5 high

#define LED_R_BLUE_BIT    0x10  //0b00010000
#define LED_R_YELLOW_BIT  0x20  //0b00100000
#define LED_R_RED_BIT     0x01  //0b00000001
#define LED_R_GREEN_BIT   0x02  //0b00000010
#define LED_L_RED_BIT     0x02  //0b00000010
#define LED_L_GREEN_BIT   0x10  //0b00010000
#define LED_L_BLUE_BIT    0x01  //0b00000001
#define LED_L_YELLOW_BIT  0x20  //0b00100000

#define STATE_0_TRIS      0xDC  //0b11011100
#define STATE_1_TRIS      0xEC  //0b11101100
#define STATE_2_TRIS      0xCD  //0b11001101
#define STATE_3_TRIS      0xCE  //0b11001110

typedef enum
{
  LED_STATE_0,
  LED_STATE_1,
  LED_STATE_2,
  LED_STATE_3
} LEDStates_t;

LEDStates_t LEDState = LED_STATE_0;

volatile uint8_t LEDOns = 0;

volatile uint16_t LeftTime = 0;
volatile uint16_t RightTime = 0;

void SetLEDOn(uint8_t LED)
{
  LEDOns = (uint8_t)(LEDOns | LED);
}

void SetLEDOff(uint8_t LED)
{
  LEDOns = (uint8_t)(LEDOns & ~LED);
}

void SetAllLEDsOff(void)
{
  LEDOns = 0;
}

void CharlieplexLEDs(void)
{
  uint8_t PortTemp = 0;
  
  if (LeftTime)
  {
    LeftTime--;
  }
  if (RightTime)
  {
    RightTime--;
  }
  
  if (LEDOns)
  {
    switch (LEDState)
    {
      case LED_STATE_0:   // A0 low, A1 or A5 high
        TRISA = STATE_0_TRIS;
        if (LEDOns & LED_R_GREEN)
        {
          PortTemp = LED_R_GREEN_BIT;
        }
        if (LEDOns & LED_L_YELLOW)
        {
          PortTemp |= LED_L_YELLOW_BIT;
        }
        break;

      case LED_STATE_1: // A1 low, A0 or A4 high
        TRISA = STATE_1_TRIS;
        if (LEDOns & LED_R_RED)
        {
          PortTemp = LED_R_RED_BIT;
        }
        if (LEDOns & LED_L_GREEN)
        {
          PortTemp |= LED_L_GREEN_BIT;
        }
        break;

      case LED_STATE_2: // A4 low, A3 or A5 high
        TRISA = STATE_2_TRIS;
        if (LEDOns & LED_R_YELLOW)
        {
          PortTemp = LED_R_YELLOW_BIT;
        }
        if (LEDOns & LED_L_RED)
        {
          PortTemp |= LED_L_RED_BIT;
        }
        break;

      case LED_STATE_3: // A5 low, A4 or A0 high
        TRISA = STATE_3_TRIS;
        if (LEDOns & LED_R_BLUE)
        {
          PortTemp = LED_R_BLUE_BIT;
        }
        if (LEDOns & LED_L_BLUE)
        {
          PortTemp |= LED_L_BLUE_BIT;
        }
        break;

      default:
        break;
    }
    PORTA = PortTemp;
    
    LEDState++;
    if (LEDState > LED_STATE_3)
    {
      LEDState = LED_STATE_0;
    }
  }
  else
  {
    // If no LEDs are supposed to be on, then set all pins to be outputs low
    TRISA = TRISA_LEDS_ALL_OUTUPT;
    PORTA = PORTA_LEDS_ALL_LOW;
  }
}


/*
                         Main application
 */
void main(void)
{
  // initialize the device
  SYSTEM_Initialize();

  TMR0_SetInterruptHandler(CharlieplexLEDs);

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
  
  uint8_t LeftState = 0;
  uint8_t RightState = 0;
  uint8_t LastLeftButtonState = 0;
  uint8_t LastRightButtonState = 0;
  
  while (1)
  {
    while (LeftState || RightState)
    {
      switch(RightState)
      {
        case 0:
          // Do nothing, this side off
          break;

        case 1:
          SetLEDOn(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          SetLEDOff(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          RightTime = SLOW_DELAY;
          RightState++;
          break;

        case 2:
          if (RightTime == 0)
          {
            SetLEDOff(LED_R_BLUE);
            SetLEDOn(LED_R_YELLOW);
            SetLEDOff(LED_R_RED);
            SetLEDOff(LED_R_GREEN);
            RightTime = SLOW_DELAY;
            RightState++;
          }
          break;

        case 3:
          if (RightTime == 0)
          {
            SetLEDOff(LED_R_BLUE);
            SetLEDOff(LED_R_YELLOW);
            SetLEDOn(LED_R_RED);
            SetLEDOff(LED_R_GREEN);
            RightTime = SLOW_DELAY;
            RightState++;
          }
          break;

        case 4:
          if (RightTime == 0)
          {
            SetLEDOff(LED_R_BLUE);
            SetLEDOff(LED_R_YELLOW);
            SetLEDOff(LED_R_RED);
            SetLEDOn(LED_R_GREEN);
            RightTime = SLOW_DELAY;
            RightState++;
          }
          break;

        case 5:
          if (RightTime == 0)
          {
            SetLEDOff(LED_R_BLUE);
            SetLEDOff(LED_R_YELLOW);
            SetLEDOff(LED_R_RED);
            SetLEDOff(LED_R_GREEN);
            RightState = 0;
          }
          break;

        default:
          break;
      }

      switch(LeftState)
      {
        case 0:
          // Do nothing, this side off
          break;

        case 1:
          SetLEDOn(LED_L_BLUE);
          SetLEDOff(LED_L_YELLOW);
          SetLEDOff(LED_L_RED);
          SetLEDOff(LED_L_GREEN);
          LeftTime = SLOW_DELAY;
          LeftState++;
          break;

        case 2:
          if (LeftTime == 0)
          {
            SetLEDOff(LED_L_BLUE);
            SetLEDOn(LED_L_YELLOW);
            SetLEDOff(LED_L_RED);
            SetLEDOff(LED_L_GREEN);
            LeftTime = SLOW_DELAY;
            LeftState++;
          }
          break;

        case 3:
          if (LeftTime == 0)
          {
            SetLEDOff(LED_L_BLUE);
            SetLEDOff(LED_L_YELLOW);
            SetLEDOn(LED_L_RED);
            SetLEDOff(LED_L_GREEN);
            LeftTime = SLOW_DELAY;
            LeftState++;
          }
          break;

        case 4:
          if (LeftTime == 0)
          {
            SetLEDOff(LED_L_BLUE);
            SetLEDOff(LED_L_YELLOW);
            SetLEDOff(LED_L_RED);
            SetLEDOn(LED_L_GREEN);
            LeftTime = SLOW_DELAY;
            LeftState++;
          }
          break;

        case 5:
          if (LeftTime == 0)
          {
            SetLEDOff(LED_L_BLUE);
            SetLEDOff(LED_L_YELLOW);
            SetLEDOff(LED_L_RED);
            SetLEDOff(LED_L_GREEN);
            LeftState = 0;
          }
          break;

        default:
          break;
      }

      // Check left button for press
      if (PORTAbits.RA2 == 0)
      {
        if (LastLeftButtonState == 0)
        {
          LeftState = 1;
        }
        LastLeftButtonState = 1;
      }
      else
      {
        LastLeftButtonState = 0;
      }
      
      // Check right button for press
      if (PORTAbits.RA3 == 0)
      {
        if (LastRightButtonState == 0)
        {
          RightState = 1;
        }
        LastRightButtonState = 1;
      }
      else
      {
        LastRightButtonState = 0;
      }
    }
    
    // Hit the VREGPM bit to put us in low power sleep mode
    VREGCONbits.VREGPM = 1;

    SetAllLEDsOff();
    
    __delay_ms(50);
    
    SLEEP();
    
    LeftTime = 0;
    RightTime = 0;

    if (PORTAbits.RA2 == 0)
    {
      LeftState = 1;
    }
    if (PORTAbits.RA3 == 0)
    {
      RightState = 1;
    }
    
  }
}
/**
 End of File
*/