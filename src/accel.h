#ifndef ACCEL_H
#define ACCEL_H

int accel_record_start(void);
int accel_record_stop(void);
size_t accel_read_record(char *buf, uint16_t len, uint16_t offset);
void accel_start(void *p1, void *p2, void *p3);

#endif