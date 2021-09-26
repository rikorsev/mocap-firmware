#include <zephyr.h>

#include "manager.h"

#define MANAGER_STACK_SIZE 2048
#define MANAGER_PRIORITY 1

K_THREAD_DEFINE(manager, MANAGER_STACK_SIZE, manager_entry, NULL, NULL, NULL, MANAGER_PRIORITY, 0, 0);