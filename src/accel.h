#ifndef ACCEL_H
#define ACCEL_H

void accel_start_record(void);
void accel_stop_record(void);
size_t accel_read_record(char *buf, uint16_t len);
void accel_entry(void *p1, void *p2, void *p3);

#endif