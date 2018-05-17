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
 * 
 * 
 * Ideas:
 *   - Add wake timer : force sleep if system has been awake for too long, even
 *       if one or both buttons is still down
 *   - Add simple button debounce logic (quick, for rapid button presses)
 *   - Primary Display pattern :
 *       On button push, start single LED march along right or left side
 *       (based on which button was pushed) r, g, b, y, then back b, g, r
 *       at modest speed. As long as button is held down, decrease time on each
 *       led. Do this in a logarithmic way so that they don't get all of a
 *       sudden fast. When all LEDs look like their completely solid on, then 
 *       start blinking all four at once, slow at first, then faster and faster.
 *       Each side completely independently controlled. Pattern repeats over
 *       and over. 
 *   - Secondary Display patten : If left button held down, while right button
 *       tapped quickly 4 times, switch to secondary display. For secondary
 *       display, light up more and more LEDs from right to left as the right
 *       button is tapped faster and faster. If all LEDs get lit up, signal a
 *       'win' by blinking all LEDs 10 times rapidly.
 *   - Menu : If left button is quickly tapped 4 times while right button 
 *       pressed, go to 'menu' mode. This is where just one LED is lit at a time
 *       starting at the right. Each press of the left button moves the lit
 *       LED to the left as long as the right button is held down. When the right
 *       button is released, then that 'slot' (or 'program') gets run.
 *   - Slot 1: (Right red)
 *   - Slot 2: (Right green)
 */

#include "mcc_generated_files/mcc.h"

#define SLOW_DELAY 1000

#define NUMBER_OF_PATTERNS  8

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

#define PATTERN_OFF_STATE     0 // State for all patterns where they are inactive

// Index for each pattern into the patterns arrays
#define PATTERN_RIGHT_FLASH   0
#define PATTERN_LEFT_FLASH    1

// Maximum number of milliseconds to allow system to run
#define MAX_AWAKE_TIME_MS     (1UL * 60UL * 1000UL)

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
// Counts up from 0 to 7, represents the LED number currently being serviced in the ISR
static uint8_t LEDState = 0;

// Each pattern has a delay counter that counts down at a 1ms rate
volatile uint16_t PatternDelay[NUMBER_OF_PATTERNS];
// Each pattern has a state variable defining what state it is in
volatile uint8_t PatternState[NUMBER_OF_PATTERNS];

// Counts number of milliseconds we are awake for, and puts us to sleep if 
// we stay awake for too long
volatile static uint32_t WakeTimer = 0;

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
  uint8_t i;
    
  // Default all LEDs to be off
  TRISA = TRISA_LEDS_ALL_OUTUPT;
  PORTA = PORTA_LEDS_ALL_LOW;

  // Create local bit pattern to test for what LED we should be thinking about
  i = 1 << LEDState;
  
  // If the bit in LEDOns we're looking at is high (i.e. LED on)
  if (i & LEDOns)
  {
    // Then set the tris and port registers from the tables
    TRISA = TRISTable[LEDState];
    PORTA = PORTTable[LEDState];
  }

  // Always increment state and bit
  LEDState++;
  if (LEDState == 8)
  {
    // Approximately 1ms has passed since last time LEDState was 0, so
    // perform the 1ms tasks

    // Always increment wake timer to count this millisecond
    WakeTimer++;

    // Handle time delays for patterns
    for (i=0; i < 8; i++)
    {
      if (PatternDelay[i])
      {
        PatternDelay[i]--;
      }
    }

    LEDState = 0;
  }
}

bool RightButtonPressed(void)
{
  return (PORTAbits.RA3 == 0);
}

bool LeftButtonPressed(void)
{
  return (PORTAbits.RA2 == 0);
}

void RunRightFlash(void)
{
  if (PatternState[PATTERN_RIGHT_FLASH])
  {
    if (PatternDelay[PATTERN_RIGHT_FLASH] == 0)
    {
      switch(PatternState[PATTERN_RIGHT_FLASH])
      {
        case 0:
          // Do nothing, this pattern inactive
          break;

        case 1:
          SetLEDOn(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          break;

        case 2:
          SetLEDOff(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          break;

        case 3:
          SetLEDOff(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          break;

        case 4:
          SetLEDOff(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          break;

        case 5:
          SetLEDOff(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          PatternState[PATTERN_RIGHT_FLASH] = PATTERN_OFF_STATE;
          break;

        default:
          PatternState[PATTERN_RIGHT_FLASH] = 0;
          break;
      }

      // Move to the next state
      if (PatternState[PATTERN_RIGHT_FLASH] != 0)
      {
        if ((PatternState[PATTERN_RIGHT_FLASH] == 4) && RightButtonPressed())
        {
          PatternState[PATTERN_RIGHT_FLASH] = 1;
        }
        else
        {
          PatternState[PATTERN_RIGHT_FLASH]++;
        }
        PatternDelay[PATTERN_RIGHT_FLASH] = SLOW_DELAY;
      }
    }
  }
}

void RunLeftFlash(void)
{
  static uint16_t delay = SLOW_DELAY;
  
  if (PatternDelay[PATTERN_LEFT_FLASH] == 0)
  {
    switch(PatternState[PATTERN_LEFT_FLASH])
    {
      case 0:
        // Do nothing, this pattern inactive
        delay = SLOW_DELAY;
        break;

      case 1:
        SetLEDOn(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 2:
        SetLEDOff(LED_L_RED);
        SetLEDOn(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 3:
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOn(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 4:
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOn(LED_L_YELLOW);
        break;

      case 5:
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        PatternState[PATTERN_LEFT_FLASH] = PATTERN_OFF_STATE;
        break;

      default:
        PatternState[PATTERN_LEFT_FLASH] = 0;
        break;
    }

    // Move to the next state
    if (PatternState[PATTERN_LEFT_FLASH] != 0)
    {
      if ((PatternState[PATTERN_LEFT_FLASH] == 4) && LeftButtonPressed())
      {
        PatternState[PATTERN_LEFT_FLASH] = 1;
        if (delay > 50)
        {
          delay -= 15;
        }
      }
      else
      {
        PatternState[PATTERN_LEFT_FLASH]++;
      }
      PatternDelay[PATTERN_LEFT_FLASH] = delay;
    }
  }
}

void CheckForButtonPushes(void)
{
  static bool LastLeftButtonState = false;
  static bool LastRightButtonState = false;

  // Check left button for press
  if (LeftButtonPressed())
  {
    if (LastLeftButtonState == false)
    {
      PatternState[PATTERN_LEFT_FLASH] = 1;
    }
    LastLeftButtonState = true;
  }
  else
  {
    LastLeftButtonState = false;
  }

  // Check right button for press
  if (RightButtonPressed())
  {
    if (LastRightButtonState == false)
    {
      PatternState[PATTERN_RIGHT_FLASH] = 1;
    }
    LastRightButtonState = true;
  }
  else
  {
    LastRightButtonState = false;
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
    
  uint8_t i;
  bool APatternIsRunning = false;
  
  while (1)
  {
    RunRightFlash();
    RunLeftFlash();

    APatternIsRunning = false;
    for (i=0; i < 8; i++)
    {
      if (PatternState[i] != 0)
      {
        APatternIsRunning = true;
      }
    }
    if (!APatternIsRunning || (WakeTimer > MAX_AWAKE_TIME_MS))
    {
      // Hit the VREGPM bit to put us in low power sleep mode
      VREGCONbits.VREGPM = 1;

      SetAllLEDsOff();
      // Allow LEDsOff command to percolate to LEDs
      __delay_ms(10);

      SLEEP();

      // Start off with time = 0;
      WakeTimer = 0;
    }

    CheckForButtonPushes();
  }
}
/**
 End of File
*/