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

//DOM-IGNORE-BEGIN
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
//DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>                     // Defines memset, memcmp
#include <stdio.h>                      // Defines printf
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#include <intrinsics.h>                 // Defines __get_LR
#endif
#include "definitions.h"                // SYS function prototypes

// *****************************************************************************
// *****************************************************************************
// Section: Application Definitions and Types (from app.h)
// *****************************************************************************
// *****************************************************************************

#define PAGE_SIZE                  (256U)
#define ERASE_SIZE                 (4096U)

#define SST26VF032B_JEDEC_ID       (0x004226BFUL)

#define SET_IOC_BIT                (1 << 1)

#define LED_ON                     LED_Clear
#define LED_OFF                    LED_Set
#define LED_TOGGLE                 LED_Toggle

// *****************************************************************************
/* Application states

  Summary:
    Application states enumeration

  Description:
    This enumeration defines the valid application states.  These states
    determine the behavior of the application at various times.
*/

typedef enum
{
    /* The app mounts the disk */
    APP_STATE_INIT = 0,

    /* Reset Flash*/
    APP_STATE_RESET_FLASH,

    /* Enable Quad IO Mode*/
    APP_STATE_ENABLE_QUAD_IO,

    /* Unlock Flash*/
    APP_STATE_UNLOCK_FLASH,

    /* Read JEDEC ID*/
    APP_STATE_READ_JEDEC_ID,

    /* Erase Flash */
    APP_STATE_ERASE_FLASH,

    /* Erase Wait */
    APP_STATE_ERASE_WAIT,

    /* Write to Memory */
    APP_STATE_WRITE_MEMORY,

    /* Erase Wait */
    APP_STATE_WRITE_WAIT,

    /* Read From Memory */
    APP_STATE_READ_MEMORY,

    /* Read and verify data in continuous mode*/
    APP_STATE_READ_CONTINUOUS,

    /* Verify Data Read */
    APP_STATE_VERIFY_DATA,

    /* The app idles */
    APP_STATE_SUCCESS,

    /* An app error has occurred */
    APP_STATE_ERROR

} APP_STATES;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    Application strings and buffers are be defined outside this structure.
 */

typedef struct
{
    /* Application's current state */
    APP_STATES state;

    /* Application transfer status */
    volatile bool xfer_done;

    /* Jedec-ID*/
    uint32_t jedec_id;
} APP_DATA;

/* SST26 Command set

  Summary:
    Enumeration listing the SST26VF commands.

  Description:
    This enumeration defines the commands used to interact with the SST26VF
    series of devices.

  Remarks:
    None
*/

typedef enum
{
    /* Reset enable command. */
    SST26_CMD_FLASH_RESET_ENABLE = 0x66,

    /* Command to reset the flash. */
    SST26_CMD_FLASH_RESET        = 0x99,

    /* Command to Enable QUAD IO */
    SST26_CMD_ENABLE_QUAD_IO     = 0x38,

    /* Command to Reset QUAD IO */
    SST26_CMD_RESET_QUAD_IO      = 0xFF,

    /* Command to read JEDEC-ID of the flash device. */
    SST26_CMD_JEDEC_ID_READ      = 0x9F,

    /* QUAD Command to read JEDEC-ID of the flash device. */
    SST26_CMD_QUAD_JEDEC_ID_READ = 0xAF,

    /* Command to perform High Speed Read */
    SST26_CMD_HIGH_SPEED_READ    = 0x0B,

    /* Command to perform Quad IO Read - XIP */
    SST26_CMD_QUADIO_READ        = 0xEB,

    /* Write enable command. */
    SST26_CMD_WRITE_ENABLE       = 0x06,

    /* Page Program command. */
    SST26_CMD_PAGE_PROGRAM       = 0x02,

    /* Command to read the Flash status register. */
    SST26_CMD_READ_STATUS_REG    = 0x05,

    /* Command to read the Flash status register. */
    SST26_CMD_READ_CONFIG_REG    = 0x35,

    /* Command to read the Flash status register. */
    SST26_CMD_WRITE_CONFIG_REG   = 0x01,

    /* Command to perform sector erase */
    SST26_CMD_SECTOR_ERASE       = 0x20,

    /* Command to perform Bulk erase */
    SST26_CMD_BULK_ERASE_64K     = 0xD8,

    /* Command to perform Chip erase */
    SST26_CMD_CHIP_ERASE         = 0xC7,

    /* Command to unlock the flash device. */
    SST26_CMD_UNPROTECT_GLOBAL   = 0x98

} SST26_CMD;

typedef enum
{
    /* Buffer is being processed */
    APP_TRANSFER_BUSY,
    /* APP Buffer transfer is successfully completed*/
    APP_TRANSFER_COMPLETED,
    /* APP Buffer transfer had error*/
    APP_TRANSFER_ERROR_UNKNOWN,
    /* APP Transfer has not been asked yet */
    APP_TRANSFER_IDLE
} APP_TRANSFER_STATUS;

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions (from app.c)
// *****************************************************************************
// *****************************************************************************

static APP_DATA appData;

/* Linker-defined symbols/sections for the QSPI Region.
 *
 * XC32 (GNU ld): the custom linker script places ".code_in_qspi" with an
 * execution address (VMA) in the "qspi" memory region and a load address
 * (LMA) in "rom" right after "_etext". _start_qspi/_end_qspi mark the
 * execution address range; _etext marks the load address of the section's
 * initial image in internal Flash.
 *
 * IAR (ILINK): the ".code_in_qspi" block is placed in QSPI_region, and
 * "initialize manually by copy" makes the linker generate a load image of
 * that same block in ROM_region -- as ".code_in_qspi_init" -- without any
 * automatic startup copy, mirroring the XC32 VMA/LMA split above.
 */
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#pragma section = ".code_in_qspi"
#pragma section = ".code_in_qspi_init"
#else
extern uint32_t _start_qspi, _end_qspi, _etext;
#endif

static uint32_t erase_index = 0;
static uint32_t write_index = 0;
static uint32_t qspi_start, qspi_end;
static uint8_t *qspi_loadMemAddr;
static uint32_t qspi_length;
static uint8_t *verify_qspiMemory = NULL;

static qspi_command_xfer_t qspi_command_xfer = { 0 };
static qspi_register_xfer_t qspi_register_xfer = { 0 };
static qspi_memory_xfer_t qspi_memory_xfer = { 0 };

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions (from app.c)
// *****************************************************************************
// *****************************************************************************

/* This function resets the flash by sending down the reset enable command
 * followed by the reset command. */

static APP_TRANSFER_STATUS APP_ResetFlash(void)
{
    memset((void *)&qspi_command_xfer, 0, sizeof(qspi_command_xfer_t));

    qspi_command_xfer.instruction = SST26_CMD_FLASH_RESET_ENABLE;
    qspi_command_xfer.width = SINGLE_BIT_SPI;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    qspi_command_xfer.instruction = SST26_CMD_FLASH_RESET;
    qspi_command_xfer.width = SINGLE_BIT_SPI;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* Disables the QUAD IO on the flash */
static APP_TRANSFER_STATUS APP_DisableQuadIO(void)
{
    memset((void *)&qspi_command_xfer, 0, sizeof(qspi_command_xfer_t));

    qspi_command_xfer.instruction = SST26_CMD_RESET_QUAD_IO;
    qspi_command_xfer.width = QUAD_CMD;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* Enables the QUAD IO on the flash */
static APP_TRANSFER_STATUS APP_EnableQuadIO(void)
{
    memset((void *)&qspi_command_xfer, 0, sizeof(qspi_command_xfer_t));

    qspi_command_xfer.instruction = SST26_CMD_ENABLE_QUAD_IO;
    qspi_command_xfer.width = SINGLE_BIT_SPI;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* Sends Write Enable command to flash */
static APP_TRANSFER_STATUS APP_WriteEnable(void)
{
    memset((void *)&qspi_command_xfer, 0, sizeof(qspi_command_xfer_t));

    qspi_command_xfer.instruction = SST26_CMD_WRITE_ENABLE;
    qspi_command_xfer.width = QUAD_CMD;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* This function sends down command to perform a global unprotect of the flash. */
static APP_TRANSFER_STATUS APP_UnlockFlash(void)
{
    if (APP_TRANSFER_COMPLETED != APP_WriteEnable())
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    memset((void *)&qspi_command_xfer, 0, sizeof(qspi_command_xfer_t));

    qspi_command_xfer.instruction = SST26_CMD_UNPROTECT_GLOBAL;
    qspi_command_xfer.width = QUAD_CMD;

    if (QSPI_CommandWrite(&qspi_command_xfer, 0) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* This function reads and stores the flash id. */
static APP_TRANSFER_STATUS APP_ReadJedecId(uint32_t *jedec_id)
{
    memset((void *)&qspi_register_xfer, 0, sizeof(qspi_register_xfer_t));

    qspi_register_xfer.instruction = SST26_CMD_QUAD_JEDEC_ID_READ;
    qspi_register_xfer.width = QUAD_CMD;
    qspi_register_xfer.dummy_cycles = 2;

    if (QSPI_RegisterRead(&qspi_register_xfer, jedec_id, 3) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* Function to read the status register of the flash. */
static APP_TRANSFER_STATUS APP_ReadStatus( uint32_t *rx_data, uint32_t rx_data_length )
{
    memset((void *)&qspi_register_xfer, 0, sizeof(qspi_register_xfer_t));

    qspi_register_xfer.instruction = SST26_CMD_READ_STATUS_REG;
    qspi_register_xfer.width = QUAD_CMD;
    qspi_register_xfer.dummy_cycles = 2;

    if (QSPI_RegisterRead(&qspi_register_xfer, rx_data, rx_data_length) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }
    return APP_TRANSFER_COMPLETED;
}

/* Function to read the configuration register of the flash. */
static APP_TRANSFER_STATUS APP_ReadConfig( uint32_t *rx_data, uint32_t rx_data_length )
{
    memset((void *)&qspi_register_xfer, 0, sizeof(qspi_register_xfer_t));

    qspi_register_xfer.instruction = SST26_CMD_READ_CONFIG_REG;
    qspi_register_xfer.width = QUAD_CMD;
    qspi_register_xfer.dummy_cycles = 2;

    if (QSPI_RegisterRead(&qspi_register_xfer, rx_data, rx_data_length) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }
    return APP_TRANSFER_COMPLETED;
}

static APP_TRANSFER_STATUS APP_GetConfig(uint16_t *config)
{
    uint8_t reg_status = 0;
    uint8_t reg_config = 0;

    if (APP_ReadStatus((uint32_t *)&reg_status, 1) != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    if (APP_ReadConfig((uint32_t *)&reg_config, 1) != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    *config = (reg_status | (reg_config << 8));

    return APP_TRANSFER_COMPLETED;
}

/* Function to write to configuration register of the flash. */
static APP_TRANSFER_STATUS APP_WriteConfig(uint16_t *config)
{
    if (APP_WriteEnable() != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    memset((void *)&qspi_register_xfer, 0, sizeof(qspi_register_xfer_t));

    qspi_register_xfer.instruction = SST26_CMD_WRITE_CONFIG_REG;
    qspi_register_xfer.width = QUAD_CMD;

    if (QSPI_RegisterWrite(&qspi_register_xfer, (uint32_t *)config, 2) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }
    return APP_TRANSFER_COMPLETED;

}

/* Checks for any pending transfers Erase/Write */
static APP_TRANSFER_STATUS APP_TransferStatusCheck(void)
{
    uint8_t reg_status = 0;

    if (APP_ReadStatus((uint32_t *)&reg_status, 1) != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    if(reg_status & (1<<0))
        return APP_TRANSFER_BUSY;
    else
        return APP_TRANSFER_COMPLETED;
}

/* Reads n Bytes of data from the flash memory */
static APP_TRANSFER_STATUS APP_MemoryRead( uint32_t *rx_data, uint32_t rx_data_length, uint32_t address )
{
    memset((void *)&qspi_memory_xfer, 0, sizeof(qspi_memory_xfer_t));

    qspi_memory_xfer.instruction = SST26_CMD_HIGH_SPEED_READ;
    qspi_memory_xfer.width = QUAD_CMD;
    qspi_memory_xfer.dummy_cycles = 6;

    if (QSPI_MemoryRead(&qspi_memory_xfer, rx_data, rx_data_length, address) == false) {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }
    return APP_TRANSFER_COMPLETED;
}

/* Reads n Bytes of data from the flash memory */
static APP_TRANSFER_STATUS APP_MemoryReadContinuous( uint32_t address )
{
    uint32_t data;
    uint16_t config = 0;

    if (APP_GetConfig(&config) != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    config |= (SET_IOC_BIT << 8);

    if (APP_WriteConfig(&config) != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    if (APP_DisableQuadIO() != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    memset((void *)&qspi_memory_xfer, 0, sizeof(qspi_memory_xfer_t));

    qspi_memory_xfer.instruction = SST26_CMD_QUADIO_READ;
    qspi_memory_xfer.option = 0xA;
    qspi_memory_xfer.width = QUAD_IO;
    qspi_memory_xfer.dummy_cycles = 5;
    qspi_memory_xfer.option_en = true;
    qspi_memory_xfer.option_len = OPTL_4_BIT;
    qspi_memory_xfer.continuous_read_en = true;

    if (QSPI_MemoryRead(&qspi_memory_xfer, &data, 1, address) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }
    return APP_TRANSFER_COMPLETED;
}

/* Writes n Bytes of data to the flash memory */
static APP_TRANSFER_STATUS APP_MemoryWrite( uint32_t *tx_data, uint32_t tx_data_length, uint32_t address )
{
    if (APP_WriteEnable() != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    memset((void *)&qspi_memory_xfer, 0, sizeof(qspi_memory_xfer_t));

    qspi_memory_xfer.instruction = SST26_CMD_PAGE_PROGRAM;
    qspi_memory_xfer.width = QUAD_CMD;

    if (QSPI_MemoryWrite(&qspi_memory_xfer, tx_data, tx_data_length, address) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

/* Sends Erase command to flash */
static APP_TRANSFER_STATUS APP_Erase(uint8_t instruction, uint32_t address)
{
    if (APP_WriteEnable() != APP_TRANSFER_COMPLETED)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    qspi_command_xfer.instruction = instruction;
    qspi_command_xfer.width = QUAD_CMD;
    qspi_command_xfer.addr_en = 1;

    if (QSPI_CommandWrite(&qspi_command_xfer, address) == false)
    {
        return APP_TRANSFER_ERROR_UNKNOWN;
    }

    return APP_TRANSFER_COMPLETED;
}

static APP_TRANSFER_STATUS APP_SectorErase(uint32_t address)
{
    return (APP_Erase(SST26_CMD_SECTOR_ERASE, address));
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/* If a function placed in Internal flash memory has to be called from
 * QSPI Flash memory it has to be defined with "long_call" compiler attribute
 * under XC32/GCC, since the QSPI region is far outside the +-16MB range of a
 * direct branch. IAR's ILINK linker inserts range-extension veneers for such
 * out-of-range calls automatically, so no equivalent attribute is needed
 * under IAR.
 *
 * Below delay_ms() is a wrapper function with "long_call" attribute defined in
 * Internal Flash memory to invoke SYSTICK PLIB and printf which are also
 * defined in internal flash memory.
 *
 * "noinline" attribute is used so that compiler does not make this function inline
 */
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
static __attribute__((noinline)) void delay_ms(uint32_t delay, char const * caller_name)
#else
static __attribute__((noinline, long_call)) void delay_ms(uint32_t delay, char const * caller_name)
#endif
{
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
    void const * const returnAddr = (void const *)__get_LR();
#endif
    SYSTICK_DelayMs(delay);

    printf("\r\nInternal Flash function %s called from QSPI Flash function %s with Return Address (%p)\r\n",
            __FUNCTION__,
            caller_name,
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
            returnAddr);
#else
            __builtin_return_address(0));
#endif
}

/* Below blink_led() function is placed in QSPI flash memory region.
 *
 * ".code_in_qspi" is a special section created in the custom linker file to
 * place this function in the QSPI Flash memory region. Under XC32/GCC, any
 * function placed in this section should have the "long_call" compiler
 * attribute. Under IAR, "#pragma location" places the function in the
 * section and no equivalent attribute is required (see note above delay_ms).
 *
 * This function can call any internal flash function which are defined with
 * "long_call" compiler attribute (XC32/GCC) or plainly (IAR).
 */
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#pragma location = ".code_in_qspi"
void blink_led(void)
#else
__attribute__((section(".code_in_qspi"),long_call)) void blink_led(void)
#endif
{
    delay_ms(1000, __FUNCTION__);

    LED_TOGGLE();
}

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Summary:
     Application initialization routine.

  Description:
    This function initializes the application.  It places the
    application in its initial state and prepares it to run so that its
    APP_Tasks function can be called.

  Parameters:
    None.

  Returns:
    None.

  Remarks:
    This routine must be called from the main function.
*/

static void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    LED_OFF();

    /* Get the Start address and end address of the QSPI Flash memory region
     * used from the Linker symbols.
     */
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
    qspi_start = (uint32_t)__section_begin(".code_in_qspi");
    qspi_end = (uint32_t)__section_end(".code_in_qspi");
#else
    qspi_start = (uint32_t)&_start_qspi;
    qspi_end = (uint32_t)&_end_qspi;
#endif

    qspi_length = qspi_end - qspi_start;

    verify_qspiMemory = (uint8_t *)(qspi_start);

    /* Starting location of blink_led function loaded in Internal Flash*/
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
    qspi_loadMemAddr = (uint8_t *)__section_begin(".code_in_qspi_init");
#else
    qspi_loadMemAddr = (uint8_t *) &_etext;
#endif

    SYSTICK_TimerStart();
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Summary:
    Application tasks function

  Description:
    This routine is the Application's tasks function.  It
    defines the application's state machine and core logic.

  Precondition:
    The system and application initialization ("SYS_Initialize") should be
    called before calling this.

  Parameters:
    None.

  Returns:
    None.

  Remarks:
    This routine must be called from SYS_Tasks() routine.
 */

static void APP_Tasks ( void )
{
    uint32_t idx;

    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {
            appData.state = APP_STATE_RESET_FLASH;
        }

        case APP_STATE_RESET_FLASH:
        {
            if (APP_ResetFlash() != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
            }
            else
            {
                appData.state = APP_STATE_ENABLE_QUAD_IO;
            }
            break;
        }

        case APP_STATE_ENABLE_QUAD_IO:
        {
            if (APP_EnableQuadIO() != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
            }
            else
            {
                appData.state = APP_STATE_UNLOCK_FLASH;
            }
            break;
        }

        case APP_STATE_UNLOCK_FLASH:
        {
            if (APP_UnlockFlash() != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
            }
            else
            {
                appData.state = APP_STATE_READ_JEDEC_ID;
            }
            break;
        }

        case APP_STATE_READ_JEDEC_ID:
        {
            if (APP_ReadJedecId(&appData.jedec_id) != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
                break;
            }

            if (appData.jedec_id != SST26VF032B_JEDEC_ID)
            {
                appData.state = APP_STATE_ERROR;
                break;
            }

            appData.state = APP_STATE_ERASE_FLASH;

            break;
        }

        case APP_STATE_ERASE_FLASH:
        {
            if (APP_SectorErase((qspi_start + erase_index)) != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
                break;
            }

            appData.state = APP_STATE_ERASE_WAIT;

            break;
        }

        case APP_STATE_ERASE_WAIT:
        {
            if (APP_TransferStatusCheck() == APP_TRANSFER_COMPLETED)
            {
                erase_index += ERASE_SIZE;

                if (erase_index < qspi_length)
                {
                    appData.state = APP_STATE_ERASE_FLASH;
                }
                else
                {
                    appData.state = APP_STATE_WRITE_MEMORY;
                }
            }
            break;
        }

        case APP_STATE_WRITE_MEMORY:
        {
            if (APP_MemoryWrite(((uint32_t *)qspi_loadMemAddr + write_index), PAGE_SIZE, (qspi_start + write_index)) != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
                break;
            }

            appData.state = APP_STATE_WRITE_WAIT;

            break;
        }

        case APP_STATE_WRITE_WAIT:
        {
            if (APP_TransferStatusCheck() == APP_TRANSFER_COMPLETED)
            {
                write_index += PAGE_SIZE;
                if (write_index < qspi_length)
                {
                    appData.state = APP_STATE_WRITE_MEMORY;
                }
                else
                {
                    appData.state = APP_STATE_READ_CONTINUOUS;
                }
            }
            break;
        }

        case APP_STATE_READ_CONTINUOUS:
        {
            appData.state = APP_STATE_SUCCESS;

            if (APP_MemoryReadContinuous(qspi_start) != APP_TRANSFER_COMPLETED)
            {
                appData.state = APP_STATE_ERROR;
                break;
            }

            /* Verify the programmed QSPI memory region */
            for (idx = 0; idx < qspi_length; idx++)
            {
                if(*(verify_qspiMemory) != *(qspi_loadMemAddr + idx))
                {
                    appData.state = APP_STATE_ERROR;
                    break;

                }
                verify_qspiMemory++;
            }

            break;
        }

        case APP_STATE_SUCCESS:
        {
            /* Call the blink_led() function placed in QSPI Flash Memory Region */
            blink_led();
            break;
        }

        case APP_STATE_ERROR:
        default:
        {
            LED_ON();
        }
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
    
    APP_Initialize();

    while ( true )
    {
        APP_Tasks();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}




/*******************************************************************************
 End of File
*/

