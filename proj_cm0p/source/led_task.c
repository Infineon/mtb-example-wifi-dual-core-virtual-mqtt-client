/******************************************************************************
* File Name: led_task.c
*
* Description: This file contains the task that handles led.
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
#include "cy_retarget_io.h"

#include "mqtt_client_config.h"
#include "cy_mqtt_api.h"

#include "led_task.h"
#include "virtual_mqtt_task.h"

/*******************************************************************************
* Global constants
*******************************************************************************/
/* Maximum number of retries for MQTT subscribe operation */
#define MAX_SUBSCRIBE_RETRIES                   (3u)

/* Time interval in milliseconds between MQTT subscribe retries. */
#define MQTT_SUBSCRIBE_RETRY_INTERVAL_MS        (1000)

/* The number of MQTT topics to be subscribed to. */
#define SUBSCRIPTION_COUNT                      (1)

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void subscribe_to_topic(void);
static void unsubscribe_from_topic(void);

/*******************************************************************************
 * Global variable
 ******************************************************************************/
/* Queue handle used for LED data */
QueueHandle_t led_command_data_q;
led_command_data_t led_cmd_data;

/* Configure the subscription information structure. */
cy_mqtt_subscribe_info_t subscriber_data =
{
    .qos = (cy_mqtt_qos_t) MQTT_MESSAGES_QOS,
    .topic = MQTT_SUB_TOPIC,
    .topic_len = (sizeof(MQTT_SUB_TOPIC) - 1)
};


/*******************************************************************************
* Function Name: task_led
********************************************************************************
* Summary:
*  Task that controls the LED.
*
* Parameters:
*  void *param : Task parameter defined during task creation (unused)
*
* Return:
*  void
*******************************************************************************/
void task_led(void* param)
{
    /* Suppress warning for unused parameter */
    (void)param;

    /* Initialize the User LED. */
    cyhal_gpio_init(CYBSP_USER_LED2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_PULLUP,
                    CYBSP_LED_STATE_OFF);

    /* Subscribe to the specified MQTT topic. */
    subscribe_to_topic();

    /* Repeatedly running part of the task */
    for(;;)
    {
        /* Wait for commands from other tasks and callbacks. */
        if (pdTRUE == xQueueReceive(led_command_data_q, &led_cmd_data, portMAX_DELAY))
        {
            switch(led_cmd_data.command)
            {
                case SUBSCRIBE_TO_TOPIC:
                {
                    subscribe_to_topic();
                    break;
                }
                case UNSUBSCRIBE_FROM_TOPIC:
                {
                    unsubscribe_from_topic();
                    break;
                }
                case UPDATE_DEVICE_STATE:
                {
                    /* Update the LED state as per received notification. */
                    cyhal_gpio_write(CYBSP_USER_LED2, led_cmd_data.data);

                    print_heap_usage("subscriber_task: After updating LED state");
                    break;
                }
                /* Invalid command */
                default:
                {
                    /* Handle invalid command here */
                    break;
                }

            }
        }
    }
}

/******************************************************************************
 * Function Name: subscribe_to_topic
 ******************************************************************************
 * Summary:
 *  Function that subscribes to the MQTT topic specified by the macro
 *  'MQTT_SUB_TOPIC'. This operation is retried a maximum of
 *  'MAX_SUBSCRIBE_RETRIES' times with interval of
 *  'MQTT_SUBSCRIBE_RETRY_INTERVAL_MS' milliseconds.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 ******************************************************************************/
static void subscribe_to_topic(void)
{
    /* Status variable */
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Subscribe with the configured parameters. */
    for (uint32_t retry_count = 0; retry_count < MAX_SUBSCRIBE_RETRIES; retry_count++)
    {
        result = cy_mqtt_subscribe(virtual_mqtt_connection, &subscriber_data, SUBSCRIPTION_COUNT);
        if (CY_RSLT_SUCCESS == result)
        {
            printf("\nMQTT client subscribed to the topic '%.*s' successfully.\n",
                    subscriber_data.topic_len, subscriber_data.topic);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_SUBSCRIBE_RETRY_INTERVAL_MS));
    }

    if (CY_RSLT_SUCCESS != result)
    {
        printf("\nMQTT Subscribe failed with error 0x%0X after %d retries...\n",
               (int)result, MAX_SUBSCRIBE_RETRIES);

        /* Notify the MQTT client task about the subscription failure */
        virtual_mqtt_task_cmd = HANDLE_VIRTUAL_MQTT_SUBSCRIBE_FAILURE;
        xQueueSend(virtual_mqtt_task_data_q, &virtual_mqtt_task_cmd, portMAX_DELAY);
    }
}

/******************************************************************************
 * Function Name: mqtt_subscription_callback
 ******************************************************************************
 * Summary:
 *  Callback to handle incoming MQTT messages. This callback prints the
 *  contents of the incoming message and informs the subscriber task, via a
 *  message queue, to turn on / turn off the device based on the received
 *  message.
 *
 * Parameters:
 *  cy_mqtt_publish_info_t *received_msg_info : Information structure of the
 *                                              received MQTT message
 *
 * Return:
 *  void
 ******************************************************************************/
void mqtt_subscription_callback(cy_mqtt_publish_info_t *received_msg_info)
{
    /* Received MQTT message */
    const char *received_msg = received_msg_info->payload;
    int received_msg_len = received_msg_info->payload_len;

    const char * topic = received_msg_info->topic;
    int topic_length = received_msg_info->topic_len;

    /* Assign the device state depending on the received MQTT message.
     *
     * In this example, The MQTT handle is shared across both the cores so both
     * the cores receives subscription messages. Hence filtering the publisher
     * topics to avoid printing the publish messages in this callback.
     *  */
    if (0 == strncmp(MQTT_PUB_TOPIC, topic, topic_length))
    {
        return;
    }
    else if ((strlen(RED_ON_MESSAGE) == received_msg_len) &&
        (0 == strncmp(RED_ON_MESSAGE, received_msg, received_msg_len)))
    {
        printf("  \nSubsciber: Incoming MQTT message received:\n"
               "    Received topic name: %.*s\n"
               "    Received QoS: %d\n"
               "    Received payload: %.*s\n",
               received_msg_info->topic_len, received_msg_info->topic,
               (int) received_msg_info->qos,
               (int) received_msg_info->payload_len, (const char *)received_msg_info->payload);

        /* Assign the command to be sent to the subscriber task. */
        led_cmd_data.command = UPDATE_DEVICE_STATE;
        led_cmd_data.data = ON_STATE;
    }
    else if ((strlen(RED_OFF_MESSAGE) == received_msg_len) &&
             (0 == strncmp(RED_OFF_MESSAGE, received_msg, received_msg_len)))
    {
        printf("  \nSubsciber: Incoming MQTT message received:\n"
               "    Received topic name: %.*s\n"
               "    Received QoS: %d\n"
               "    Received payload: %.*s\n",
               received_msg_info->topic_len, received_msg_info->topic,
               (int) received_msg_info->qos,
               (int) received_msg_info->payload_len, (const char *)received_msg_info->payload);

        /* Assign the command to be sent to the subscriber task. */
        led_cmd_data.command = UPDATE_DEVICE_STATE;
        led_cmd_data.data = OFF_STATE;
    }
    else
    {
        printf("  Subscriber: Received MQTT message not in valid format!\n");
        return;
    }

    print_heap_usage("MQTT subscription callback");

    /* Send the command and data to LED task queue */
    xQueueSend(led_command_data_q, &led_cmd_data, portMAX_DELAY);
}


/******************************************************************************
 * Function Name: unsubscribe_from_topic
 ******************************************************************************
 * Summary:
 *  Function that unsubscribes from the topic specified by the macro
 *  'MQTT_SUB_TOPIC'.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void unsubscribe_from_topic(void)
{
    cy_rslt_t result = cy_mqtt_unsubscribe(virtual_mqtt_connection,
                                           (cy_mqtt_unsubscribe_info_t *) &subscriber_data,
                                           SUBSCRIPTION_COUNT);

    if (CY_RSLT_SUCCESS != result)
    {
        printf("MQTT Unsubscribe operation failed with error 0x%0X!\n", (int)result);
    }
}

/* END OF FILE [] */
