#ifndef __SDCARD_H
#define __SDCARD_H

#include "sys.h"

// 函数声明
u8 SDCard_Init(void);
u8 SDCard_Mount(void);
u8 SDCard_Unmount(void);
u8 SDCard_CreateDir(const char* path);
u8 SDCard_WriteCSV(const char* filename, const char* data);
u8 SDCard_WriteJSON(const char* filename, const char* data);
u8 SDCard_GetStatus(void);

// 错误代码
#define SDCARD_OK              0
#define SDCARD_ERROR           1
#define SDCARD_MOUNT_ERROR     2
#define SDCARD_WRITE_ERROR     3
#define SDCARD_DIR_ERROR       4

#endif /* __SDCARD_H */
