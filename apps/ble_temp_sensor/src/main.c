/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <nrf_temp.h>

#include <os/mynewt.h>
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

#include "ble_temp_sensor.h"
#include "gatt_svr.h"

/* Log data */
struct log logger;

static const char *device_name = "Andrew_temp_sensor";

static int ble_temp_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t ble_temp_addr_type;

static const uint32_t TEMPERATURE_PERIOD = 100; 

/* Define task stack and task object */
#define TASK1_TASK_PRI         (1)
#define TASK1_STACK_SIZE       (64)
struct os_task task1;
os_stack_t task1_stack[TASK1_STACK_SIZE];

/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void
ble_temp_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                    BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble_temp_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_temp_gap_event, NULL);
    if (rc != 0) {
        LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}


static int
ble_temp_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
        LOG(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising */
            ble_temp_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        LOG(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

        /* Connection terminated; resume advertising */
        ble_temp_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        LOG(INFO, "adv complete\n");
        ble_temp_advertise();
        break;


    case BLE_GAP_EVENT_MTU:
        LOG(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;

    }

    return 0;
}

static void
on_sync(void)
{
    int rc;

    /* Use privacy */
    rc = ble_hs_id_infer_auto(0, &ble_temp_addr_type);
    assert(rc == 0);

    /* Begin advertising */
    ble_temp_advertise();

    LOG(INFO, "adv started\n");
}

/* Task 1 handler function */
void
task1_handler(void *arg)
{
    while (1) {

        os_time_delay(TEMPERATURE_PERIOD);

    }
}

/* Initialize task 1 exposed data objects */
void
task1_init(void)
{

}

/**
* init_app_tasks
*  
* This function performs initializations that are required before tasks run. 
*  
* @return int 0 success; error otherwise.
*/
static int
init_app_tasks(void)
{
    /* 
    * Call task specific initialization functions to initialize any shared objects 
    * before initializing the tasks with the OS.
    */
    task1_init();

    /*
    * Initialize task 1 with the OS. 
    */
    os_task_init(&task1, "task1", task1_handler, NULL, TASK1_TASK_PRI, 
                    OS_WAIT_FOREVER, task1_stack, TASK1_STACK_SIZE);

    return 0;
}

/*
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int rc;

    /* Initialize OS */
    sysinit();

    /* Initialize the logger */
    log_register("ble_temp_sensor_log", &logger, &log_console_handler, NULL, LOG_SYSLEVEL);

    LOG(INFO, "hello\n");

    /* Prepare the internal temperature module for measurement */
    nrf_temp_init();

    /* Prepare BLE host and GATT server */
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    /* Initialize application specific tasks */
    init_app_tasks();

    /* As the last thing, process events from default event queue */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}

