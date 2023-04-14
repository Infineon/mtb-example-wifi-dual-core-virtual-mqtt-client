/******************************************************************************
* File Name: virtual_mqtt_task.h
*
* Description: This file is the public interface of virtual_mqtt_task.c source file
*
* Related Document: README.md
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
#ifndef SOURCE_VIRTUAL_MQTT_TASK_H_
#define SOURCE_VIRTUAL_MQTT_TASK_H_

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
 * Global variable
 ******************************************************************************/
/* Priorities of user tasks in this project. configMAX_PRIORITIES is defined in
 * the FreeRTOSConfig.h and higher priority numbers denote high priority tasks.
 */
#define TASK_VIRTUAL_PRIORITY (configMAX_PRIORITIES - 1)

/* Stack sizes of user tasks in this project */
#define TASK_VIRTUAL_STACK_SIZE (5*1024u)

/* Commands for the Virtual MQTT Client Task. */
typedef enum
{
    HANDLE_VIRTUAL_MQTT_SUBSCRIBE_FAILURE,
    HANDLE_VIRTUAL_MQTT_PUBLISH_FAILURE,
    HANDLE_VIRTUAL_MQTT_DISCONNECTION
} virtual_mqtt_task_cmd_t;

extern volatile uint8_t wifi_status;
extern QueueHandle_t virtual_mqtt_task_data_q;
extern volatile uint8_t init_done;
extern cy_mqtt_t virtual_mqtt_connection;

extern virtual_mqtt_task_cmd_t virtual_mqtt_task_cmd;
/*******************************************************************************
 * Function prototype
 ******************************************************************************/
void virtual_mqtt_task(void* param);

#ifdef __cplusplus
}
#endif

#endif /* SOURCE_VIRTUAL_MQTT_TASK_H_ */

/* [] END OF FILE  */

