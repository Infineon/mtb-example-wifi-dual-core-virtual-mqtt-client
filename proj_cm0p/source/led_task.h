/******************************************************************************
* File Name: led_task.h
*
* Description: This file is the public interface of led_task.c source file
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
 * Include guard
 ******************************************************************************/
#ifndef SOURCE_LED_TASK_H_
#define SOURCE_LED_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Header file includes
 ******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cy_mqtt_api.h"



/*******************************************************************************
* Global constants
*******************************************************************************/
/* 8-bit value denoting the LED state. */
#define ON_STATE                    (0x00u)
#define OFF_STATE                   (0x01u)

/*******************************************************************************
 * Data structure and enumeration
 ******************************************************************************/
/* Available LED commands */
typedef enum
{
    SUBSCRIBE_TO_TOPIC,
    UNSUBSCRIBE_FROM_TOPIC,
    UPDATE_DEVICE_STATE
} led_command_t;

/* Structure used for storing LED data */
typedef struct
{
    led_command_t command;
    uint8_t data;
} led_command_data_t;


/*******************************************************************************
 * Global variable
 ******************************************************************************/
extern QueueHandle_t led_command_data_q;

extern led_command_data_t led_cmd_data;

/*******************************************************************************
 * Function prototype
 ******************************************************************************/
void task_led(void* param);

void virtual_mqtt_subscription_callback(cy_mqtt_received_msg_info_t *received_msg_info);

extern void print_heap_usage(char *msg);

#ifdef __cplusplus
}
#endif

#endif /* SOURCE_LED_TASK_H_ */


/* [] END OF FILE  */
