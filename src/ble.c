#include <assert.h>
#include <sys/types.h>
#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <drivers/gpio.h>

#include <logging/log.h>

#include "accel.h"

#define CONNECTION_LED_NODE DT_ALIAS(led0)
#define CONNECTION_LED_PORT DT_GPIO_LABEL(CONNECTION_LED_NODE, gpios)
#define CONNECTION_LED_PIN DT_GPIO_PIN(CONNECTION_LED_NODE, gpios)

LOG_MODULE_REGISTER(ble);

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

static const struct bt_data adv[] = 
{
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1)
};

static void connected(struct bt_conn *conn, uint8_t err)
{
    LOG_INF("Connected");
    is_connected = true;
    gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, false);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    accel_stop_record();
    LOG_INF("Disconnected");
    is_connected = false;
    gpio_pin_set(connection_led_port, CONNECTION_LED_PIN, true);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static ssize_t control(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    bool is_start = *(bool *)buf;

    LOG_INF("Control");

    if(is_start == true)
    {
        LOG_INF("Start");

        accel_start_record();
    }
    else
    {
        LOG_INF("Stop");

        accel_stop_record();
    }

    return 0;
}

static ssize_t read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            void *buf, uint16_t len, uint16_t offset)
{
    static const char test_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                                      0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                                      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                                      0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
                                      0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
                                      0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
                                      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
                                      0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
                                      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 
                                      0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 
                                      0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 
                                      0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
                                      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 
                                      0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
                                      0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 
                                      0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
                                      0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 
                                      0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
                                      0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 
                                      0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
                                      0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 
                                      0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
                                      0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 
                                      0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
                                      0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 
                                      0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 
                                      0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 
                                      0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF  
                                    };
    LOG_INF("Read");
    LOG_INF("Len %d", len);

    len = sizeof(test_data) > offset + len ? len : offset - sizeof(test_data);
    memcpy(buf, test_data + offset, len);

    //len = accel_read_record((char *)buf, len);

    LOG_INF("Read %d", len);

    return len;
}

BT_GATT_SERVICE_DEFINE(mocap_service, 
BT_GATT_PRIMARY_SERVICE(&mocap_service_uuid),
BT_GATT_CHARACTERISTIC(&control_char_uuid.uuid, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                       BT_GATT_PERM_WRITE, NULL, control, NULL),
BT_GATT_CHARACTERISTIC(&record_char_uuid.uuid, BT_GATT_CHRC_READ,
                       BT_GATT_PERM_READ, read, NULL, NULL),
);

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
    assert(result >= 0);

    connection_led_port = device_get_binding(CONNECTION_LED_PORT);
    assert(connection_led_port != NULL);

    result = gpio_pin_configure(connection_led_port, CONNECTION_LED_PIN, GPIO_OUTPUT_ACTIVE);
    assert(result >= 0);

    LOG_INF("BLE inited sucessfully");

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
