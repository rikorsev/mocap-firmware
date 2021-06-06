#include <assert.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <stdio.h>
#include <logging/log.h>
#include <drivers/gpio.h>

LOG_MODULE_REGISTER(accel);

#define STATUS_LED_NODE DT_ALIAS(led1)
#define STATUS_LED_PORT DT_GPIO_LABEL(STATUS_LED_NODE, gpios)
#define STATUS_LED_PIN DT_GPIO_PIN(STATUS_LED_NODE, gpios)

/* Write after the end of app image */
#define ACCEL_DATA_OFFSET FLASH_AREA_OFFSET(image_1)
#define FLASH_PAGE_SIZE 4096

static k_tid_t g_accel_tid = {0};
static const struct device *g_flash = NULL;
static const struct device *status_led_port = NULL;
static int g_count = 0;

struct mpu_data 
{
    struct sensor_value accel[3];
    struct sensor_value gyro[3];    
};

static void process(const struct device *dev)
{
    off_t offset = 0;
    struct mpu_data data;

    int result = 0;

    result = sensor_sample_fetch(dev);
    __ASSERT(result, "Sensor sampel fetch - fail. Result %d", result);

    result = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, data.accel);
    __ASSERT(result, "Sensor accel XYZ channel get - fail. Result %d", result);
    
    result = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, data.gyro);
    __ASSERT(result, "Sensor gyro XYZ channel get - fail. Result %d", result);
    
    /* Calculate offset to write */
    offset = ACCEL_DATA_OFFSET + g_count * sizeof(data);

    /* Write to flash */
    result = flash_write(g_flash, offset, &data, sizeof(data));
    __ASSERT(result, "Write accel data to flash - fail. Result %d", result);
    
    g_count++;

    gpio_pin_toggle(status_led_port, STATUS_LED_PIN);

    LOG_INF("Write");
}

static void storage_init(void)
{
    int result = 0;

    result = flash_write_protection_set(g_flash, false);
    __ASSERT(result, "Flash grant write access - fail. Result %d", result);

    result = flash_erase(g_flash, ACCEL_DATA_OFFSET, FLASH_PAGE_SIZE);
    __ASSERT(result, "Flash erase - fail. Result %d", result);
}

size_t accel_read_record(char *buf, uint16_t len)
{
    int result = 0;

    result = flash_read(g_flash, ACCEL_DATA_OFFSET, buf, len);
    __ASSERT(result, "Fail to read accel data from flash. Result %d", result);
    
    return len;
}

void accel_start_record(void)
{
    storage_init();

    g_count = 0;

    k_thread_resume(g_accel_tid);
}

void accel_stop_record(void)
{
    int result = 0;

    k_thread_suspend(g_accel_tid);

    gpio_pin_set(status_led_port, STATUS_LED_PIN, true);

    result = flash_write_protection_set(g_flash, true);
    __ASSERT(result >= 0, "Flash forbid write access - fail. Result %d", result);

}

void accel_entry(void *p1, void *p2, void *p3)
{
    const struct device *mpu6050 = device_get_binding(DT_LABEL(DT_INST(0, invensense_mpu6050)));
    g_flash = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

    LOG_INF("Accel entry");

    __ASSERT(mpu6050 != NULL, "Failed to find sensor %s", label);
    __ASSERT(g_flash != NULL, "Failed to find flash");

    status_led_port = device_get_binding(STATUS_LED_PORT);
    assert(connection_led_port != NULL);

    int result = gpio_pin_configure(status_led_port, STATUS_LED_PIN, GPIO_OUTPUT_ACTIVE);
    __ASSERT(result >= 0, "Failed to config status led");

    /* 
    After initialization suspend the thread and 
    wait till it be resumed by BLE command 
    */
    g_accel_tid = k_current_get();
    k_thread_suspend(g_accel_tid);

    while(true) 
    {
        process(mpu6050);

        k_sleep(K_SECONDS(1));
    }
}
