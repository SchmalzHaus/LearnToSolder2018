/**
  Generated Pin Manager File

  Company:
    Microchip Technology Inc.

  File Name:
    pin_manager.c

  Summary:
    This is the Pin Manager file generated using MPLAB(c) Code Configurator

  Description:
    This header file provides implementations for pin APIs for all pins selected in the GUI.
    Generation Information :
        Product Revision  :  MPLAB(c) Code Configurator - 4.35
        Device            :  PIC12F1572
        Driver Version    :  1.02
    The generated drivers are tested against the following:
        Compiler          :  XC8 1.35
        MPLAB             :  MPLAB X 3.40

    Copyright (c) 2013 - 2015 released Microchip Technology Inc.  All rights reserved.

    Microchip licenses to you the right to use, modify, copy and distribute
    Software only when embedded on a Microchip microcontroller or digital signal
    controller that is integrated into your product or third party product
    (pursuant to the sublicense terms in the accompanying license agreement).

    You should refer to the license agreement accompanying this Software for
    additional information regarding your rights and obligations.

    SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
    EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
    MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
    IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
    CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
    OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
    INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
    CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
    SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
    (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

*/

#include <xc.h>
#include "pin_manager.h"
#include "stdbool.h"


void (*IOCAF2_InterruptHandler)(void);
void (*IOCAF3_InterruptHandler)(void);


void PIN_MANAGER_Initialize(void)
{
    /**
    LATx registers
    */   
    LATA = 0x00;    

    /**
    TRISx registers
    */    
    TRISA = 0x3F;

    /**
    ANSELx registers
    */   
    ANSELA = 0x13;

    /**
    WPUx registers
    */ 
    WPUA = 0x0C;
    OPTION_REGbits.nWPUEN = 0;

    /**
    ODx registers
    */   
    ODCONA = 0x00;
    
    /**
    APFCONx registers
    */
    APFCON = 0x00;

    /**
    IOCx registers
    */
    // interrupt on change for group IOCAF - flag
    IOCAFbits.IOCAF2 = 0;
    IOCAFbits.IOCAF3 = 0;
    // interrupt on change for group IOCAN - negative
    IOCANbits.IOCAN2 = 1;
    IOCANbits.IOCAN3 = 1;
    // interrupt on change for group IOCAP - positive
    IOCAPbits.IOCAP2 = 1;
    IOCAPbits.IOCAP3 = 1;

    // register default IOC callback functions at runtime; use these methods to register a custom function
    IOCAF2_SetInterruptHandler(IOCAF2_DefaultInterruptHandler);
    IOCAF3_SetInterruptHandler(IOCAF3_DefaultInterruptHandler);
   
    // Enable IOCI interrupt 
    INTCONbits.IOCIE = 1; 
    
}       

void PIN_MANAGER_IOC(void)
{   
    // interrupt on change for pin IOCAF2
    if(IOCAFbits.IOCAF2 == 1)
    {
        IOCAF2_ISR();  
    }                          

    // interrupt on change for pin IOCAF3
    if(IOCAFbits.IOCAF3 == 1)
    {
        IOCAF3_ISR();  
    }                          


}

/**
   IOCAF2 Interrupt Service Routine
*/
void IOCAF2_ISR(void) {

    // Add custom IOCAF2 code

    // Call the interrupt handler for the callback registered at runtime
    if(IOCAF2_InterruptHandler)
    {
        IOCAF2_InterruptHandler();
    }
    IOCAFbits.IOCAF2 = 0;
}

/**
  Allows selecting an interrupt handler for IOCAF2 at application runtime
*/
void IOCAF2_SetInterruptHandler(void (* InterruptHandler)(void)){
    IOCAF2_InterruptHandler = InterruptHandler;
}

/**
  Default interrupt handler for IOCAF2
*/
void IOCAF2_DefaultInterruptHandler(void){
    // add your IOCAF2 interrupt custom code
    // or set custom function using IOCAF2_SetInterruptHandler()
}

/**
   IOCAF3 Interrupt Service Routine
*/
void IOCAF3_ISR(void) {

    // Add custom IOCAF3 code

    // Call the interrupt handler for the callback registered at runtime
    if(IOCAF3_InterruptHandler)
    {
        IOCAF3_InterruptHandler();
    }
    IOCAFbits.IOCAF3 = 0;
}

/**
  Allows selecting an interrupt handler for IOCAF3 at application runtime
*/
void IOCAF3_SetInterruptHandler(void (* InterruptHandler)(void)){
    IOCAF3_InterruptHandler = InterruptHandler;
}

/**
  Default interrupt handler for IOCAF3
*/
void IOCAF3_DefaultInterruptHandler(void){
    // add your IOCAF3 interrupt custom code
    // or set custom function using IOCAF3_SetInterruptHandler()
}

/**
 End of File
*/