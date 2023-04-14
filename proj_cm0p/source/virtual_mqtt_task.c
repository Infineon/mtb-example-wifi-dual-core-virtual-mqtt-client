/******************************************************************************
* File Name:   virtual_mqtt_task.c
*
* Description: This file contains the task that handle Virtual MQTT task on CM0+
*
*              It demonstrates:
*              - Receiving status flag for setting up virtualization on CM4
*              - Creating FreeRTOS tasks for LED control and CapSense input
*              - Initialize WCM and MQTT
*              - Implements handles for disconnection
*
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

/*******************************************************************************
 * Header file includes
 ******************************************************************************/

#include "cybsp.h"
#include "cyhal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cycfg.h"

/* Task header files */
#include "capsense_task.h"
#include "led_task.h"
#include "virtual_mqtt_task.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_wcm.h"
#include "cy_vcm.h"
#include "mqtt_client_config.h"
#include "cy_mqtt_api.h"
#include "clock.h"

/******************************************************************************
* Macros
******************************************************************************/

#define MQTT_HANDLE_DESCRIPTOR           "MQTThandleID"

#define WAIT_DELAY_MS      (2000U)

/* Priorities of user tasks in this project. configMAX_PRIORITIES is defined in
 * the FreeRTOSConfig.h and higher priority numbers denote high priority tasks.
 */
#define TASK_CAPSENSE_PRIORITY (configMAX_PRIORITIES - 2)
#define TASK_LED_PRIORITY (configMAX_PRIORITIES - 3)

/* Stack sizes of user tasks in this project */
#define TASK_CAPSENSE_STACK_SIZE (512u)
#define TASK_LED_STACK_SIZE (2*1024u )


/* Queue lengths of message queues used in this project */
#define SINGLE_ELEMENT_QUEUE (1u)

/* Macro to check if the result of an operation was successful and set the
 * corresponding bit in the status_flag based on 'init_mask' parameter. When
 * it has failed, print the error message and return the result to the
 * calling function.
 */
#define CHECK_RESULT(result, success_message, error_message...)      \
                     do                                        \
                     {                                         \
                         if (CY_RSLT_SUCCESS == (int)result)   \
                         {                                     \
                             printf(success_message);          \
                         }                                     \
                         else                                  \
                         {                                     \
                             printf(error_message);            \
                         }                                     \
                     } while(0)

/*******************************************************************************
 * Global variable
 ******************************************************************************/
/* Queue handle used for Virtual tasks */
cy_mqtt_t virtual_mqtt_connection;

/* Queue handle used for Virtual tasks */
QueueHandle_t virtual_mqtt_task_data_q;

volatile uint8_t wifi_status;

virtual_mqtt_task_cmd_t virtual_mqtt_task_cmd;

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void event_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data);
static void virtual_mqtt_event_callback(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event,
        void *user_data);


/*******************************************************************************
* Function Name: virtual_mqtt_task
********************************************************************************
* Summary:
*  Task that controls the virtual task
*
* Parameters:
*  void *param : Task parameter defined during task creation (unused)
*
* Return:
*  void
*******************************************************************************/
void virtual_mqtt_task(void *arg)
{
    cy_rslt_t result;

    printf("\nPlease wait until VCM is initialized from CM4...\r\n");
    while(0 == init_done)
    {
        vTaskDelay(WAIT_DELAY_MS);
    }

    /* Create the queues. See the respective data-types for queue contents   */

    led_command_data_q = xQueueCreate(SINGLE_ELEMENT_QUEUE,
                                         sizeof(led_command_data_t));
    virtual_mqtt_task_data_q = xQueueCreate(SINGLE_ELEMENT_QUEUE,
                                         sizeof(virtual_mqtt_task_cmd_t));
    capsense_command_q = xQueueCreate(SINGLE_ELEMENT_QUEUE,
                                         sizeof(capsense_command_t));

    /* Create the user tasks. See the respective task definition for more
     * details of these tasks.
     */
    xTaskCreate(task_capsense, "CapSense Task", TASK_CAPSENSE_STACK_SIZE,
                   NULL, TASK_CAPSENSE_PRIORITY, NULL);

    printf("\nCapsense task created\n");

    xTaskCreate(task_led, "Led Task", TASK_LED_STACK_SIZE,
                   NULL, TASK_LED_PRIORITY, NULL);

    printf("LED task created\n");

    /* Wi-Fi and MQTT initialization */

    result = cy_wcm_init(NULL);
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nWi-Fi Connection Manager Initialization failed on CM0+.\r\n");
    }
    result = cy_mqtt_init();
    if(CY_RSLT_SUCCESS != result)
    {
        printf("\nMQTT Initialization failed on CM0+.\r\n");
    }
    printf("\nWi-Fi connection status : %s\n",
                (1 == cy_wcm_is_connected_to_ap()) ? "Connected":"Disconnected");

    result = cy_wcm_register_event_callback(event_callback);

    CHECK_RESULT(result, "Wi-Fi event callback registered successfully\n",
            "Wi-Fi event callback could not be registered\n");

    result = cy_mqtt_get_handle(&virtual_mqtt_connection, MQTT_HANDLE_DESCRIPTOR);

    CHECK_RESULT(result, "\nMQTT connection established on CM0+\r\n",
            "\nMQTT connection could not be established on CM0+\r\n");

    result = cy_mqtt_register_event_callback(virtual_mqtt_connection,
            (cy_mqtt_callback_t)virtual_mqtt_event_callback, NULL);

    CHECK_RESULT(result, "MQTT event callback registered successfully\r\n",
            "MQTT event callback could not be registered\n");


    /* Repeatedly running part of the task */
    for(;;)
    {
        /* Block until a command has been received over queue */
        if(pdTRUE == xQueueReceive(virtual_mqtt_task_data_q, &virtual_mqtt_task_cmd,
                portMAX_DELAY) )
        {
            switch(virtual_mqtt_task_cmd)
            {
                case HANDLE_VIRTUAL_MQTT_PUBLISH_FAILURE:
                {
                    /* Handle Publish Failure here. */
                    break;
                }

                case HANDLE_VIRTUAL_MQTT_SUBSCRIBE_FAILURE:
                {
                    /* Handle Subscribe Failure here. */

                    vTaskDelay(WAIT_DELAY_MS);

                    /* Re-initiate MQTT subscribe. */
                    led_cmd_data.command = SUBSCRIBE_TO_TOPIC;
                    xQueueSend(led_command_data_q, &led_cmd_data, portMAX_DELAY);

                    break;
                }

                case HANDLE_VIRTUAL_MQTT_DISCONNECTION:
                {
                    /* Check if Wi-Fi connection is active. If not, update the
                     * status flag and initiate Wi-Fi reconnection.
                     */
                    if ( CY_WCM_EVENT_CONNECTED == cy_wcm_is_connected_to_ap())
                    {
                        printf("\nWi-Fi connection is alive\n");

                        /* Initiate MQTT subscribe post the reconnection. */
                        led_cmd_data.command = SUBSCRIBE_TO_TOPIC;
                        xQueueSend(led_command_data_q, &led_cmd_data, portMAX_DELAY);

                    }
                    else
                    {
                        printf("\nWaiting for Wi-Fi connection to back alive\n");
                        /* Wait until Wi-Fi connection is back */
                        vTaskDelay(WAIT_DELAY_MS);

                        virtual_mqtt_task_cmd= HANDLE_VIRTUAL_MQTT_DISCONNECTION;
                        xQueueSend(virtual_mqtt_task_data_q, &virtual_mqtt_task_cmd,
                                portMAX_DELAY);
                    }
                    break;
                }

                default:
                    break;
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


/******************************************************************************
 * Function Name: virtual_mqtt_event_callback
 ******************************************************************************
 * Summary:
 *  Callback invoked by the MQTT library for events like MQTT disconnection,
 *  incoming MQTT subscription messages from the MQTT broker.
 *    1. In case of MQTT disconnection, the MQTT client task is communicated
 *       about the disconnection using a message queue.
 *    2. When an MQTT subscription message is received, the subscriber callback
 *       function implemented in subscriber_task.c is invoked to handle the
 *       incoming MQTT message.
 *
 * Parameters:
 *  cy_mqtt_t mqtt_handle : MQTT handle corresponding to the MQTT event (unused)
 *  cy_mqtt_event_t event : MQTT event information
 *  void *user_data : User data pointer passed during cy_mqtt_create() (unused)
 *
 * Return:
 *  void
 ******************************************************************************/
static void virtual_mqtt_event_callback(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data)
{
    cy_mqtt_publish_info_t *received_msg;

    (void) mqtt_handle;
    (void) user_data;

    switch(event.type)
    {
        case CY_MQTT_EVENT_TYPE_DISCONNECT:
        {
            /* MQTT connection with the MQTT broker is broken as the client
             * is unable to communicate with the broker.
             */
            printf("\nUnexpectedly disconnected from MQTT broker!\n");

            virtual_mqtt_task_cmd= HANDLE_VIRTUAL_MQTT_DISCONNECTION;
            xQueueSend(virtual_mqtt_task_data_q, &virtual_mqtt_task_cmd, portMAX_DELAY);
            break;
        }

        case CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE:
        {
            /* Incoming MQTT message has been received. Send this message to
             * the subscriber callback function to handle it.
             */
            received_msg = &(event.data.pub_msg.received_message);

            virtual_mqtt_subscription_callback(received_msg);
            break;
        }
        default :
        {
            /* Unknown MQTT event */
            printf("\nUnknown Event received from MQTT callback!\n");
            break;
        }
    }
}


/******************************************************************************
 * Function Name: event_callback
 ******************************************************************************
 * Summary:
 *  Callback invoked by the WCM library for events like WCM disconnection,
 *  connection, reconnection or change in IP, etc..
 *
 * Parameters:
 *  cy_wcm_event_t event : WCM event information
 *  cy_wcm_event_data_t *event_data : get MAC address of the connected STA to SoftAP
 *
 * Return:
 *  void
 ******************************************************************************/
void event_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data)
{

    wifi_status = event ;
}

/* [] END OF FILE  */
