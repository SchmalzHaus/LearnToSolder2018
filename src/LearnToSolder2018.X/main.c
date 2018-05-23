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
 * 5/22/18 1.0 Finished major features (see below) except for menu.
 * 
 * Ideas:
 *   - Add wake timer : force sleep if system has been awake for too long, even
 *       if one or both buttons is still down (done)
 *   - Add simple button debounce logic (quick, for rapid button presses) (done)
 *   - Primary Display pattern :
 *       On button push, start single LED march along right or left side
 *       (based on which button was pushed) r, g, b, y, then back b, g, r
 *       at modest speed. As long as button is held down, decrease time on each
 *       led. Do this in a logarithmic way so that they don't get all of a
 *       sudden fast. When all LEDs look like their completely solid on, then 
 *       start blinking all four at once, slow at first, then faster and faster.
 *       Each side completely independently controlled. Pattern repeats over
 *       and over. (done)
 *   - Secondary Display patten : If left button held down, while right button
 *       tapped quickly 4 times, switch to secondary display. For secondary
 *       display, light up more and more LEDs from right to left as the right
 *       button is tapped faster and faster. If all LEDs get lit up, signal a
 *       'win' by blinking all LEDs 5 times rapidly. (done)
 *   - Menu : If left button is quickly tapped 4 times while right button 
 *       pressed, go to 'menu' mode. This is where just one LED is lit at a time
 *       starting at the right. Each press of the left button moves the lit
 *       LED to the left as long as the right button is held down. When the right
 *       button is released, then that 'slot' (or 'program') gets run.
 *   - Slot 1: (Right red)
 *   - Slot 2: (Right green)
 */

#include "mcc_generated_files/mcc.h"

// Starting time, in ms, between switching which LED is currently on in main pattern
#define SLOW_DELAY          250

// Maximum number of patterns allowed in the battery array
#define NUMBER_OF_PATTERNS    8

// Button debounce time in milliseconds
#define BUTTON_DEBOUNCE_MS   20

// Number of milliseconds to stay awake for before sleeping just to see if another
// button will be pressed
#define SHUTDOWN_DELAY_MS   100

// Time between two button presses below which is considered 'short'
#define QUICK_PRESS_MS      250

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
#define PATTERN_RIGHT_GAME    2

// Maximum number of milliseconds to allow system to run
#define MAX_AWAKE_TIME_MS     (5UL * 60UL * 1000UL)

// The five states a button can be in (for debouncing))
typedef enum {
    BUTTON_STATE_IDLE = 0,
    BUTTON_STATE_PRESSED_TIMING,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASED_TIMING,
    BUTTON_STATE_RELEASED
} ButtonState_t;

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

// Counts down from SHUTDOWN_DELAY_MS after everything is over before we go to sleep
volatile static uint8_t ShutdownDelayTimer = 0;

// Countdown 1ms timers to  debounce the button inputs
volatile static uint8_t LeftDebounceTimer = 0;
volatile static uint8_t RightDebounceTimer = 0;

// Keep track of the state of each button during debounce
volatile static ButtonState_t LeftButtonState = BUTTON_STATE_IDLE;
volatile static ButtonState_t RightButtonState = BUTTON_STATE_IDLE;

// Record the last value of WakeTimer when the button was pushed
volatile static uint32_t LastRightButtonPressTime = 0;
volatile static uint32_t LastLeftButtonPressTime = 0;

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

/* This ISR runs every 125 uS. It takes the values in LEDState and lights
 * up the LEDs appropriately.
 * It also handles a number of software timer decrementing every 1ms.
 */
void TMR0_Callback(void)
{
  uint8_t i;
    
  // Default all LEDs to be off
  TRISA = TRISA_LEDS_ALL_OUTUPT;
  PORTA = PORTA_LEDS_ALL_LOW;

  // Create local bit pattern to test for what LED we should be thinking about
  i = (uint8_t)(1 << LEDState);
  
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

    // Decrement button debounce timers
    if (LeftDebounceTimer)
    {
        LeftDebounceTimer--;
    }

    if (RightDebounceTimer)
    {
        RightDebounceTimer--;
    }
    
    if (ShutdownDelayTimer)
    {
      ShutdownDelayTimer--;
    }
  }
}

// Return the raw state of the right button input
bool RightButtonPressedRaw(void)
{
  return (uint8_t)(PORTAbits.RA3 == 0);
}

// Return the raw state of the left button input
bool LeftButtonPressedRaw(void)
{
  return (uint8_t)(PORTAbits.RA2 == 0);
}

// Return the logical (debounced) state of the right button
bool RightButtonPressed(void)
{
    return (RightButtonState == BUTTON_STATE_PRESSED);
}

// Return the logical (debounced) state of the left button
bool LeftButtonPressed(void)
{
    return (LeftButtonState == BUTTON_STATE_PRESSED);
}

void RunRightFlash(void)
{
  static uint16_t right_delay = SLOW_DELAY;

  if (PatternDelay[PATTERN_RIGHT_FLASH] == 0)
  {
    switch(PatternState[PATTERN_RIGHT_FLASH])
    {
      case 0:
        // Do nothing, this pattern inactive
        right_delay = SLOW_DELAY;
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
        SetLEDOn(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        break;

      case 6:
        SetLEDOff(LED_R_RED);
        SetLEDOn(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        break;

      case 7:
        SetLEDOn(LED_R_RED);
        SetLEDOff(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        break;

      case 8:
        SetLEDOn(LED_R_RED);
        SetLEDOn(LED_R_GREEN);
        SetLEDOn(LED_R_BLUE);
        SetLEDOn(LED_R_YELLOW);
        break;

      case 9:
        SetLEDOff(LED_R_RED);
        SetLEDOff(LED_R_GREEN);
        SetLEDOff(LED_R_BLUE);
        SetLEDOff(LED_R_YELLOW);
        break;

      case 10:
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
      // When we get to state 7, there is a decision to make
      if (PatternState[PATTERN_RIGHT_FLASH] == 7)
      {
        // If the right button is still held down
        if (RightButtonPressed())
        {
          // Then keep going with the pattern
          if (right_delay > 3)
          {
            // If we're not yet going super fast, decrease our delay and
            // start over at state 2
            right_delay = ((right_delay * 80)/100);
            PatternState[PATTERN_RIGHT_FLASH] = 2;
          }
          else
          {
            // If we're already going super fast, then jump to state 8
            // and slow things down
            PatternState[PATTERN_RIGHT_FLASH] = 8;
            right_delay = SLOW_DELAY;
          }
        }
        else
        {
          // Button not pressed, so stop the pattern by shutting off all the LEDs
          PatternState[PATTERN_RIGHT_FLASH] = 10;
        }
      }
      // Now, if we get to state 9 and the button is still pressed
      else if ((PatternState[PATTERN_RIGHT_FLASH] == 9) && RightButtonPressed())
      {
        // Then see if we're not yet going super fast
        if (right_delay > 10)
        {
          // And go a bit faster, jumping back to state 8
          right_delay = ((right_delay * 95)/100);
          PatternState[PATTERN_RIGHT_FLASH] = 8;
        }
        else
        {
          // We're already going super fast, so jump back to state 1 to restart
          // the whole pattern over, nice and slow.
          PatternState[PATTERN_RIGHT_FLASH] = 1;
          right_delay = SLOW_DELAY;
        }
      }
      else
      {
        // If none of the above applies, then just march on to the next state
        PatternState[PATTERN_RIGHT_FLASH]++;
      }
      PatternDelay[PATTERN_RIGHT_FLASH] = right_delay;
    }
  }
}

void RunLeftFlash(void)
{
  static uint16_t left_delay = SLOW_DELAY;
  
  if (PatternDelay[PATTERN_LEFT_FLASH] == 0)
  {
    switch(PatternState[PATTERN_LEFT_FLASH])
    {
      case 0:
        // Do nothing, this pattern inactive
        left_delay = SLOW_DELAY;
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
        SetLEDOn(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 6:
        SetLEDOff(LED_L_RED);
        SetLEDOn(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 7:
        SetLEDOn(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 8:
        SetLEDOn(LED_L_RED);
        SetLEDOn(LED_L_GREEN);
        SetLEDOn(LED_L_BLUE);
        SetLEDOn(LED_L_YELLOW);
        break;

      case 9:
        SetLEDOff(LED_L_RED);
        SetLEDOff(LED_L_GREEN);
        SetLEDOff(LED_L_BLUE);
        SetLEDOff(LED_L_YELLOW);
        break;

      case 10:
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
      if (PatternState[PATTERN_LEFT_FLASH] == 7)
      {
        if (LeftButtonPressed())
        {
          if (left_delay > 3)
          {
            left_delay = ((left_delay * 80)/100);
            PatternState[PATTERN_LEFT_FLASH] = 2;
          }
          else
          {
            PatternState[PATTERN_LEFT_FLASH] = 8;
            left_delay = SLOW_DELAY;
          }
        }
        else
        {
          PatternState[PATTERN_LEFT_FLASH] = 10;
        }
      }
      else if ((PatternState[PATTERN_LEFT_FLASH] == 9) && LeftButtonPressed())
      {
        if (left_delay > 10)
        {
          left_delay = ((left_delay * 95)/100);
          PatternState[PATTERN_LEFT_FLASH] = 8;
        }
        else
        {
          PatternState[PATTERN_LEFT_FLASH] = 1;
          left_delay = SLOW_DELAY;
        }
      }
      else
      {
        PatternState[PATTERN_LEFT_FLASH]++;
      }
      PatternDelay[PATTERN_LEFT_FLASH] = left_delay;
    }
  }
}

void RunGame(void)
{
  static uint8_t num_leds_lit = 1;
  static uint32_t last_button_press_time = 0;
  static uint32_t next_decrement_time = 0;
  
  if (PatternDelay[PATTERN_RIGHT_GAME] == 0)
  {
    if (PatternState[PATTERN_RIGHT_GAME])
    {
      switch(num_leds_lit)
      {
        case 0:
          // Do nothing, this pattern inactive
          break;

        case 1:
          SetLEDOn(LED_R_RED);
          SetLEDOff(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          SetLEDOff(LED_L_YELLOW);
          SetLEDOff(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 2:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOff(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          SetLEDOff(LED_L_YELLOW);
          SetLEDOff(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 3:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOff(LED_R_YELLOW);
          SetLEDOff(LED_L_YELLOW);
          SetLEDOff(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 4:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          SetLEDOff(LED_L_YELLOW);
          SetLEDOff(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 5:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          SetLEDOn(LED_L_YELLOW);
          SetLEDOff(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 6:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          SetLEDOn(LED_L_YELLOW);
          SetLEDOn(LED_L_BLUE);
          SetLEDOff(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 7:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          SetLEDOn(LED_L_YELLOW);
          SetLEDOn(LED_L_BLUE);
          SetLEDOn(LED_L_GREEN);
          SetLEDOff(LED_L_RED);
          break;

        case 8:
          SetLEDOn(LED_R_RED);
          SetLEDOn(LED_R_GREEN);
          SetLEDOn(LED_R_BLUE);
          SetLEDOn(LED_R_YELLOW);
          SetLEDOn(LED_L_YELLOW);
          SetLEDOn(LED_L_BLUE);
          SetLEDOn(LED_L_GREEN);
          SetLEDOn(LED_L_RED);
          break;

        default:
          break;
      }
      
      // Detect new button presses and increment LED count if seen
      if (last_button_press_time != LastRightButtonPressTime)
      {
        if (LastRightButtonPressTime < (last_button_press_time + 150))
        {
          num_leds_lit++;
          
          if (num_leds_lit > 8)
          {
            num_leds_lit = 0;
            
            SetLEDOn(0xFF);
            __delay_ms(100);
            SetLEDOff(0xFF);
            __delay_ms(100);
            SetLEDOn(0xFF);
            __delay_ms(100);
            SetLEDOff(0xFF);
            __delay_ms(100);
            SetLEDOn(0xFF);
            __delay_ms(100);
            SetLEDOff(0xFF);
            __delay_ms(100);
            SetLEDOn(0xFF);
            __delay_ms(100);
            SetLEDOff(0xFF);
            __delay_ms(100);
            SetLEDOn(0xFF);
            __delay_ms(100);
            SetLEDOff(0xFF);
            __delay_ms(100);
          }
        }
        last_button_press_time = LastRightButtonPressTime;
      }
      
      // Decrement LED count every so many milliseconds
      if (WakeTimer > next_decrement_time)
      {
        next_decrement_time = WakeTimer + 160;
        if (num_leds_lit)
        {
          num_leds_lit--;
        }
      }
    }
  }    
}

// Return true if either button is currently down (raw)
bool CheckForButtonPushes(void)
{
  static bool LastLeftButtonState = false;
  static bool LastRightButtonState = false;
  static uint8_t LeftButtonQuickPressCount = 0;
  
  // Debounce left button press
  if (LeftButtonPressedRaw())
  {
    if (LeftButtonState == BUTTON_STATE_PRESSED_TIMING)
    {
      if (LeftDebounceTimer == 0)
      {
        LeftButtonState = BUTTON_STATE_PRESSED;
      }
    }
    else if (LeftButtonState != BUTTON_STATE_PRESSED)
    {
      LeftButtonState = BUTTON_STATE_PRESSED_TIMING;
      LeftDebounceTimer = BUTTON_DEBOUNCE_MS;
    }
  }
  else
  {
    if (LeftButtonState == BUTTON_STATE_RELEASED_TIMING)
    {
      if (LeftDebounceTimer == 0)
      {
        LeftButtonState = BUTTON_STATE_RELEASED;
      }
    }
    else if (LeftButtonState != BUTTON_STATE_RELEASED)
    {
      LeftButtonState = BUTTON_STATE_RELEASED_TIMING;
      LeftDebounceTimer = BUTTON_DEBOUNCE_MS;
    }
  }
    
  // Debounce right button  press
  if (RightButtonPressedRaw())
  {
    if (RightButtonState == BUTTON_STATE_PRESSED_TIMING)
    {
      if (RightDebounceTimer == 0)
      {
        RightButtonState = BUTTON_STATE_PRESSED;
      }
    }
    else if (RightButtonState != BUTTON_STATE_PRESSED)
    {
      RightButtonState = BUTTON_STATE_PRESSED_TIMING;
      RightDebounceTimer = BUTTON_DEBOUNCE_MS;
    }
  }
  else
  {
    if (RightButtonState == BUTTON_STATE_RELEASED_TIMING)
    {
      if (RightDebounceTimer == 0)
      {
        RightButtonState = BUTTON_STATE_RELEASED;
      }
    }
    else if (RightButtonState != BUTTON_STATE_RELEASED)
    {
      RightButtonState = BUTTON_STATE_RELEASED_TIMING;
      RightDebounceTimer = BUTTON_DEBOUNCE_MS;
    }
  }

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
      
      // Check for entry into game mode
      if (LeftButtonPressed())
      {
        if (WakeTimer < (LastRightButtonPressTime + QUICK_PRESS_MS))
        {
          LeftButtonQuickPressCount++;

          if (LeftButtonQuickPressCount == 4)
          {
              // Enter into game mode
              PatternState[PATTERN_RIGHT_FLASH] = 0;
              PatternState[PATTERN_LEFT_FLASH] = 0;
              PatternState[PATTERN_RIGHT_GAME] = 1;

//          SetLEDOn(LED_R_RED);
//          SetLEDOn(LED_R_GREEN);
//          SetLEDOn(LED_R_BLUE);
//          SetLEDOn(LED_R_YELLOW);
//          SetLEDOn(LED_L_YELLOW);
//          SetLEDOn(LED_L_BLUE);
//          SetLEDOn(LED_L_GREEN);
//          SetLEDOn(LED_L_RED);

//          __delay_ms(1000);        
          }
        }
        else
        {
            LeftButtonQuickPressCount = 0;
        }
      }
      LastRightButtonPressTime = WakeTimer;
    }
    LastRightButtonState = true;
  }
  else
  {
    LastRightButtonState = false;
  }
  
  return ((bool)(LeftButtonPressedRaw() || RightButtonPressedRaw()));
}

/*
                         Main application
 */
void main(void)
{
  // initialize the device
  SYSTEM_Initialize();

  TMR0_SetInterruptHandler(TMR0_Callback);

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
    RunGame();
    
    APatternIsRunning = false;
    for (i=0; i < 8; i++)
    {
      if (PatternState[i] != 0)
      {
        APatternIsRunning = true;
      }
    }
    if ((!APatternIsRunning && RightDebounceTimer == 0 && LeftDebounceTimer == 0) || (WakeTimer > MAX_AWAKE_TIME_MS))
    {
      SetAllLEDsOff();
      // Allow LEDsOff command to percolate to LEDs
      __delay_ms(5);

      ShutdownDelayTimer = SHUTDOWN_DELAY_MS;

      while (ShutdownDelayTimer && !CheckForButtonPushes())
      {
      }

      if (ShutdownDelayTimer == 0)
      {
          // Hit the VREGPM bit to put us in low power sleep mode
        VREGCONbits.VREGPM = 1;

        SLEEP();

        // Start off with time = 0;
        WakeTimer = 0;
      }
    }

    CheckForButtonPushes();
  }
}
/**
 End of File
*/