#include "sdcard_jpeg.h"
#include "diskio.h"
#include "delay.h"
#include "stdio.h"
#include "string.h"
#include "ov5640.h"

// 文件系统对象
static FATFS fs;
static FIL file;

// 文件计数器
static u32 file_count = 0;

// SD卡状态
static u8 sdcard_ready = 0;

// 初始化SD卡JPEG保存模块
u8 sdcard_jpeg_init(void)
{
    FRESULT res;
    
    printf("[SDCard] Initializing...\r\n");
    
    // 挂载文件系统
    res = f_mount(&fs, "0:", 1);  // 1 = 立即挂载
    
    if(res == FR_OK) {
        printf("[SDCard] File system mounted successfully!\r\n");
        sdcard_ready = 1;
        
        // 获取SD卡信息
        u32 total_space = 0;
        u32 free_space = 0;
        sdcard_get_info(&total_space, &free_space);
        
        printf("[SDCard] Total space: %lu MB\r\n", total_space / (1024 * 1024));
        printf("[SDCard] Free space: %lu MB\r\n", free_space / (1024 * 1024));
        
        return 1;
    } else {
        printf("[SDCard] Mount failed! Error: %d\r\n", res);
        sdcard_ready = 0;
        
        // 打印详细错误信息
        switch(res) {
            case FR_DISK_ERR:     printf("  - Disk error\r\n"); break;
            case FR_NOT_READY:    printf("  - Drive not ready\r\n"); break;
            case FR_NO_FILESYSTEM: printf("  - No filesystem\r\n"); break;
            default:             printf("  - Unknown error\r\n"); break;
        }
        
        return 0;
    }
}

// 保存JPEG图像到SD卡
SDCARD_Status sdcard_save_jpeg(JPEG_Image* jpeg_img, const char* filename)
{
    FRESULT res;
    UINT bytes_written;
    
    if(!sdcard_ready) {
        printf("[SDCard] SD card not ready!\r\n");
        return SDCARD_NO_CARD;
    }
    
    if(jpeg_img == NULL || jpeg_img->data == NULL || jpeg_img->length == 0) {
        printf("[SDCard] Invalid JPEG data!\r\n");
        return SDCARD_ERROR;
    }
    
    printf("[SDCard] Saving: %s (%d bytes)\r\n", filename, jpeg_img->length);
    
    // 打开文件（创建新文件）
    res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    
    if(res != FR_OK) {
        printf("[SDCard] Failed to open file! Error: %d\r\n", res);
        
        switch(res) {
            case FR_DISK_ERR:       return SDCARD_WRITE_ERROR;
            case FR_NOT_READY:      return SDCARD_NO_CARD;
            case FR_NO_FILESYSTEM:   return SDCARD_NO_FILESYSTEM;
            case FR_DENIED:         return SDCARD_FULL;
            default:                return SDCARD_ERROR;
        }
    }
    
    // 写入JPEG数据
    res = f_write(&file, jpeg_img->data, jpeg_img->length, &bytes_written);
    
    if(res != FR_OK) {
        printf("[SDCard] Failed to write file! Error: %d\r\n", res);
        f_close(&file);
        return SDCARD_WRITE_ERROR;
    }
    
    if(bytes_written != jpeg_img->length) {
        printf("[SDCard] Warning: Written %d bytes, expected %d bytes\r\n", 
               bytes_written, jpeg_img->length);
    }
    
    // 关闭文件
    f_close(&file);
    
    printf("[SDCard] Saved: %s (%d bytes)\r\n", filename, bytes_written);
    
    return SDCARD_OK;
}

// 保存JPEG图像到SD卡（自动命名）
SDCARD_Status sdcard_save_jpeg_auto(JPEG_Image* jpeg_img, Filename_Mode mode)
{
    char filename[32];
    
    if(jpeg_img == NULL || jpeg_img->data == NULL || jpeg_img->length == 0) {
        printf("[SDCard] Invalid JPEG data!\r\n");
        return SDCARD_ERROR;
    }
    
    // 生成文件名
    sdcard_generate_filename(filename, file_count, mode);
    file_count++;
    
    // 保存JPEG图像
    return sdcard_save_jpeg(jpeg_img, filename);
}

// 生成文件名
void sdcard_generate_filename(char* filename, u32 count, Filename_Mode mode)
{
    if(filename == NULL) {
        return;
    }
    
    switch(mode) {
        case FILENAME_SEQ:
            // 顺序编号：image_001.jpg
            sprintf(filename, "0:image_%03lu.jpg", count);
            break;
            
        case FILENAME_TIMESTAMP:
            // 时间戳：20250219_120000.jpg
            // 注意：需要RTC支持
            sprintf(filename, "0:img_%06lu_%03lu.jpg", 
                   (u32)0, (u32)0);  // 占位符，需要RTC支持
            break;
            
        case FILENAME_DETECTION:
            // 检测类型：safe_001.jpg, fire_001.jpg
            sprintf(filename, "0:img_%03lu.jpg", count);
            break;
            
        default:
            sprintf(filename, "0:image_%03lu.jpg", count);
            break;
    }
}

// 获取已保存的文件数量
u32 sdcard_get_file_count(void)
{
    return file_count;
}

// 删除最旧的文件
SDCARD_Status sdcard_delete_oldest_file(void)
{
    DIR dir;
    FILINFO fno;
    char oldest_file[32] = {0};
    u32 oldest_time = 0xFFFFFFFF;
    FRESULT res;
    
    if(!sdcard_ready) {
        printf("[SDCard] SD card not ready!\r\n");
        return SDCARD_NO_CARD;
    }
    
    // 打开目录
    res = f_opendir(&dir, "0:");
    
    if(res != FR_OK) {
        printf("[SDCard] Failed to open directory! Error: %d\r\n", res);
        return SDCARD_ERROR;
    }
    
    // 查找最旧的文件
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if(fno.fattrib & AM_ARC) {  // 是文件
            if(fno.ftime < oldest_time) {
                oldest_time = fno.ftime;
                strcpy(oldest_file, fno.fname);
            }
        }
    }
    
    // 关闭目录
    f_closedir(&dir);
    
    // 删除最旧的文件
    if(oldest_file[0] != 0) {
        res = f_unlink(oldest_file);
        
        if(res == FR_OK) {
            printf("[SDCard] Deleted: %s\r\n", oldest_file);
            return SDCARD_OK;
        } else {
            printf("[SDCard] Failed to delete file! Error: %d\r\n", res);
            return SDCARD_ERROR;
        }
    }
    
    printf("[SDCard] No files to delete!\r\n");
    return SDCARD_ERROR;
}

// 格式化SD卡
SDCARD_Status sdcard_format(void)
{
    FRESULT res;
    u8 work[512];  // 工作缓冲区
    
    if(!sdcard_ready) {
        printf("[SDCard] SD card not ready!\r\n");
        return SDCARD_NO_CARD;
    }
    
    printf("[SDCard] Formatting...\r\n");
    
    // 格式化SD卡
    res = f_mkfs("0:", 0, 512);
    
    if(res == FR_OK) {
        printf("[SDCard] Format successful!\r\n");
        file_count = 0;  // 重置文件计数
        return SDCARD_OK;
    } else {
        printf("[SDCard] Format failed! Error: %d\r\n", res);
        return SDCARD_ERROR;
    }
}

// 获取SD卡信息
SDCARD_Status sdcard_get_info(u32* total_space, u32* free_space)
{
    FATFS* fs_ptr;
    DWORD fre_clust;
    FRESULT res;
    
    if(total_space == NULL || free_space == NULL) {
        return SDCARD_ERROR;
    }
    
    if(!sdcard_ready) {
        printf("[SDCard] SD card not ready!\r\n");
        return SDCARD_NO_CARD;
    }
    
    // 获取空闲簇数和文件系统信息
    res = f_getfree("0:", &fre_clust, &fs_ptr);
    
    if(res == FR_OK) {
        // 计算总空间和空闲空间
        DWORD total_sect = (fs_ptr->n_fatent - 2) * fs_ptr->csize;
        DWORD free_sect = fre_clust * fs_ptr->csize;
        
        *total_space = total_sect * 512;  // 总空间（字节）
        *free_space = free_sect * 512;  // 空闲空间（字节）
        return SDCARD_OK;
    } else {
        printf("[SDCard] Failed to get free space! Error: %d\r\n", res);
        return SDCARD_ERROR;
    }
}

// 检查SD卡状态
u8 sdcard_is_ready(void)
{
    return sdcard_ready;
}

// 切换为SD卡模式（GPIOC8/9/11切换为SDIO接口）
void sw_sdcard_mode(void)
{
    OV5640_WR_Reg(0X3017,0X00);	// 关闭OV5640所有引脚(避免影响SD卡通信)
    OV5640_WR_Reg(0X3018,0X00); 
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource8,GPIO_AF_SDIO);  //PC8,AF12
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource9,GPIO_AF_SDIO);	//PC9,AF12 
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_SDIO); 
}

// 切换为摄像头模式（GPIOC8/9/11切换为DCMI接口）
void sw_ov5640_mode(void)
{
    OV5640_WR_Reg(0X3017,0XFF);	// 打开OV5650所有引脚(避免影响摄像头显示)
    OV5640_WR_Reg(0X3018,0XFF); 
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource8,GPIO_AF_DCMI);  //PC8,AF13  DCMI_D2
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource9,GPIO_AF_DCMI);  //PC9,AF13  DCMI_D3
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_DCMI); //PC11,AF13 DCMI_D4  
}
