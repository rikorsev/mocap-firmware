#ifndef ACCEL_H
#define ACCEL_H

#include <drivers/sensor.h>

struct accel_entry
{
    uint32_t timestamp;
    struct sensor_value accel[3];
    struct sensor_value gyro[3];    
};

void accel_init(void);
int accel_record_start(void);
int accel_record_stop(void);
struct k_msgq *accel_queue_get(void);
uint16_t accel_count_get(void);
bool accel_is_running(void);

#endif