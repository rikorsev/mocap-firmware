#ifndef STORAGE_H
#define STORAGE_H

#include <sys/types.h>

void storage_init(void);
int storage_open(void);
int storage_clear(void);
ssize_t storage_write(void *data, size_t size);
ssize_t storage_read(void *data, size_t size);
ssize_t storage_meta_write(void *data, size_t size);
ssize_t storage_meta_read(void *data, size_t size);
int storage_close(void);

#endif