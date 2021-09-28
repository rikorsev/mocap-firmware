#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <stdio.h>
#include <logging/log.h>
#include <drivers/i2c.h>

#include "accel.h"

LOG_MODULE_REGISTER(accel);

#define ACCEL                DT_LABEL(DT_INST(0, invensense_mpu6050))
#define I2C                  DT_LABEL(DT_NODELABEL(i2c0))
#define PERIOD               K_MSEC(100)
#define QUEUE_SIZE           10
#define QUEUE_TIMEOUT        K_MSEC(100)
#define SAMPLE_RATE_DEVIDER  99
#define SAMPLE_RATE_REG_ADDR 0x19
#define I2C_ACCEL_ADDRESS    0x68
#define DLPF_VALUE           6

static const struct device *accle_device;
static const struct device *i2c_device;
static bool is_running = false;
static uint32_t base_timestamp = 0; // Timestamp whene record was started
static uint16_t count = 0;

void accel_timer_handler(struct k_timer *timer_id);

K_MSGQ_DEFINE(accel_queue, sizeof(struct accel_entry), QUEUE_SIZE, 4);
K_TIMER_DEFINE(accel_timer, accel_timer_handler, NULL);

static void accel_trigger_handler(const struct device *dev,
                struct sensor_trigger *trig)
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

    if (is_running != true) 
    {
        LOG_INF("Stop");

        result = sensor_trigger_set(dev, trig, NULL);
        __ASSERT(result == 0, "Accel stop - fail. Result %d", result);
    }
}

int accel_record_start(void)
{
    int result     = 0;
    is_running     = true;
    base_timestamp = k_uptime_get_32();
    count          = 0;

    static const struct sensor_trigger trigger = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ALL,
    };

    result = sensor_trigger_set(accle_device, (struct sensor_trigger *) &trigger, accel_trigger_handler);
    if(result != 0)
    {
        LOG_ERR("Sensor trigger set - fail. Result %d", result);

        return result;
    }
    
    return result;
}

int accel_record_stop(void)
{
    is_running = false;

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

int accel_sample_rate_set(const struct device *i2c)
{
    /* Switch on Digital Low-Pass filter (DLPF) to decrease sample rate to 1kHz */
    /* Set sample rate devider */
    uint8_t data[] = { SAMPLE_RATE_REG_ADDR, SAMPLE_RATE_DEVIDER, DLPF_VALUE };
	struct i2c_msg message = {0};

    message.buf   = data;
	message.len   = sizeof(data);
	message.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

    return i2c_transfer(i2c, &message, 1, I2C_ACCEL_ADDRESS);
}

void accel_init(void)
{
    int result = 0;

    accle_device = device_get_binding(ACCEL);
    __ASSERT(accle_device != NULL, "Failed to find %s", ACCEL);

    i2c_device = device_get_binding(I2C);
    __ASSERT(accle_device != NULL, "Failed to find %s", I2C);

    result = accel_sample_rate_set(i2c_device);
    __ASSERT(result == 0, "Failed to set accel sample rate. Result %d", result);
    
    LOG_INF("Inited successfully");
}