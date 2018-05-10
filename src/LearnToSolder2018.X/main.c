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

#define SLOW_DELAY 1000

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
 *  State 0:  Right Red D3
 *    If ON:  A0 H, A1 L, A4 Z, A5 Z
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 1:   Right Green D4
 *    If ON:  A0 L, A1 H, A4 Z, A5 Z
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 2:   Right Blue D1
 *    If ON:  A0 Z, A1 Z, A4 H, A5 L
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 3:   Right Yellow D2
 *    If ON:  A0 Z, A1 Z, A4 L, A5 H
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 4:   Left Yellow  D8
 *    If ON:  A0 L, A1 L, A4 L, A5 L
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 5:   Left Blue    D7
 *    If ON:  A0 H, A1 Z, A4 Z, A5 L
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 6:   Left Green   D6
 *    If ON:  A0 Z, A1 L, A4 H, A5 Z
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 * State 7:   Left Red     D5
 *    If ON:  A0 Z, A1 H, A4 L, A5 Z
 *    If OFF: A0 L, A1 L, A4 L, A5 L
 */

#define TRISA_LEDS_ALL_OUTUPT 0xCC
#define PORTA_LEDS_ALL_LOW    0x00

#define LED_R_RED         0x01  // D3 State 1 A0 high
#define LED_R_GREEN       0x02  // D4 State 0 A1 high
#define LED_R_BLUE        0x04  // D1 State 3 A4 high
#define LED_R_YELLOW      0x08  // D2 State 2 A5 high
#define LED_L_YELLOW      0x10  // D8 State 0 A5 high
#define LED_L_BLUE        0x20  // D7 State 3 A0 high
#define LED_L_GREEN       0x40  // D6 State 1 A4 high
#define LED_L_RED         0x80  // D5 State 2 A1 high

// Bits that define the 8 patterns we can be playing
#define PATTERN_RIGHT_FLASH   0x01
#define PATTERN_LEFT_FLASH    0x02

static uint8_t TRISTable[] =
{
  0xFC,     // Right Red
  0xFC,     // Right Green
  0xCF,     // Right Blue
  0xCF,     // Right Yellow
  0xDE,     // Left Yellow
  0xDE,     // Left Blue
  0xED,     // Left Green
  0xED      // Left Red
};

static uint8_t PORTTable[] =
{
  0x01,     // Right Red
  0x02,     // Right Green
  0x10,     // Right Blue
  0x20,     // Right Yellow
  0x20,     // Left Yellow
  0x01,     // Left Blue
  0x10,     // Left Green
  0x02      // Left Red
};

// Each bit represents an LED. Set high to turn that LED on. Interface from mainline to ISR
static volatile uint8_t LEDOns = 0;
// Counts up from 0 
static uint8_t LEDState = 0;
static volatile uint8_t LEDTestBit = 0x01;
uint8_t PatternRightFlashState = 0;
uint8_t PatternLeftFlashState = 0;
// Bitfield of 8 possible patterns that can be playing
uint8_t Patterns = 0;

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
  // Handle time delays for patterns
  if (LeftTime)
  {
    LeftTime--;
  }
  if (RightTime)
  {
    RightTime--;
  }
  
  // Default all LEDs to be off
  TRISA = TRISA_LEDS_ALL_OUTUPT;
  PORTA = PORTA_LEDS_ALL_LOW;

  // If the bit in LEDOns we're looking at is high (i.e. LED on)
  if (LEDTestBit & LEDOns)
  {
    // Then set the tris and port registers from the tables
    TRISA = TRISTable[LEDState];
    PORTA = PORTTable[LEDState];
  }

  // Always increment state and bit
  LEDState++;
  LEDTestBit = (uint8_t)(LEDTestBit << 1);
  if (LEDState == 8)
  {
    LEDState = 0;
    LEDTestBit = 0x01;
  }
}

void RunRightFlash(void)
{     
  switch(PatternRightFlashState)
  {
    case 0:
      // Do nothing, this side off
      break;

    case 1:
      SetLEDOn(LED_R_RED);
      SetLEDOff(LED_R_GREEN);
      SetLEDOff(LED_R_BLUE);
      SetLEDOff(LED_R_YELLOW);
      RightTime = SLOW_DELAY;
      PatternRightFlashState++;
      break;

    case 2:
      if (RightTime == 0)
      {
        SetLEDOff(LED_R_RED);
        SetLEDOn(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        RightTime = SLOW_DELAY;
        PatternRightFlashState++;
      }
      break;

    case 3:
      if (RightTime == 0)
      {
        SetLEDOff(LED_R_RED);
        SetLEDOff(LED_R_GREEN);
        SetLEDOn(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        RightTime = SLOW_DELAY;
        PatternRightFlashState++;
      }
      break;

    case 4:
      if (RightTime == 0)
      {
        SetLEDOff(LED_R_RED);
        SetLEDOff(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOn(LED_R_YELLOW);
        RightTime = SLOW_DELAY;
        PatternRightFlashState++;
      }
      break;

    case 5:
      if (RightTime == 0)
      {
        SetLEDOff(LED_R_RED);
        SetLEDOff(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        RightTime = SLOW_DELAY;
        PatternRightFlashState = 0;          
        Patterns &= ~PATTERN_RIGHT_FLASH;
      }
      break;

    default:
      PatternRightFlashState = 0;
      break;
  }
}

void RunLeftFlash(void)
{
  switch(PatternLeftFlashState)
  {
    case 0:
      // Do nothing, this side off
      break;

    case 1:
      SetLEDOn(LED_L_RED);
      SetLEDOff(LED_L_GREEN);
      SetLEDOff(LED_L_BLUE);
      SetLEDOff(LED_L_YELLOW);
      LeftTime = SLOW_DELAY;
      PatternLeftFlashState++;
      break;

    case 2:
      if (LeftTime == 0)
      {
        SetLEDOff(LED_L_RED);
        SetLEDOn(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        LeftTime = SLOW_DELAY;
        PatternLeftFlashState++;
      }
      break;

    case 3:
      if (LeftTime == 0)
      {
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOn(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        LeftTime = SLOW_DELAY;
        PatternLeftFlashState++;
      }
      break;

    case 4:
      if (LeftTime == 0)
      {
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOn(LED_L_YELLOW);
        LeftTime = SLOW_DELAY;
        PatternLeftFlashState++;
      }
      break;

    case 5:
      if (LeftTime == 0)
      {
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        PatternLeftFlashState = 0;
        Patterns &= ~PATTERN_LEFT_FLASH;
      }
      break;

    default:
      break;
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
  
  uint8_t LastLeftButtonState = 0;
  uint8_t LastRightButtonState = 0;
  
  while (1)
  {
    while (Patterns)
    {
      if (Patterns & PATTERN_RIGHT_FLASH)
      {
        RunRightFlash();
      }
      if (Patterns & PATTERN_LEFT_FLASH)
      {
        RunLeftFlash();
      }


      // Check left button for press
      if (PORTAbits.RA2 == 0)
      {
        if (LastLeftButtonState == 0)
        {
          Patterns |= PATTERN_LEFT_FLASH;
          PatternLeftFlashState = 1;
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
          Patterns |= PATTERN_RIGHT_FLASH;
          PatternRightFlashState = 1;
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
      Patterns |= PATTERN_LEFT_FLASH;
      PatternLeftFlashState = 1;
    }
    if (PORTAbits.RA3 == 0)
    {
      Patterns |= PATTERN_RIGHT_FLASH;
      PatternRightFlashState = 1;
    }
  }
}
/**
 End of File
*/