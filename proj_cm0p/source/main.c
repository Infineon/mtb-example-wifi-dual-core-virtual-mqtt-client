/******************************************************************************
* File Name: main.c
*
* Description: This file demonstrates the initialization required on virtual
*              core side(CM0+) for this Dual-core virtual MQTT client example.
*
*              It demonstrates:
*              - how to initialize Virtual connection manager
*              - create a FreeRTOS task for virtual connection to MQTT broker
*
*              This code example uses FreeRTOS. For documentation and API
*              references of FreeRTOS, visit : https://www.freertos.org
*
* Related Document: See README.md
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

#include "cybsp.h"
#include "cyhal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Task header files */
#include "capsense_task.h"
#include "led_task.h"
#include "virtual_mqtt_task.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_wcm.h"
#include "cy_vcm.h"

#if ( defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS) )
#include "cy_log.h"
#endif /* defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS) */

#include "cy_mqtt_api.h"
#include "clock.h"

/*******************************************************************************
 * Global constants
 ******************************************************************************/
#define Print_DelayMS           (100u)

/******************************************************************************
* Global variables
******************************************************************************/
/* Flag to check VCM initialization is done or not*/
volatile uint8_t init_done=0;

static const cyhal_resource_inst_t lptimer_1_inst_obj =
{
    .type = CYHAL_RSC_LPTIMER,
    .block_num = 1U,
    .channel_num = 0U,
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static void vcm_callback(cy_vcm_event_t event);

/*******************************************************************************
* Function Name: main()
********************************************************************************
* Summary:
*  System entrance point. This function sets up user tasks and then starts
*  the RTOS scheduler.
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    CY_ASSERT(CY_RSLT_SUCCESS == result);

    cyhal_hwmgr_reserve(&lptimer_1_inst_obj);

   /* Initialize retarget-io to use the debug UART port. */
    result = cy_retarget_io_init(DEBUG_UART_TX, DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nRetarget IO Initialization failed on CM0+.\r\n");
        CY_ASSERT(0);
    }
#if defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS)
    result = cy_log_init(CY_LOG_DEBUG4, NULL, NULL);
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nLog Initialization failed on CM0+.\r\n");
        CY_ASSERT(0);
    }   
#endif /*defined(ENABLE_VCM_LOGS) || defined(ENABLE_WCM_LOGS)  */




    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("===============================================================\r\n");
    printf("Dual-core MQTT Client: Virtual  connectivity on CM0+\r\n");
    printf("===============================================================\r\n");
    Cy_SysLib_Delay(Print_DelayMS);
    
    /* VCM configuration for secondary connectivity core (CM0+) */
    cy_vcm_config_t secondary_core_config;
    secondary_core_config.hal_resource_opt = CY_VCM_CREATE_HAL_RESOURCE;
    secondary_core_config.channel_num = CYHAL_IPC_CHAN_0;
    secondary_core_config.event_cb = vcm_callback;
    result = cy_vcm_init(&secondary_core_config);

    /* VCM Initialization failed. Stop program execution */
    if (CY_RSLT_SUCCESS != result)
    {
        printf("\nVCM Initialization failed on CM0+.\r\n");
        CY_ASSERT(0);
    }
    else
    {
        printf("\nVirtual Connectivity Manager Initialized on CM0+\n");
    }
    Cy_SysLib_Delay(Print_DelayMS);

    /* Enable CM4.*/
    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR);

    /* Enable global interrupts */
    __enable_irq();


    xTaskCreate(virtual_mqtt_task, "Virtual MQTT Task", TASK_VIRTUAL_STACK_SIZE,
                NULL, TASK_VIRTUAL_PRIORITY, NULL);

    printf("Virtual MQTT task created\r\n");


    /* Start the RTOS scheduler. This function should never return */
    vTaskStartScheduler();

    /********************** Should never get here ***************************/
    /* RTOS scheduler exited */
    /* Halt the CPU if scheduler exits */
    CY_ASSERT(0);

    for (;;)
    {
    }

}


/*******************************************************************************
* Function Name: vcm_callback
********************************************************************************
* Summary:
*  Callback invoked by VCM library for events like Virtualization init or deinit
*
* Parameters:
*  cy_vcm_event_t event : VCM event information
*
* Return:
*  void
*******************************************************************************/
static void vcm_callback(cy_vcm_event_t event)
{
    printf( "\nVCM Callback:");
    switch(event)
    {
    case CY_VCM_EVENT_INIT_COMPLETE:
    {
        printf("received INIT_COMPLETE from CM4\n");
        init_done = 1;
        break;
    }
    case CY_VCM_EVENT_DEINIT:
    {
        printf("received DEINIT from CM4\n");
        break;
    }
    default:
    {
        printf("Unknown event");
        break;
    }
    }
}

/*******************************************************************************
* Function Name: cybsp_register_custom_sysclk_pm_callback
********************************************************************************
* Summary:
*  Registers a power management callback that prepares the clock system for
*  entering deep sleep mode and restore the clocks upon wakeup from deep sleep.
*  Here, Empty callback function
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t
*******************************************************************************/
#if defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK)

cy_rslt_t cybsp_register_custom_sysclk_pm_callback(void)
{
    return CY_RSLT_SUCCESS;
}

#endif /* defined(CYBSP_CUSTOM_SYSCLK_PM_CALLBACK) */

/* [] END OF FILE  */
