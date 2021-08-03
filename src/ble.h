#ifndef BLE_H
#define BLE_H

struct record_meta
{
    size_t size;
    uint16_t count;
};

void ble_entry(void *p1, void *p2, void *p3);

#endif