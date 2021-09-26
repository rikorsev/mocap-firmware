#include <sys/types.h>
#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <logging/log.h>

#include "manager.h"

LOG_MODULE_REGISTER(ble);

enum
{
    BLE_STOP_CMD,
    BLE_START_CMD,
    BLE_OPEN_STORAGE_CMD,
    BLE_CLOES_STORAGE_CMD,
    BLE_GET_META_CMD
};

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

static ssize_t control(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
                       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    int result = 0;
    struct record_meta meta = {0};

    uint8_t cmd = *(uint8_t *)buf;

    LOG_INF("Control");

    switch(cmd)
    {
        case BLE_STOP_CMD:
            manager_record_stop();
        break;

        case BLE_START_CMD:
            manager_record_start();
        break;
              
        case BLE_OPEN_STORAGE_CMD:
            manager_storage_open();
        break;

        case BLE_CLOES_STORAGE_CMD:
            manager_storage_close();
        break;

        case BLE_GET_META_CMD:
            manager_record_meta_get(&meta);

            result = bt_gatt_notify(conn, &mocap_service.attrs[2], &meta, sizeof(meta));
            __ASSERT(result == 0, "Fail to send BLE notify. Result %d", result);
        break;

        default:
            __ASSERT(0, "Unknown commadn, %d", cmd);
    }

    return 0;
}

static ssize_t read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
            void *buf, uint16_t len, uint16_t offset)
{
    return (uint16_t)manager_record_read(buf, len);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    LOG_INF("Connected");
    is_connected = true;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    /* Stop record first */
    manager_record_stop();

    LOG_INF("Disconnected");
    is_connected = false;
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

bool ble_is_connected(void)
{
    return is_connected;
}

void ble_init(void)
{
    int result = 0;

    result = bt_enable(NULL);
    __ASSERT(result == 0, "Bluetooth enable - fail. Result %d", result);
    
    bt_conn_cb_register(&conn_callbacks);

    result = bt_le_adv_start(BT_LE_ADV_CONN, adv, ARRAY_SIZE(adv), NULL, 0);
    __ASSERT(result == 0, "Advertising start - fail. Result %d", result);

    LOG_INF("Inited sucessfully");
}
