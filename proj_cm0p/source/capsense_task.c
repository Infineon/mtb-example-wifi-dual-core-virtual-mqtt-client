/*******************************************************************************
* File Name: capsense_task.c
*
* Description: This file contains the task that handles touch sensing.
*
*              It demonstrates:
*              - Required CapSense initialization and touch process algorithm
*              - Publishes MQTT messages over secondary publisher topic based
*              on the CapSense button input.
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


/******************************************************************************
* Header files includes
******************************************************************************/

#include "cybsp.h"
#include "cyhal.h"
#include "cycfg.h"
#include "cycfg_capsense.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "capsense_task.h"
#include "virtual_mqtt_task.h"
#include "led_task.h"

#include "cy_retarget_io.h"
#include "cy_wcm.h"
#include "cy_vcm.h"
#include "mqtt_client_config.h"
#include "cy_mqtt_api.h"


/*******************************************************************************
* Global constants
*******************************************************************************/
#define CAPSENSE_INTERRUPT_PRIORITY    (3u)
#define EZI2C_INTERRUPT_PRIORITY    (2u)    /* EZI2C interrupt priority must be
                                             * higher than CapSense interrupt
                                             */
#define CAPSENSE_SCAN_INTERVAL_MS    (20u)   /* in milliseconds*/

/* Publisher topic used for secondary core*/
#define SECONDARY_PUB_TOPIC                     "ORANGE_APP_STATUS"

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static uint32_t capsense_init(void);
static void tuner_init(void);
static void process_touch(void);
static void capsense_isr(void);
static void capsense_end_of_scan_callback(cy_stc_active_scan_sns_t* active_scan_sns_ptr);
static void capsense_timer_callback(TimerHandle_t xTimer);
static void handle_error(void);


/******************************************************************************
* Global variables
******************************************************************************/
QueueHandle_t capsense_command_q;
static capsense_command_data_t capsense_cmd_data;
static TimerHandle_t scan_timer_handle;
static cyhal_ezi2c_t sEzI2C;
static cyhal_ezi2c_slave_cfg_t sEzI2C_sub_cfg;
static cyhal_ezi2c_cfg_t sEzI2C_cfg;

/* Structure to store publish message information for secondary core. */
static cy_mqtt_publish_info_t secondary_publish_info =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = SECONDARY_PUB_TOPIC,
    .topic_len = (sizeof(SECONDARY_PUB_TOPIC) - 1),
    .retain = false,
    .dup = false
};

/* SysPm callback params */
static cy_stc_syspm_callback_params_t callback_params =
{
    .base       = CYBSP_CSD_HW,
    .context    = &cy_capsense_context
};

static cy_stc_syspm_callback_t capsense_deep_sleep_cb =
{
    Cy_CapSense_DeepSleepCallback,
    CY_SYSPM_DEEPSLEEP,
    (CY_SYSPM_SKIP_CHECK_FAIL | CY_SYSPM_SKIP_BEFORE_TRANSITION | CY_SYSPM_SKIP_AFTER_TRANSITION),
    &callback_params,
    NULL,
    NULL
};


/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void handle_error(void)
{
    /* Disable all interrupts. */
    __disable_irq();

    CY_ASSERT(0);
}


/*******************************************************************************
* Function Name: task_capsense
********************************************************************************
* Summary:
*  Task that initializes the CapSense block and processes the touch input.
*
* Parameters:
*  void *param : Task parameter defined during task creation (unused)
*
* Return:
*  void
*******************************************************************************/
void task_capsense(void* param)
{
    cy_rslt_t result;
    cy_capsense_status_t cap_stat;
    cy_status status;

    /* Remove warning for unused parameter */
    (void)param;

    /* Initialize timer for periodic CapSense scan */
    scan_timer_handle = xTimerCreate ("Scan Timer", CAPSENSE_SCAN_INTERVAL_MS,
                                      pdTRUE, NULL, capsense_timer_callback);

    /* Setup communication between Tuner GUI and PSoC 6 MCU */
    tuner_init();

    /* Initialize CapSense block */
    status = capsense_init();
    if(CY_RET_SUCCESS != status)
    {
        CY_ASSERT(0u);
    }

    /* Start the timer */
    xTimerStart(scan_timer_handle, 0u);

    /* Repeatedly running part of the task */
    for(;;)
    {
        /* Block until a CapSense command has been received over queue */
        /* Command has been received from capsense_cmd_data */
        if(pdTRUE == xQueueReceive(capsense_command_q, &capsense_cmd_data, portMAX_DELAY))
        {
            /* Check if CapSense is busy with a previous scan */
            if(CY_CAPSENSE_NOT_BUSY == Cy_CapSense_IsBusy(&cy_capsense_context))
            {
                switch(capsense_cmd_data.command)
                {
                    case CAPSENSE_SCAN:
                    { 
                        /* Start scan */
                        cap_stat = Cy_CapSense_ScanAllWidgets(&cy_capsense_context);
                        if(CY_CAPSENSE_STATUS_SUCCESS != cap_stat)
                        {
                            printf("scanning error");
                        }                           
                        break;
                    }
                    case CAPSENSE_PROCESS:
                    {
                        /* Process all widgets */
                        cap_stat = Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);
                        if(CY_CAPSENSE_STATUS_SUCCESS != cap_stat)
                        {
                            printf("processing error");
                        }                       
                        process_touch();

                        /* Establishes synchronized operation between the CapSense
                         * middleware and the CapSense Tuner tool.
                         */
                        Cy_CapSense_RunTuner(&cy_capsense_context);
                        break;
                    }
                    case CAPSENSE_DATA_PUBLISH:
                    {
                        /* Publish the data received over the message queue. */
                        if (capsense_cmd_data.data)
                        {
                            secondary_publish_info.payload = ON_MESSAGE;
                        }
                        else
                        {
                            secondary_publish_info.payload = OFF_MESSAGE;
                        }
                        secondary_publish_info.payload_len = strlen(secondary_publish_info.payload);

                        printf("\nPublisher: Publishing '%s' on the topic '%s'\n",
                                 (char *) secondary_publish_info.payload, secondary_publish_info.topic);

                        result = cy_mqtt_publish(virtual_mqtt_connection, &secondary_publish_info);

                        if (CY_RSLT_SUCCESS != result)
                        {
                            printf("  Publisher: MQTT Publish failed with error 0x%0X.\n\n", (int)result);

                            /* Communicate the publish failure with the the MQTT
                             * client task.
                             */
                            virtual_mqtt_task_cmd = HANDLE_VIRTUAL_MQTT_PUBLISH_FAILURE;
                            xQueueSend(virtual_mqtt_task_data_q, &virtual_mqtt_task_cmd, portMAX_DELAY);
                        }

                        print_heap_usage("publisher_task: After publishing an MQTT message");
                        break;
                    }
                    /* Invalid command */
                    default:
                    {
                        break;
                    }
                }
            }
        }
        /* Task has timed out and received no data during an interval of
         * portMAXDELAY ticks.
         */
        else
        {
            /* Handle timeout here */
        }
    }
}


/*******************************************************************************
* Function Name: process_touch
********************************************************************************
* Summary:
*  This function processes the touch input and sends command to LED task.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void process_touch(void)
{
    /* Variables used to store touch information */
    uint32_t button0_status = 0;
    uint32_t button1_status = 0;

    /* Variables used to store previous touch information */
    static uint32_t button0_status_prev = 0;
    static uint32_t button1_status_prev = 0;

    /* Variables used for storing command and data for LED Task */
    bool send_led_command = false;

    /* Get button 0 status */
    button0_status = Cy_CapSense_IsSensorActive(
        CY_CAPSENSE_BUTTON0_WDGT_ID,
        CY_CAPSENSE_BUTTON0_SNS0_ID,
        &cy_capsense_context);

    /* Get button 1 status */
    button1_status = Cy_CapSense_IsSensorActive(
        CY_CAPSENSE_BUTTON1_WDGT_ID,
        CY_CAPSENSE_BUTTON1_SNS0_ID,
        &cy_capsense_context);


    /* Detect new touch on Button0 */
    if((0u != button0_status) && (0u == button0_status_prev))
    {
        capsense_cmd_data.command = CAPSENSE_DATA_PUBLISH;
        capsense_cmd_data.data = OFF_STATE;
        send_led_command = true;
    }

    /* Detect new touch on Button1 */
    if((0u != button1_status) && (0u == button1_status_prev))
    {
        capsense_cmd_data.command = CAPSENSE_DATA_PUBLISH;
        capsense_cmd_data.data = ON_STATE;
        send_led_command = true;
    }

    /* Send command to update LED state if required */
    if(send_led_command)
    {
        xQueueSendToBack(capsense_command_q, &capsense_cmd_data, 0u);
    }

    /* Update previous touch status */
    button0_status_prev = button0_status;
    button1_status_prev = button1_status;
}


/*******************************************************************************
* Function Name: capsense_init
********************************************************************************
* Summary:
*  This function initializes the CSD HW block, and configures the CapSense
*  interrupt.
*
* Parameters:
*  void
*
* Return:
*  uint32_t
*******************************************************************************/
static uint32_t capsense_init(void)
{
    uint32_t status = CYRET_SUCCESS;
    cy_capsense_status_t cap_stat;
    cy_en_sysint_status_t sysint_stat;
    bool cback_stat;
    
    /* CapSense interrupt configuration parameters */
    static const cy_stc_sysint_t capSense_intr_config =
    {
#if defined(COMPONENT_CM0P)
            .intrSrc = NvicMux3_IRQn,
            .cm0pSrc = csd_interrupt_IRQn,
            .intrPriority = CAPSENSE_INTERRUPT_PRIORITY,
#endif
#if defined(COMPONENT_CM4)
            .intrSrc = csd_interrupt_IRQn,
            .intrPriority = CAPSENSE_INTERRUPT_PRIORITY,
#endif
    };

    /*Initialize CapSense Data structures */
    status = Cy_CapSense_Init(&cy_capsense_context);
    if (CY_CAPSENSE_STATUS_SUCCESS == status)
    {
        /* Initialize CapSense interrupt */
        sysint_stat = Cy_SysInt_Init(&capSense_intr_config, capsense_isr);
        if(CY_SYSINT_SUCCESS == sysint_stat)
        {
            NVIC_ClearPendingIRQ(capSense_intr_config.intrSrc);
            NVIC_EnableIRQ(capSense_intr_config.intrSrc);
            /* Initialize the CapSense deep sleep callback functions. */
            cap_stat = Cy_CapSense_Enable(&cy_capsense_context);
            if(CY_CAPSENSE_STATUS_SUCCESS != cap_stat)
            {
                printf("\nFailed enable");
            }
            else
            {
                cback_stat = Cy_SysPm_RegisterCallback(&capsense_deep_sleep_cb);
                if(true != cback_stat)
                {
                    printf("\nFailed Callback registration");
                }
                else
                {
                    /* Register end of scan callback */
                    status = Cy_CapSense_RegisterCallback(CY_CAPSENSE_END_OF_SCAN_E,
                                                 capsense_end_of_scan_callback, &cy_capsense_context);
                    if (CY_CAPSENSE_STATUS_SUCCESS == status)
                    {
                        /* Initialize the CapSense firmware modules. */
                        status = Cy_CapSense_Enable(&cy_capsense_context);
                    }
                }
            }
        }
    }
    return status;
}


/*******************************************************************************
* Function Name: capsense_end_of_scan_callback
********************************************************************************
* Summary:
*  CapSense end of scan callback function. This function sends a command to
*  CapSense task to process scan.
*
* Parameters:
*  cy_stc_active_scan_sns_t * active_scan_sns_ptr (unused)
*
* Return:
*  void
*******************************************************************************/
static void capsense_end_of_scan_callback(cy_stc_active_scan_sns_t* active_scan_sns_ptr)
{
    BaseType_t xYieldRequired;

    (void)active_scan_sns_ptr;

    /* Send command to process CapSense data */
    capsense_cmd_data.command = CAPSENSE_PROCESS;
    xYieldRequired = xQueueSendToBackFromISR(capsense_command_q, &capsense_cmd_data, 0u);
    portYIELD_FROM_ISR(xYieldRequired);
}


/*******************************************************************************
* Function Name: capsense_timer_callback
********************************************************************************
* Summary:
*  CapSense timer callback. This function sends a command to start CapSense
*  scan.
*
* Parameters:
*  TimerHandle_t xTimer (unused)
*
* Return:
*  void
*******************************************************************************/
static void capsense_timer_callback(TimerHandle_t xTimer)
{
    Cy_CapSense_Wakeup(&cy_capsense_context);
    capsense_cmd_data.command = CAPSENSE_SCAN;
    BaseType_t xYieldRequired;

    (void)xTimer;

    /* Send command to start CapSense scan */
    xYieldRequired = xQueueSendToBackFromISR(capsense_command_q, &capsense_cmd_data, 0u);
    portYIELD_FROM_ISR(xYieldRequired);
}


/*******************************************************************************
* Function Name: capsense_isr
********************************************************************************
* Summary:
*  Wrapper function for handling interrupts from CSD block.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void capsense_isr(void)
{
    Cy_CapSense_InterruptHandler(CYBSP_CSD_HW, &cy_capsense_context);
}


/*******************************************************************************
* Function Name: tuner_init
********************************************************************************
* Summary:
*  Initializes communication between Tuner GUI and PSoC 6 MCU.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void tuner_init(void)
{
    cy_rslt_t result;
    /* Configure Capsense Tuner as EzI2C Slave */
    sEzI2C_sub_cfg.buf = (uint8 *)&cy_capsense_tuner;
    sEzI2C_sub_cfg.buf_rw_boundary = sizeof(cy_capsense_tuner);
    sEzI2C_sub_cfg.buf_size = sizeof(cy_capsense_tuner);
    sEzI2C_sub_cfg.slave_address = 8U;

    sEzI2C_cfg.data_rate = CYHAL_EZI2C_DATA_RATE_400KHZ;
    sEzI2C_cfg.enable_wake_from_sleep = true;
    sEzI2C_cfg.slave1_cfg = sEzI2C_sub_cfg;
    sEzI2C_cfg.sub_address_size = CYHAL_EZI2C_SUB_ADDR16_BITS;
    sEzI2C_cfg.two_addresses = false;

    result = cyhal_ezi2c_init( &sEzI2C, CYBSP_I2C_SDA, CYBSP_I2C_SCL, NULL, &sEzI2C_cfg);
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }
}
/* END OF FILE [] */
