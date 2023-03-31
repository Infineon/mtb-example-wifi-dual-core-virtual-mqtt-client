/******************************************************************************
* File Name: virtual_mqtt_task.h
*
* Description: This file is the public interface of virtual_mqtt_task.c source file
*
* Related Document: README.md
*
*******************************************************************************
* Copyright 2023, Cypress Semiconductor Corporation (an Infineon company)
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

