#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <stdio.h>
#include <logging/log.h>
#include <drivers/gpio.h>

#include "accel.h"

LOG_MODULE_REGISTER(accel);

#define MPU6050         DT_LABEL(DT_INST(0, invensense_mpu6050))
#define PERIOD          K_MSEC(100)
#define QUEUE_SIZE      10
#define QUEUE_TIMEOUT   K_MSEC(100)

static const struct device *accle_device;
static bool is_running = false;
static uint32_t base_timestamp = 0; // Timestamp whene record was started
static uint16_t count = 0;

void accel_timer_handler(struct k_timer *timer_id);

K_MSGQ_DEFINE(accel_queue, sizeof(struct accel_entry), QUEUE_SIZE, 4);
K_TIMER_DEFINE(accel_timer, accel_timer_handler, NULL);

void accel_timer_handler(struct k_timer *timer_id)
{
    struct accel_entry entry;

    int result = 0;
    
    /* Get timestamp */
    entry.timestamp = k_uptime_get_32() - base_timestamp;

    /* Fetch accelerometr data */
    result = sensor_sample_fetch(accle_device);
    __ASSERT(result == 0, "Sensor sample fetch - fail. Result %d", result);

    /* Get acceleromiter data */
    result = sensor_channel_get(accle_device, SENSOR_CHAN_ACCEL_XYZ, entry.accel);
    __ASSERT(result == 0, "Sensor accel XYZ channel get - fail. Result %d", result);
    
    /* Get gyroscope data */
    result = sensor_channel_get(accle_device, SENSOR_CHAN_GYRO_XYZ, entry.gyro);
    __ASSERT(result == 0, "Sensor gyro XYZ channel get - fail. Result %d", result);

    /* Put data to queue */
    result = k_msgq_put(&accel_queue, &entry, QUEUE_TIMEOUT);
    __ASSERT(result == 0, "Add data to queue - fail. Result %d", result);

    count++;
}

int accel_record_start(void)
{
    is_running     = true;
    base_timestamp = k_uptime_get_32();
    count          = 0;

    k_timer_start(&accel_timer, PERIOD, PERIOD);
    
    return 0;
}

int accel_record_stop(void)
{
    if(is_running == true)
    {
        k_timer_stop(&accel_timer);

        is_running = false;
    }

    return 0;
}

struct k_msgq *accel_queue_get(void)
{
    return &accel_queue;
}

uint16_t accel_count_get(void)
{
    return count;
}

bool accel_is_running(void)
{
    return is_running;
}

void accel_init(void)
{
    accle_device = device_get_binding(MPU6050);
    __ASSERT(accle_device != NULL, "Failed to find sensor %s", MPU6050);

    LOG_INF("Inited successfully");
}