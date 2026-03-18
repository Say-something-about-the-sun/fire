// SDCard.c - SD卡操作封装
// 功能：提供SD卡初始化、挂载、文件读写等接口

#include "SDCard.h"
#include "delay.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FatFS文件系统对象
static FATFS fs;
static FIL file;
static u8 sdcard_status = SDCARD_ERROR;

// 初始化SD卡
u8 SDCard_Init(void)
{
    printf("[SDCard] Initializing SDIO...\r\n");
    
    // 这里使用diskio.c中的SDIO初始化函数
    // 如果需要自定义初始化，可以在这里添加
    
    if(disk_initialize(0) == RES_OK)
    {
        printf("[SDCard] SD card initialized successfully\r\n");
        sdcard_status = SDCARD_OK;
        return SDCARD_OK;
    }
    else
    {
        printf("[SDCard] SD card initialization failed\r\n");
        sdcard_status = SDCARD_ERROR;
        return SDCARD_ERROR;
    }
}

// 挂载文件系统
u8 SDCard_Mount(void)
{
    FRESULT res;
    
    printf("[SDCard] Mounting file system...\r\n");
    
    res = f_mount(&fs, "", 1);
    if(res == FR_OK)
    {
        printf("[SDCard] File system mounted successfully\r\n");
        return SDCARD_OK;
    }
    else if(res == FR_NO_FILESYSTEM)
    {
        printf("[SDCard] No file system detected, attempting to format...\r\n");
        // 尝试格式化SD卡
        res = f_mkfs("", 0, 4096);
        if(res == FR_OK)
        {
            printf("[SDCard] Formatting successful\r\n");
            // 重新挂载
            res = f_mount(&fs, "", 1);
            if(res == FR_OK)
            {
                printf("[SDCard] Mounted successfully after formatting\r\n");
                return SDCARD_OK;
            }
        }
        printf("[SDCard] File system mount failed, error code: %d\r\n", res);
        return SDCARD_MOUNT_ERROR;
    }
    else
    {
        printf("[SDCard] File system mount failed, error code: %d\r\n", res);
        return SDCARD_MOUNT_ERROR;
    }
}

// 卸载文件系统
u8 SDCard_Unmount(void)
{
    FRESULT res;
    
    res = f_mount(NULL, "", 1);
    if(res == FR_OK)
    {
        printf("[SDCard] File system unmounted successfully\r\n");
        return SDCARD_OK;
    }
    else
    {
        printf("[SDCard] File system unmount failed\r\n");
        return SDCARD_ERROR;
    }
}

// 创建目录
u8 SDCard_CreateDir(const char* path)
{
    FRESULT res;
    
    res = f_mkdir(path);
    if(res == FR_OK || res == FR_EXIST)
    {
        printf("[SDCard] Directory created/exists: %s\r\n", path);
        return SDCARD_OK;
    }
    else
    {
        printf("[SDCard] Directory creation failed: %s, error code: %d\r\n", path, res);
        return SDCARD_DIR_ERROR;
    }
}

// 写入CSV文件
u8 SDCard_WriteCSV(const char* filename, const char* data)
{
    FRESULT res;
    UINT bytes_written;
    
    // 确保文件系统已挂载
    if(SDCard_GetStatus() != SDCARD_OK)
    {
        printf("[SDCard] File system not mounted\r\n");
        if(SDCard_Mount() != SDCARD_OK)
        {
            return SDCARD_WRITE_ERROR;
        }
    }
    
    // 创建目录结构
    char dir_path[64];
    strcpy(dir_path, filename);
    char* last_slash = (char*)strrchr(dir_path, '/');
    if(last_slash != NULL)
    {
        *last_slash = '\0';
        SDCard_CreateDir(dir_path);
    }
    
    // 打开文件，追加模式
    res = f_open(&file, filename, FA_OPEN_ALWAYS | FA_WRITE);
    if(res != FR_OK)
    {
        printf("[SDCard] Failed to open CSV file: %s, error code: %d\r\n", filename, res);
        return SDCARD_WRITE_ERROR;
    }
    
    // 移动到文件末尾
    f_lseek(&file, f_size(&file));
    
    // 写入数据
    res = f_write(&file, data, strlen(data), &bytes_written);
    if(res != FR_OK || bytes_written != strlen(data))
    {
        printf("[SDCard] Failed to write CSV file, error code: %d\r\n", res);
        f_close(&file);
        return SDCARD_WRITE_ERROR;
    }
    
    // 关闭文件
    f_close(&file);
    
    printf("[SDCard] CSV file written successfully: %s\r\n", filename);
    return SDCARD_OK;
}

// 写入JSON文件
u8 SDCard_WriteJSON(const char* filename, const char* data)
{
    FRESULT res;
    UINT bytes_written;
    
    // 确保文件系统已挂载
    if(SDCard_GetStatus() != SDCARD_OK)
    {
        printf("[SDCard] File system not mounted\r\n");
        if(SDCard_Mount() != SDCARD_OK)
        {
            return SDCARD_WRITE_ERROR;
        }
    }
    
    // 创建目录结构
    char dir_path[64];
    strcpy(dir_path, filename);
    char* last_slash = (char*)strrchr(dir_path, '/');
    if(last_slash != NULL)
    {
        *last_slash = '\0';
        SDCard_CreateDir(dir_path);
    }
    
    // 打开文件，写入模式（覆盖）
    res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if(res != FR_OK)
    {
        printf("[SDCard] Failed to open JSON file: %s, error code: %d\r\n", filename, res);
        return SDCARD_WRITE_ERROR;
    }
    
    // 写入数据
    res = f_write(&file, data, strlen(data), &bytes_written);
    if(res != FR_OK || bytes_written != strlen(data))
    {
        printf("[SDCard] Failed to write JSON file, error code: %d\r\n", res);
        f_close(&file);
        return SDCARD_WRITE_ERROR;
    }
    
    // 关闭文件
    f_close(&file);
    
    printf("[SDCard] JSON file written successfully: %s\r\n", filename);
    return SDCARD_OK;
}

// 获取SD卡状态
u8 SDCard_GetStatus(void)
{
    return sdcard_status;
}
