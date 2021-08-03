#include <assert.h>
#include <sys/types.h>
#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <drivers/gpio.h>
#include <fs/fs.h>

#include <logging/log.h>

#include "accel.h"
#include "storage.h"
#include "ble.h"

#define CONNECTION_LED_NODE DT_ALIAS(led0)
#define CONNECTION_LED_PORT DT_GPIO_LABEL(CONNECTION_LED_NODE, gpios)
#define CONNECTION_LED_PIN DT_GPIO_PIN(CONNECTION_LED_NODE, gpios)

LOG_MODULE_REGISTER(ble);

enum
{
    BLE_STOP_CMD,
    BLE_START_CMD,
    BLE_OPEN_STORAGE_CMD,
    BLE_CLOES_STORAGE_CMD,
    BLE_GET_META_CMD
};

static const struct device *connection_led_port = NULL;
static bool is_connected = false;

static struct bt_uuid_128 mocap_service_uuid = BT_UUID_INIT_128(
    0x03, 0x00, 0x13, 0xac, 0x42, 0x02, 0xeb, 0x8d,
    0xeb, 0x11, 0xe9, 0x81, 0x98, 0x82, 0x04, 0x06);

static struct bt_uuid_128 control_char_uuid = BT_UUID_INIT_128(
    0x03, 0x00, 0x13, 0xac, 0x42, 0x02, 0xeb, 0x8d,
    0xeb, 0x11, 0xe9, 0x81, 0x99, 0x82, 0x04, 0x06);

static struct bt_uuid_128 record_char_uuid = BT_UUID_INIT_128(
    0x03, 0x00, 0x13, 0xac, 0x42, 0x02, 0xeb, 0x8d,
    0xeb, 0x11, 0xe9, 0x81, 0x9a, 0x82, 0x04, 0x06);

/* Propotype of control callback */
static ssize_t control(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                       const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

/* Prototype of read callback */ 
static ssize_t read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            void *buf, uint16_t len, uint16_t offset);

BT_GATT_SERVICE_DEFINE(mocap_service, 
BT_GATT_PRIMARY_SERVICE(&mocap_service_uuid),
BT_GATT_CHARACTERISTIC(&control_char_uuid.uuid, BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
                       BT_GATT_PERM_WRITE, NULL, control, NULL),
BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
BT_GATT_CHARACTERISTIC(&record_char_uuid.uuid, BT_GATT_CHRC_READ,
                       BT_GATT_PERM_READ, read, NULL, NULL),
);

static const struct bt_data adv[] = 
{
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1)
};

static void stop_cmd_handler(void)
{
    int result = 0;

    LOG_INF("Stop");

    result = accel_record_stop();
    __ASSERT(result == 0, "Fail to stop recording. Result %d", result);

    /* Close storage */
    result = storage_close();
    __ASSERT(result == 0, "Fail to close storage. Result %d", result); 
}

static void start_cmd_handler(void)
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
    __ASSERT(result == 0, "Fail to stop recording. Result %d", result);
}

static void open_storage_cmd_handler(void)
{
    int result = 0;

    LOG_INF("Open storage");

    result = storage_open();
    __ASSERT(result == 0, "Fail to open storage. Result %d", result);
}

static void close_storage_cmd_handler(void)
{
    int result = 0;

    LOG_INF("Close storage");

    result = storage_close();
    __ASSERT(result == 0, "Fail to close storage. Result %d", result);
}

static void get_meta_cmd_handler(struct bt_conn *conn)
{
    int result = 0;
    struct record_meta meta = {0};

    LOG_INF("Get meta");

    result = storage_meta_read(&meta, sizeof(meta));
    if(result < 0)
    {
        LOG_INF("Fail to read meta. Result %d", result);

        return;
    }

    result = bt_gatt_notify(conn, &mocap_service.attrs[2], &meta, sizeof(meta));
    __ASSERT(result == 0, "Fail to send BLE notify. Result %d", result);

    LOG_INF("Notification send. Meta size: %d, count %d", meta.size, meta.count);
}

static ssize_t control(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    uint8_t cmd = *(uint8_t *)buf;

    LOG_INF("Control");

    switch(cmd)
    {
        case BLE_STOP_CMD:
            stop_cmd_handler();
        break;

        case BLE_START_CMD:
            start_cmd_handler();
        break;
              
        case BLE_OPEN_STORAGE_CMD:
            open_storage_cmd_handler();
        break;

        case BLE_CLOES_STORAGE_CMD:
            close_storage_cmd_handler();
        break;

        case BLE_GET_META_CMD:
            get_meta_cmd_handler(conn);
        break;

        default:
            __ASSERT(0, "Unknown commadn, %d", cmd);
    }

    return 0;
}

static ssize_t read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Read");

    ssize_t result = storage_read(buf, len);
    __ASSERT(result >= 0, "Storage read - fail. Result %d", result);

    len = (uint16_t) result;

    LOG_INF("Len %d",  len);

    return len;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    LOG_INF("Connected");
    is_connected = true;
    gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, false);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    int result = 0;

    /* Stop record first */
    stop_cmd_handler();

    LOG_INF("Disconnected");
    is_connected = false;
    gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, true);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static int ble_init(void)
{
    int result = 0;

    result = bt_enable(NULL);
    if(result < 0)
    {
        LOG_ERR("Bluetooth enable - fail. Result %d", result);
        return result;
    }

    bt_conn_cb_register(&conn_callbacks);

    result = bt_le_adv_start(BT_LE_ADV_CONN, adv, ARRAY_SIZE(adv), NULL, 0);
    if(result < 0)
    {
        LOG_ERR("Advertising start - fail. Result %d", result);
        return result;
    }

    return result;
}

void ble_entry(void *p1, void *p2, void *p3)
{
    int result = 0;
    
    result = ble_init();
    __ASSERT(result == 0, "Init - fail. Result %d", result);

    connection_led_port = device_get_binding(CONNECTION_LED_PORT);
    __ASSERT(connection_led_port != NULL, "Fali to find connection LED. Result %d", result);

    result = gpio_pin_configure(connection_led_port, CONNECTION_LED_PIN, GPIO_OUTPUT_ACTIVE);
    __ASSERT(result == 0, "Config connection LED - fail. Result %d", result);

    /* TODO: move to some generic init module ? */
    storage_init();

    LOG_INF("Inited sucessfully");

    while(true)
    {
        if(is_connected == false)
        {
            gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, false);
            k_sleep(K_MSEC(50));
            gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, true);
            k_sleep(K_MSEC(950));            
        }
        else
        {
            gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, false);
            k_sleep(K_MSEC(950));
            gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, true);
            k_sleep(K_MSEC(50));  
        }
    }
}
