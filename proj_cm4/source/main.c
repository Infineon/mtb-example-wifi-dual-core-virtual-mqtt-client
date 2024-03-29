/******************************************************************************
* File Name:   main.c
*
* Description: This file demonstrates the mqtt task initialization on primary
*              connectivity core side(CM4) for this Dual-core virtual MQTT
*              client example.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
 * Header file includes
 ******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "FreeRTOS.h"
#include "task.h"

#include "mqtt_task.h"

/* middleware */
#include "cy_vcm.h"
#include "cy_retarget_io.h"

#if ( defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS) )
#include "cy_log.h"
#endif /* defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS) */

/*******************************************************************************
 * Global variable
 ******************************************************************************/
/* LP timer */
static const cyhal_resource_inst_t lptimer_0_inst_obj =
 {
        .type = CYHAL_RSC_LPTIMER,
        .block_num = 0U,
        .channel_num = 0U,
 };

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************
 * Function Name: main
 ******************************************************************************
 * Summary:
 *  System entrance point. This function initializes retarget IO, sets up 
 *  the MQTT client task, and then starts the RTOS scheduler.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 ******************************************************************************/
int main()
{
    cy_rslt_t result;

    /* Initialize the board support package. */
    result = cybsp_init();

    CY_ASSERT(CY_RSLT_SUCCESS == result);

    cyhal_hwmgr_reserve(&lptimer_0_inst_obj);

    /* To avoid compiler warnings. */
    (void) result;

    /* Enable global interrupts. */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port. */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nRetarget IO Initialization failed on CM4.\r\n");
        CY_ASSERT(0);
    }
#if ( defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS) )
    result = cy_log_init(CY_LOG_DEBUG4, NULL, NULL);
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nLog Initialization failed on CM4.\r\n");
        CY_ASSERT(0);
    }
#endif /*defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS)  */

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("===============================================================\r\n");
    printf("Dual-core MQTT Client: Connectivity stack running on CM4\r\n");
    printf("===============================================================\r\n\n");


    /* Create the MQTT Client task. */
    xTaskCreate(mqtt_client_task, "MQTT Client task", MQTT_CLIENT_TASK_STACK_SIZE,
                NULL, MQTT_CLIENT_TASK_PRIORITY, NULL);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}

/* [] END OF FILE */
