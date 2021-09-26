#ifndef MANAGER_H
#define MANAGER_H

#include <stdint.h>
#include <stdlib.h>

struct record_meta
{
    size_t size;
    uint16_t count;
};

void manager_record_start(void);
void manager_record_stop(void);
void manager_storage_open(void);
void manager_storage_close(void);
void manager_record_meta_get(struct record_meta *meta);
int manager_record_read(void *buf, uint16_t len);
void manager_entry(void *p1, void *p2, void *p3);

#endif