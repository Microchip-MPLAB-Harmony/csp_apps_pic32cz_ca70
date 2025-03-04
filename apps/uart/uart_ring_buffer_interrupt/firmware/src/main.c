/*******************************************************************************
* Copyright (C) 2020 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include <stdio.h>
#include "definitions.h"                // SYS function prototypes

uint8_t txBuffer[50];
uint8_t rxBuffer[10];
static volatile uint32_t nBytesRead = 0;
volatile bool txThresholdEventReceived = false;
volatile bool rxThresholdEventReceived = false;

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

void usartReadEventHandler(UART_EVENT event, uintptr_t context )
{
    uint32_t nBytesAvailable = 0;
    
    if (event == UART_EVENT_READ_THRESHOLD_REACHED)
    {
        /* Receiver should atleast have the thershold number of bytes in the ring buffer */
        nBytesAvailable = UART1_ReadCountGet();
        
        nBytesRead += UART1_Read((uint8_t*)&rxBuffer[nBytesRead], nBytesAvailable);                          
    }
}

void usartWriteEventHandler(UART_EVENT event, uintptr_t context )
{
    txThresholdEventReceived = true;
}

int main ( void )
{
    uint32_t nBytes = 0;        
    
    /* Initialize all modules */
    SYS_Initialize ( NULL );          
    
    /* Register a callback for write events */
    UART1_WriteCallbackRegister(usartWriteEventHandler, (uintptr_t) NULL);
    
    /* Register a callback for read events */
    UART1_ReadCallbackRegister(usartReadEventHandler, (uintptr_t) NULL);              
    
    /* Print the size of the read buffer on the terminal */
    nBytes = sprintf((char*)txBuffer, "RX Buffer Size = %d\r\n", (int)UART1_ReadBufferSizeGet());
    
    UART1_Write((uint8_t*)txBuffer, nBytes);  
    
    /* Print the size of the write buffer on the terminal */
    nBytes = sprintf((char*)txBuffer, "TX Buffer Size = %d\r\n", (int)UART1_WriteBufferSizeGet());
    
    UART1_Write((uint8_t*)txBuffer, nBytes);    
    
    UART1_Write((uint8_t*)"Adding 10 characters to the TX buffer - ", sizeof("Adding 10 characters to the TX buffer - "));    
    
    /* Wait for all bytes to be transmitted out */
    while (UART1_WriteCountGet() != 0);    
    
    UART1_Write((uint8_t*)"0123456789", 10);           
        
    /* Print the amount of free space available in the TX buffer. This should be 10 bytes less than the configured write buffer size. */
    nBytes = sprintf((char*)txBuffer, "\r\nFree Space in Transmit Buffer = %d\r\n", (int)UART1_WriteFreeBufferCountGet());

    UART1_Write((uint8_t*)txBuffer, nBytes);    
    
    /* Let's enable notifications to get notified when the TX buffer is empty */
    UART1_WriteThresholdSet(UART1_WriteBufferSizeGet());   
    
    /* Enable notifications */
    UART1_WriteNotificationEnable(true, false);
   
    /* Wait for the TX buffer to become empty. Flag "txThresholdEventReceived" is set in the callback. */
    while (txThresholdEventReceived == false);
    
    txThresholdEventReceived = false;    
    
    /* Disable TX notifications */
    UART1_WriteNotificationEnable(false, false);
    
    UART1_Write((uint8_t*)"Enter 10 characters. The received characters are echoed back. \r\n>", sizeof("Enter 10 characters. The received characters are echoed back. \r\n>"));               
            
    /* Wait till 10 (or more) characters are received */
    while (UART1_ReadCountGet() < 10);
    
    /* At-least 10 characters are available in the RX buffer. Read out into the application buffer */
    UART1_Read((uint8_t*)rxBuffer, 10);  
    
    /* Echo the received data */
    UART1_Write((uint8_t*)rxBuffer, 10);    
    
    /* Now demonstrating receiver notifications */
    UART1_Write((uint8_t*)"\r\n Now turning on RX notifications \r\n>", sizeof("\r\n Now turning on RX notifications \r\n>"));
    
    /* For demonstration purpose, set a threshold value to receive a callback after every 5 characters are received */
    UART1_ReadThresholdSet(5);
    
    /* Enable RX event notifications */
    UART1_ReadNotificationEnable(true, false);
                   
    while(1)
    {
        /* Wait until at-least 10 characters are entered by the user */
        while (nBytesRead < 10);    
    
        /* Echo the received data */
        UART1_Write((uint8_t*)rxBuffer, nBytesRead);
        
        UART1_Write((uint8_t*)"\r\n>", 3);

        nBytesRead = 0;
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

