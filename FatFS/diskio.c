/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs     (C)ChaN, 2014                  */
/*-----------------------------------------------------------------------*/
/* 适配STM32F407ZGT6的SDIO接口                                           */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "stm32f4xx.h"	/* STM32F4系列头文件 */
#include "delay.h"		/* 延时函数 */
#include "sdio_sdcard.h"	/* 官方SDIO驱动 */
#include <stdlib.h>		/* 包含NULL定义 */
#include <string.h>		/* 包含字符串处理函数 */

/* Definitions of physical drive number for each drive */
#define SD_CARD		0	/* Map SD card to physical drive 0 */

/* SD卡状态 */
static volatile DSTATUS Stat = STA_NOINIT;

/* SDIO相关变量 */
static SDIO_InitTypeDef  SDIO_InitStructure;
static SDIO_CmdInitTypeDef  SDIO_CmdInitStructure;
static SDIO_DataInitTypeDef SDIO_DataInitStructure;

/* 函数声明 */
static u8 SDIO_SendCmd(u8 cmd, u32 arg, u32 resp_type, u32* resp);
static u8 SDIO_ReadBlock(u32 sector, u8* buffer, u16 size);
static u8 SDIO_WriteBlock(u32 sector, const u8* buffer, u16 size);
static u8 SD_Card_Init(void);


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if (pdrv != SD_CARD) return STA_NOINIT;
	return Stat;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv			/* Physical drive nmuber to identify the drive */
)
{
	if (pdrv != SD_CARD) return STA_NOINIT;
	
	if (SD_Init() == SD_OK) {
		Stat &= ~STA_NOINIT;
		return Stat;
	} else {
		Stat |= STA_NOINIT;
		return Stat;
	}
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv != SD_CARD) return RES_PARERR;
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	
	// 读取扇区
	if (SD_ReadDisk(buff, sector, count) == SD_OK) {
		return RES_OK;
	} else {
		return RES_ERROR;
	}
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv != SD_CARD) return RES_PARERR;
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	if (Stat & STA_PROTECT) return RES_WRPRT;
	
	// 写入扇区
	if (SD_WriteDisk((u8*)buff, sector, count) == SD_OK) {
		return RES_OK;
	} else {
		return RES_ERROR;
	}
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_OK;
	
	if (pdrv != SD_CARD) return RES_PARERR;
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	
	switch (cmd) {
	case CTRL_SYNC :
		// 同步操作，确保数据写入完成
		break;
		
	case GET_SECTOR_COUNT :
		// 获取扇区总数
		*(DWORD*)buff = 0x100000; // 假设8GB SD卡，实际应从SD卡获取
		break;
		
	case GET_SECTOR_SIZE :
		// 获取扇区大小
		*(WORD*)buff = 512;
		break;
		
	case GET_BLOCK_SIZE :
		// 获取块大小
		*(DWORD*)buff = 1;
		break;
		
	default:
		res = RES_PARERR;
		break;
	}
	
	return res;
}
#endif


/*-----------------------------------------------------------------------*/
/* SDIO驱动函数已被官方SDIO样例替代                                     */
/*-----------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* 获取FAT时间函数（FATFS需要）                                          */
/*-----------------------------------------------------------------------*/

DWORD get_fattime(void)
{
	// 简单的时间实现，返回固定时间
	// 实际项目中应从RTC获取真实时间
	return ((2026 - 1980) << 25) | (2 << 21) | (8 << 16) | (12 << 11) | (0 << 5) | (0);
}
