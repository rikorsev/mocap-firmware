#include <zephyr.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <stdio.h>

#include "accel.h"
#include "ble.h"
#include "storage.h"
#include "manager.h"

LOG_MODULE_REGISTER(manager);

#define STATUS_LED_NODE DT_ALIAS(led1)
#define STATUS_LED_PORT DT_GPIO_LABEL(STATUS_LED_NODE, gpios)
#define STATUS_LED_PIN  DT_GPIO_PIN(STATUS_LED_NODE, gpios)
#define CONNECTION_LED_NODE DT_ALIAS(led0)
#define CONNECTION_LED_PORT DT_GPIO_LABEL(CONNECTION_LED_NODE, gpios)
#define CONNECTION_LED_PIN DT_GPIO_PIN(CONNECTION_LED_NODE, gpios)
#define SHORT_PHASE K_MSEC(50)
#define LONG_PHASE  K_MSEC(950)


static const struct device *status_led_port = NULL;
static const struct device *connection_led_port = NULL;

void connection_led_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(connection_led_timer, connection_led_handler, NULL);

void connection_led_handler(struct k_timer *timer_id)
{
    static bool led_state = false;
    k_timeout_t phase;

    int result = gpio_pin_toggle(connection_led_port, CONNECTION_LED_PIN);
    __ASSERT(result == 0, "Fail to toggle connection LED. Result %d", result);

    led_state ^= 1;

    if(ble_is_connected() == false)
    {
        if(led_state == false)
        {
            phase = LONG_PHASE;
        }
        else
        {
            phase = SHORT_PHASE;
        }
    }
    else
    {
        if(led_state == false)
        {
            phase = SHORT_PHASE;
        }
        else
        {
            phase = LONG_PHASE;
        }
    }

    k_timer_start(&connection_led_timer, phase, K_MSEC(0));
}

void manager_record_start(void)
{
    int result = 0;

    LOG_INF("Start");

    /* Open storage */
    result = storage_open();
    __ASSERT(result == 0, "Fail to open storage. Result %d", result);

    /* and clear previous data */
    result = storage_clear();
    __ASSERT(result == 0, "Fail to clear storage. Result %d", result);

    result = accel_record_start();
    __ASSERT(result == 0, "Failed to start record. Result %d", result);
}

void manager_record_stop(void)
{
    int result = 0;
    struct record_meta meta = {0};

    LOG_INF("Stop");

    if(accel_is_running() == true)
    {
        result = accel_record_stop();
        __ASSERT(result == 0, "Failed to stop record. Result %d", result);

        /* Calculate size */
        meta.count = accel_count_get();
        meta.size = meta.count * sizeof(struct accel_entry);

        /* Write meta data to flash */
        result = storage_meta_write(&meta, sizeof(meta));
        __ASSERT(result == 0, "Failed to store meta data. Result %d", result);

        gpio_pin_set(status_led_port, STATUS_LED_PIN, true);

        LOG_INF("Record count %d, Lenght %d", meta.count, 
                                        meta.count * sizeof(struct accel_entry));

        /* Close storage */
        result = storage_close();
        __ASSERT(result == 0, "Fail to close storage. Result %d", result); 
    }
}

void manager_storage_open(void)
{
    int result = 0;

    LOG_INF("Open storage");

    result = storage_open();
    __ASSERT(result == 0, "Fail to open storage. Result %d", result);
}

void manager_storage_close(void)
{
    int result = 0;

    LOG_INF("Close storage");

    result = storage_close();
    __ASSERT(result == 0, "Fail to close storage. Result %d", result);
}

void manager_record_meta_get(struct record_meta *meta)
{
    int result = 0;

    LOG_INF("Get meta");

    result = storage_meta_read(&meta, sizeof(meta));
    __ASSERT(result == 0, "Fail to read meta. Result %d", result);

    LOG_INF("Meta size: %d, count %d", meta->size, meta->count);
}

int manager_record_read(void *buf, uint16_t len)
{
    LOG_INF("Read");

    ssize_t result = storage_read(buf, len);
    __ASSERT(result >= 0, "Storage read - fail. Result %d", result);

    LOG_INF("Len %d",  result);

    return result;
}

static void manager_entry_process(void)
{
    int result = 0;
    struct accel_entry entry;

    /* Get data from queue */
    result = k_msgq_get(accel_queue_get(), &entry, K_FOREVER);
    if(result == 0)
    {
        /* Write to storage */
        result = storage_write(&entry, sizeof(entry));
        __ASSERT(result >= 0, "Write accel data to storage - fail. Result %d", result);

        /* For debug purposes */
        /* Print data on each 100 records */
        if(accel_count_get() % 100 == 0)
        {
            printf("\r\n[%08d] A: %f %f %f G: %f %f %f\r\n", entry.timestamp,
                                            sensor_value_to_double(&entry.accel[0]),
                                            sensor_value_to_double(&entry.accel[1]),
                                            sensor_value_to_double(&entry.accel[2]),
                                            sensor_value_to_double(&entry.gyro[0]),
                                            sensor_value_to_double(&entry.gyro[1]),
                                            sensor_value_to_double(&entry.gyro[2]));
        }

        gpio_pin_toggle(status_led_port, STATUS_LED_PIN);
    }
}

static void manager_status_led_init(void)
{
    int result = 0;
    
    status_led_port = device_get_binding(STATUS_LED_PORT);
    __ASSERT(status_led_port != NULL, "Failed to find status led");

    result = gpio_pin_configure(status_led_port, STATUS_LED_PIN, GPIO_OUTPUT_ACTIVE);
    __ASSERT(result >= 0, "Failed to config status led");
}

static void manager_connection_led_init(void)
{
    int result = 0;

    connection_led_port = device_get_binding(CONNECTION_LED_PORT);
    __ASSERT(connection_led_port != NULL, "Fali to find connection LED. Result %d", result);

    result = gpio_pin_configure(connection_led_port, CONNECTION_LED_PIN, GPIO_OUTPUT_ACTIVE);
    __ASSERT(result == 0, "Config connection LED - fail. Result %d", result);
}

static void manager_init(void)
{
    manager_status_led_init();
    manager_connection_led_init();
    storage_init();
    ble_init();
    accel_init();
}

void manager_entry(void *p1, void *p2, void *p3)
{
    manager_init();

    k_timer_start(&connection_led_timer, SHORT_PHASE, K_MSEC(0));

    while(true)
    {
        manager_entry_process();
    }
}