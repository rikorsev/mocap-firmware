#include <zephyr.h>
#include <device.h>
#include <disk/disk_access.h>
#include <logging/log.h>
#include <fs/fs.h>
#include <ff.h>
#include <sys/types.h>

LOG_MODULE_REGISTER(storage);

static FATFS fat_fs;

/* mounting info */
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

K_MUTEX_DEFINE(storage_mutex);

/* pointer to storage descriptor */
static struct fs_file_t storage;
static struct fs_file_t meta;

static bool is_opened = false;

void storage_init(void)
{
    int result = 0;
    
    /*
    *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
    *  in ffconf.h
    */
    mp.mnt_point = "/SD:";

    result = fs_mount(&mp);
    __ASSERT(result == 0, "storage init - fail. Result %d", result);

    LOG_INF("Init success");
}

static int storage_open_mocap_file(void)
{
    int result = 0;
    static const char path[] = "/SD:/MOCAP.DAT";

    fs_file_t_init(&storage);

    result = fs_open(&storage, path, FS_O_RDWR | FS_O_CREATE);
    if(result != 0)
    {
        LOG_ERR("Open %s - fail. Result %d", path, result);

        return result;
    }

    return result;
}

static int storage_open_meta_file(void)
{
    int result = 0;
    static const char path[] = "/SD:/META.DAT";

    fs_file_t_init(&meta);

    result = fs_open(&meta, path, FS_O_RDWR | FS_O_CREATE);
    if(result != 0)
    {
        LOG_ERR("Open %s - fail. Result %d", path, result);

        return result;
    }

    return result;
}

int storage_close(void)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    if(is_opened == true)
    {
        result = fs_close(&storage);
        if(result == 0)
        {
            result = fs_close(&meta);
            if(result == 0)
            {
                is_opened = false;

                LOG_INF("Close success");
            }
        }
    }

    k_mutex_unlock(&storage_mutex);

    return result;
}

int storage_open(void)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    /* close storage first if it is already opened */
    if(is_opened == true)
    {
        result = storage_close();
        if(result != 0)
        {
            LOG_ERR("Fail to close on open. Result %d", result);
            goto exit;
        }
    }

    result = storage_open_mocap_file();
    if(result == 0)
    {
        result = storage_open_meta_file();
        if(result == 0)
        {
            is_opened = true;

            LOG_INF("Open success");
        }
    }
    
exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}

int storage_clear(void)
{
    int result = 0; 

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    /* Truncate to zero (clear all previous data)*/
    result = fs_truncate(&storage, 0);
    if(result != 0)
    {
        LOG_ERR("Truncate storage - fail. Result %d", result);

        goto exit;
    }

    result = fs_truncate(&meta, 0);
    if(result != 0)
    {
        LOG_ERR("Truncate meta - fail. Result %d", result);

        goto exit;
    }

exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}

ssize_t storage_write(void *data, size_t size)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    result = fs_write(&storage, data, size);
    if(result < 0)
    {
        LOG_ERR("Write - fail. Result %d", result);

        goto exit;
    }

exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}

ssize_t storage_read(void *data, size_t size)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    result = fs_read(&storage, data, size);
    if(result < 0)
    {
        LOG_ERR("Read - fail. Result %d", result);

        goto exit;
    }

exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}

ssize_t storage_meta_write(void *data, size_t size)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    result = fs_write(&meta, data, size);
    if(result < 0)
    {
        LOG_ERR("Meta write - fail. Result %d", result);

        goto exit;
    }

exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}

ssize_t storage_meta_read(void *data, size_t size)
{
    int result = 0;

    result = k_mutex_lock(&storage_mutex, K_FOREVER);
    if(result != 0)
    {
        LOG_ERR("Lock mutex - fail. Result %d", result);

        return result;
    }

    result = fs_read(&meta, data, size);
    if(result < 0)
    {
        LOG_ERR("Meta read - fail. Result %d", result);

        goto exit;
    }

exit:
    k_mutex_unlock(&storage_mutex);

    return result;
}