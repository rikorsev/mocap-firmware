#include <assert.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <stdio.h>
#include <logging/log.h>
#include <drivers/gpio.h>

#include "ble.h"
#include "storage.h"

LOG_MODULE_REGISTER(accel);

#define STATUS_LED_NODE DT_ALIAS(led1)
#define STATUS_LED_PORT DT_GPIO_LABEL(STATUS_LED_NODE, gpios)
#define STATUS_LED_PIN  DT_GPIO_PIN(STATUS_LED_NODE, gpios)
#define MPU6050         DT_LABEL(DT_INST(0, invensense_mpu6050))
#define PERIOD          10

struct accel_entry
{
    struct sensor_value accel[3];
    struct sensor_value gyro[3];    
};

static k_tid_t g_accel_tid = {0};
static const struct device *status_led_port = NULL;
static struct record_meta g_meta = {0};
static bool is_running = false;

static void process(const struct device *dev)
{
    struct accel_entry entry;

    int result = 0;

    result = sensor_sample_fetch(dev);
    __ASSERT(result == 0, "Sensor sample fetch - fail. Result %d", result);

    /* Get acceleromiter data */
    result = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, entry.accel);
    __ASSERT(result == 0, "Sensor accel XYZ channel get - fail. Result %d", result);
    
    /* Get gyroscope data */
    result = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, entry.gyro);
    __ASSERT(result == 0, "Sensor gyro XYZ channel get - fail. Result %d", result);
    
    /* Write to storage */
    result = storage_write(&entry, sizeof(entry));
    __ASSERT(result >= 0, "Write accel data to storage - fail. Result %d", result);
    
    /* For debug purposes */
    /* If it is first entry */   
    if(g_meta.count == 0)
    {
        printf("\r\nA: %f %f %f G: %f %f %f\r\n", 
                                        sensor_value_to_double(&entry.accel[0]),
                                        sensor_value_to_double(&entry.accel[1]),
                                        sensor_value_to_double(&entry.accel[2]),
                                        sensor_value_to_double(&entry.gyro[0]),
                                        sensor_value_to_double(&entry.gyro[1]),
                                        sensor_value_to_double(&entry.gyro[2]));
    }

    g_meta.count++;

    gpio_pin_toggle(status_led_port, STATUS_LED_PIN);
}

int accel_record_start(void)
{
    int result = 0;

    is_running = true;

    /* Clear meta data */
    memset(&g_meta, 0, sizeof(g_meta));

    k_thread_resume(g_accel_tid);

    return result;
}

int accel_record_stop(void)
{
    int result = 0;

    if(is_running == true)
    {
        k_thread_suspend(g_accel_tid);

        is_running = false;

        /* Calculate size */
        g_meta.size = g_meta.count * sizeof(struct accel_entry);

        /* Write meta data to flash */
        result = storage_meta_write(&g_meta, sizeof(g_meta));
        if(result < 0)
        {
            LOG_ERR("Fail to write meta data. Result %d", result);

            return result;
        }
        else
        {
            /* Everything is ok. Return 0 */
            result = 0;
        }

        gpio_pin_set(status_led_port, STATUS_LED_PIN, true);

        LOG_INF("Record count %d, Lenght %d", g_meta.count, 
                                        g_meta.count * sizeof(struct accel_entry));
    }

    return result;
}

void accel_entry(void *p1, void *p2, void *p3)
{
    const struct device *mpu6050 = device_get_binding(MPU6050);
    __ASSERT(mpu6050 != NULL, "Failed to find sensor %s", MPU6050);

    status_led_port = device_get_binding(STATUS_LED_PORT);
    __ASSERT(status_led_port != NULL, "Failed to find status led");

    int result = gpio_pin_configure(status_led_port, STATUS_LED_PIN, GPIO_OUTPUT_ACTIVE);
    __ASSERT(result >= 0, "Failed to config status led");

    LOG_INF("Inited successfully");

    /* 
    After initialization suspend the thread and 
    wait till it be resumed by BLE command 
    */
    g_accel_tid = k_current_get();
    k_thread_suspend(g_accel_tid);

    while(true) 
    {
        process(mpu6050);

        k_msleep(10);
    }
}
