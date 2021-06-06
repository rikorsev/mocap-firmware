#include <zephyr.h>

#include "accel.h"
#include "ble.h"

#define ACCEL_STACK_SIZE 1024
#define ACCEL_PRIORITY 1

K_THREAD_DEFINE(accel, ACCEL_STACK_SIZE, accel_entry, NULL, NULL, NULL, ACCEL_PRIORITY, 0, 0);

#define BLE_STACK_SIZE 1024
#define BLE_PRIORITY 2

K_THREAD_DEFINE(ble, BLE_STACK_SIZE, ble_entry, NULL, NULL, NULL, BLE_PRIORITY, 0, 0);