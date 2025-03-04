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
// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
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
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include "definitions.h"                // SYS function prototypes

#define LED_ON()                        LED_Clear()
#define LED_OFF()                       LED_Set()

#define READ_SIZE                        (10)

#define UART1_TRANSMIT_ADDRESS          (&UART1_REGS->UART_THR)
#define UART1_RECEIVE_ADDRESS           (&UART1_REGS->UART_RHR)

/* Place the buffers in TCM (Tightly Coupled Memory), which is non-cacheable.
 * This demo configures DTCM for 0x8000(32kb) length in project properties.
 * Total 64k memory is reserved (including ITCM of same length), however this
 * demo enables only DTCM.*/

static char __attribute__ ((tcm)) writeBuffer[] = "**** Data TCM Demo \
****\r\n**** Demo uses TCM memory to handle cache coherency \
****\r\n**** Type a buffer of 10 characters and observe it echo back \
****\r\n**** LED toggles on each time buffer is echoed ****\r\n";
static char __attribute__ ((tcm)) readBuffer[READ_SIZE] = {};
static char __attribute__ ((tcm)) echoBuffer[READ_SIZE + 3] = {};

/* Application Status Data */
static char failureMessage[] = "\r\n\r\n**** Data transfer error ****\r\n";
static volatile bool errorStatus = false;
static volatile bool writeStatus = false;
static volatile bool readStatus = false;

void XDMAC_Callback(XDMAC_TRANSFER_EVENT status, uintptr_t context)
{
    if(status == XDMAC_TRANSFER_COMPLETE)
    {
        if(context == 0)
        {
            writeStatus = true;
        }
        else if(context == 1)
        {
            readStatus = true;
        }
    }
    else
    {
        errorStatus = true;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    LED_OFF();

    /* Register callback functions for both write and read contexts */
    XDMAC_ChannelCallbackRegister(XDMAC_CHANNEL_0, XDMAC_Callback, 0);
    XDMAC_ChannelCallbackRegister(XDMAC_CHANNEL_1, XDMAC_Callback, 1);

    XDMAC_ChannelTransfer(XDMAC_CHANNEL_0, writeBuffer, (const void *)UART1_TRANSMIT_ADDRESS, sizeof(writeBuffer));
        
    while ( true )
    {
        if(errorStatus == true)
        {
            /* Send error message to console.
             * Using USART directly to since DMA is in error condition */
            errorStatus = false;
            UART1_Write(failureMessage, sizeof(failureMessage));
        }
        else if(readStatus == true)
        {
            /* Echo back received buffer and Toggle LED */
            readStatus = false;
            
            memcpy(echoBuffer, readBuffer, READ_SIZE);
            echoBuffer[READ_SIZE] = '\r';
            echoBuffer[(READ_SIZE+1)] = '\n';            

            XDMAC_ChannelTransfer(XDMAC_CHANNEL_0, echoBuffer, (const void *)UART1_TRANSMIT_ADDRESS, (READ_SIZE + 2));
            LED_Toggle();
        }
        else if(writeStatus == true)
        {
            /* Submit buffer to read user data */
            writeStatus = false;
            XDMAC_ChannelTransfer(XDMAC_CHANNEL_1, (const void *)UART1_RECEIVE_ADDRESS, readBuffer, READ_SIZE);
        }
        else
        {
            /* Repeat the loop */
            ;
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

