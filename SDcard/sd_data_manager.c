/**
 * @file    sd_data_manager.c
 * @brief   SD卡数据管理器实现文件
 * @author  STM32 Fire Monitoring System
 * @version 1.0
 * @date    2026-03-08
 * 
 * @note    实现功能：
 *          - SD卡文件系统结构管理
 *          - 实时数据记录（CSV格式）
 *          - 日报/周报/月报生成（JSON格式）
 *          - 异常告警记录
 *          - 自动清理过期数据
 *          - 错误重试机制
 */

#include "sd_data_manager.h"
#include "ff.h"
#include "diskio.h"

// 全局变量定义
SDCardStatus g_sd_status = {0};
DataManagerStats g_dm_stats = {0};

// 静态变量
static FATFS g_fatfs;
static u8 g_initialized = 0;

/**
 * @brief  初始化数据管理器
 * @retval 0-成功，非0-失败
 * @note   初始化SD卡，创建目录结构，清理过期数据
 */
u8 SD_DataManager_Init(void)
{
    printf("\r\n[SD_DataManager] Initializing...\r\n");
    printf("[SD_DataManager] Version: %s, Date: %s\r\n", 
           SD_DATA_MANAGER_VERSION, SD_DATA_MANAGER_DATE);
    
    // 1. 初始化SD卡硬件
    printf("[SD_DataManager] Step 1: Initializing SD card hardware...\r\n");
    if(SDCard_Init() != SDCARD_OK)
    {
        printf("[SD_DataManager] ERROR: SD card hardware initialization failed!\r\n");
        g_sd_status.initialized = 0;
        return 1;
    }
    printf("[SD_DataManager] SD card hardware initialized successfully\r\n");
    
    // 2. 挂载文件系统
    printf("[SD_DataManager] Step 2: Mounting file system...\r\n");
    if(SDCard_Mount() != SDCARD_OK)
    {
        printf("[SD_DataManager] ERROR: File system mount failed!\r\n");
        g_sd_status.mounted = 0;
        return 2;
    }
    printf("[SD_DataManager] File system mounted successfully\r\n");
    
    g_sd_status.initialized = 1;
    g_sd_status.mounted = 1;
    
    // 3. 创建目录结构
    printf("[SD_DataManager] Step 3: Creating directory structure...\r\n");
    SD_DataManager_CreateDirectoryStructure();
    
    // 4. 获取SD卡空间信息
    printf("[SD_DataManager] Step 4: Getting SD card info...\r\n");
    g_sd_status.total_space = SD_DataManager_GetTotalSpace();
    g_sd_status.free_space = SD_DataManager_GetFreeSpace();
    g_sd_status.used_space = g_sd_status.total_space - g_sd_status.free_space;
    
    printf("[SD_DataManager] Total space: %lu MB\r\n", g_sd_status.total_space / (1024 * 1024));
    printf("[SD_DataManager] Free space: %lu MB\r\n", g_sd_status.free_space / (1024 * 1024));
    printf("[SD_DataManager] Used space: %lu MB\r\n", g_sd_status.used_space / (1024 * 1024));
    
    // 5. 检查空间状态
    printf("[SD_DataManager] Step 5: Checking space status...\r\n");
    u8 space_status = SD_DataManager_CheckSpace();
    if(space_status == 2)  // 空间严重不足
    {
        printf("[SD_DataManager] WARNING: SD card space critical! Performing emergency cleanup...\r\n");
        SD_DataManager_EmergencyCleanup();
    }
    else if(space_status == 1)  // 空间不足
    {
        printf("[SD_DataManager] WARNING: SD card space low!\r\n");
    }
    
    // 6. 清理过期数据
    printf("[SD_DataManager] Step 6: Cleaning up old data...\r\n");
    SD_DataManager_CleanupOldData();
    
    g_initialized = 1;
    printf("[SD_DataManager] Initialization complete!\r\n\n");
    
    return 0;
}

/**
 * @brief  创建目录结构
 * @note   创建所有必要的目录
 */
void SD_DataManager_CreateDirectoryStructure(void)
{
    // 创建数据目录
    SDCard_CreateDir(DIR_DATA);
    SDCard_CreateDir(DIR_REALTIME);
    SDCard_CreateDir(DIR_DAILY);
    SDCard_CreateDir(DIR_WEEKLY);
    SDCard_CreateDir(DIR_MONTHLY);
    
    // 创建报告目录
    SDCard_CreateDir(DIR_REPORTS);
    SDCard_CreateDir(DIR_REP_DAILY);
    SDCard_CreateDir(DIR_REP_WEEKLY);
    SDCard_CreateDir(DIR_REP_MONTHLY);
    
    // 创建其他目录
    SDCard_CreateDir(DIR_ALERTS);
    SDCard_CreateDir(DIR_CONFIG);
    SDCard_CreateDir(DIR_BACKUP);
}

/**
 * @brief  检查并初始化SD卡
 * @retval 0-成功，非0-失败
 * @note   如果SD卡未初始化，则自动初始化
 */
u8 SD_DataManager_CheckAndInit(void)
{
    if(!g_initialized || !g_sd_status.mounted)
    {
        printf("[SD_DataManager] SD card not ready, reinitializing...\r\n");
        return SD_DataManager_Init();
    }
    return 0;
}

/**
 * @brief  生成文件名
 * @param  type: 数据类型
 * @param  filename: 文件名缓冲区
 * @param  size: 缓冲区大小
 */
void SD_DataManager_FormatFilename(DataType type, char* filename, u8 size)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    
    // 获取当前时间
    RTC_GetTime(RTC_Format_BIN, &time);
    RTC_GetDate(RTC_Format_BIN, &date);
    
    switch(type)
    {
        case DATA_TYPE_REALTIME:
            // realtime_YYYYMMDD.csv
            snprintf(filename, size, "%s/realtime_%04d%02d%02d.csv",
                    DIR_REALTIME, date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date);
            break;
            
        case DATA_TYPE_DAILY:
            // daily_YYYYMMDD.json
            snprintf(filename, size, "%s/daily_%04d%02d%02d.json",
                    DIR_REP_DAILY, date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date);
            break;
            
        case DATA_TYPE_WEEKLY:
            // weekly_YYYYMMDD.json
            snprintf(filename, size, "%s/weekly_%04d%02d%02d.json",
                    DIR_REP_WEEKLY, date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date);
            break;
            
        case DATA_TYPE_MONTHLY:
            // monthly_YYYYMM.json
            snprintf(filename, size, "%s/monthly_%04d%02d.json",
                    DIR_REP_MONTHLY, date.RTC_Year + 2000, date.RTC_Month);
            break;
            
        case DATA_TYPE_ALERT:
            // alert_YYYYMMDD_HHMMSS.json
            snprintf(filename, size, "%s/alert_%04d%02d%02d_%02d%02d%02d.json",
                    DIR_ALERTS, date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date,
                    time.RTC_Hours, time.RTC_Minutes, time.RTC_Seconds);
            break;
            
        default:
            filename[0] = '\0';
            break;
    }
}

/**
 * @brief  获取当前日期字符串
 * @param  date_str: 日期字符串缓冲区
 * @param  size: 缓冲区大小
 */
void SD_DataManager_GetCurrentDateString(char* date_str, u8 size)
{
    RTC_DateTypeDef date;
    RTC_GetDate(RTC_Format_BIN, &date);
    
    snprintf(date_str, size, "%04d-%02d-%02d",
            date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date);
}

/**
 * @brief  获取当前时间字符串
 * @param  time_str: 时间字符串缓冲区
 * @param  size: 缓冲区大小
 */
void SD_DataManager_GetCurrentTimeString(char* time_str, u8 size)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    RTC_GetTime(RTC_Format_BIN, &time);
    RTC_GetDate(RTC_Format_BIN, &date);
    
    snprintf(time_str, size, "%04d-%02d-%02d %02d:%02d:%02d",
            date.RTC_Year + 2000, date.RTC_Month, date.RTC_Date,
            time.RTC_Hours, time.RTC_Minutes, time.RTC_Seconds);
}

/**
 * @brief  保存实时数据（CSV格式）
 * @param  record: 传感器数据记录
 * @retval 0-成功，非0-失败
 * @note   格式：timestamp,flame_ao,flame_do,smoke_ao,smoke_ppm,fire_detected,smoke_detected,risk_level,confidence
 */
u8 SD_DataManager_SaveRealtimeData(SensorRecord* record)
{
    char filename[64];
    char csv_data[256];
    u8 result;
    
    // 检查初始化状态
    if(!g_initialized)
    {
        if(SD_DataManager_CheckAndInit() != 0)
        {
            return 1;
        }
    }
    
    // 生成文件名
    SD_DataManager_FormatFilename(DATA_TYPE_REALTIME, filename, sizeof(filename));
    
    // 准备CSV数据
    // 如果文件不存在，添加表头
    u8 file_exists = SD_DataManager_FileExists(filename);
    
    if(!file_exists)
    {
        // 新文件，添加CSV表头
        snprintf(csv_data, sizeof(csv_data),
                "timestamp,flame_adc,flame_do,smoke_adc,smoke_ppm,fire_detected,smoke_detected,temp_detected,risk_level,confidence,frame_count\r\n");
        result = SD_DataManager_WriteFile(filename, csv_data);
        if(result != 0)
        {
            printf("[SD_DataManager] ERROR: Failed to write CSV header\r\n");
            g_dm_stats.write_errors++;
            return result;
        }
    }
    
    // 准备数据行
    snprintf(csv_data, sizeof(csv_data),
            "%s,%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d\r\n",
            record->timestamp,
            record->flame_adc,
            record->flame_do,
            record->smoke_adc,
            record->smoke_ppm,
            record->fire_detected,
            record->smoke_detected,
            record->temp_detected,
            record->risk_level,
            record->confidence,
            record->frame_count);
    
    // 写入文件
    result = SD_DataManager_WriteFile(filename, csv_data);
    
    if(result == 0)
    {
        g_dm_stats.total_records++;
        g_dm_stats.daily_records++;
        
        if(record->risk_level > 0)
        {
            g_dm_stats.total_alerts++;
            g_dm_stats.daily_alerts++;
        }
    }
    else
    {
        g_dm_stats.write_errors++;
    }
    
    return result;
}

/**
 * @brief  保存日报（JSON格式）
 * @param  report: 日报数据结构
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_SaveDailyReport(DailyReport* report)
{
    char filename[64];
    char json_data[512];
    
    // 检查初始化状态
    if(!g_initialized)
    {
        if(SD_DataManager_CheckAndInit() != 0)
        {
            return 1;
        }
    }
    
    // 生成文件名
    SD_DataManager_FormatFilename(DATA_TYPE_DAILY, filename, sizeof(filename));
    
    // 准备JSON数据
    snprintf(json_data, sizeof(json_data),
            "{\r\n"
            "  \"type\": \"daily_report\",\r\n"
            "  \"date\": \"%s\",\r\n"
            "  \"total_records\": %d,\r\n"
            "  \"fire_alerts\": %d,\r\n"
            "  \"smoke_alerts\": %d,\r\n"
            "  \"temp_alerts\": %d,\r\n"
            "  \"avg_smoke_ppm\": %.2f,\r\n"
            "  \"avg_temperature\": %.2f,\r\n"
            "  \"max_smoke_adc\": %d,\r\n"
            "  \"max_flame_adc\": %d,\r\n"
            "  \"max_temperature\": %d,\r\n"
            "  \"max_risk_level\": %d\r\n"
            "}\r\n",
            report->date,
            report->total_records,
            report->fire_alerts,
            report->smoke_alerts,
            report->temp_alerts,
            report->avg_smoke_ppm,
            report->avg_temperature,
            report->max_smoke_adc,
            report->max_flame_adc,
            report->max_temperature,
            report->max_risk_level);
    
    // 写入文件
    return SD_DataManager_WriteFile(filename, json_data);
}

/**
 * @brief  保存周报（JSON格式）
 * @param  report: 周报数据结构
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_SaveWeeklyReport(WeeklyReport* report)
{
    char filename[64];
    char json_data[512];
    
    // 检查初始化状态
    if(!g_initialized)
    {
        if(SD_DataManager_CheckAndInit() != 0)
        {
            return 1;
        }
    }
    
    // 生成文件名
    SD_DataManager_FormatFilename(DATA_TYPE_WEEKLY, filename, sizeof(filename));
    
    // 准备JSON数据
    snprintf(json_data, sizeof(json_data),
            "{\r\n"
            "  \"type\": \"weekly_report\",\r\n"
            "  \"week_start\": \"%s\",\r\n"
            "  \"week_end\": \"%s\",\r\n"
            "  \"total_records\": %d,\r\n"
            "  \"fire_alerts\": %d,\r\n"
            "  \"smoke_alerts\": %d,\r\n"
            "  \"temp_alerts\": %d,\r\n"
            "  \"avg_smoke_ppm\": %.2f,\r\n"
            "  \"max_risk_level\": %d\r\n"
            "}\r\n",
            report->week_start,
            report->week_end,
            report->total_records,
            report->fire_alerts,
            report->smoke_alerts,
            report->temp_alerts,
            report->avg_smoke_ppm,
            report->max_risk_level);
    
    // 写入文件
    return SD_DataManager_WriteFile(filename, json_data);
}

/**
 * @brief  保存月报（JSON格式）
 * @param  report: 月报数据结构
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_SaveMonthlyReport(MonthlyReport* report)
{
    char filename[64];
    char json_data[512];
    
    // 检查初始化状态
    if(!g_initialized)
    {
        if(SD_DataManager_CheckAndInit() != 0)
        {
            return 1;
        }
    }
    
    // 生成文件名
    SD_DataManager_FormatFilename(DATA_TYPE_MONTHLY, filename, sizeof(filename));
    
    // 准备JSON数据
    snprintf(json_data, sizeof(json_data),
            "{\r\n"
            "  \"type\": \"monthly_report\",\r\n"
            "  \"month\": \"%s\",\r\n"
            "  \"total_records\": %d,\r\n"
            "  \"fire_alerts\": %d,\r\n"
            "  \"smoke_alerts\": %d,\r\n"
            "  \"temp_alerts\": %d,\r\n"
            "  \"avg_smoke_ppm\": %.2f,\r\n"
            "  \"max_risk_level\": %d\r\n"
            "}\r\n",
            report->month,
            report->total_records,
            report->fire_alerts,
            report->smoke_alerts,
            report->temp_alerts,
            report->avg_smoke_ppm,
            report->max_risk_level);
    
    // 写入文件
    return SD_DataManager_WriteFile(filename, json_data);
}

/**
 * @brief  保存异常记录（JSON格式）
 * @param  record: 传感器数据记录
 * @param  reason: 异常原因
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_SaveAlert(SensorRecord* record, char* reason)
{
    char filename[64];
    char json_data[512];
    
    // 检查初始化状态
    if(!g_initialized)
    {
        if(SD_DataManager_CheckAndInit() != 0)
        {
            return 1;
        }
    }
    
    // 生成文件名
    SD_DataManager_FormatFilename(DATA_TYPE_ALERT, filename, sizeof(filename));
    
    // 准备JSON数据
    snprintf(json_data, sizeof(json_data),
            "{\r\n"
            "  \"type\": \"alert\",\r\n"
            "  \"timestamp\": \"%s\",\r\n"
            "  \"reason\": \"%s\",\r\n"
            "  \"flame_adc\": %d,\r\n"
            "  \"flame_do\": %d,\r\n"
            "  \"smoke_adc\": %d,\r\n"
            "  \"smoke_ppm\": %.2f,\r\n"
            "  \"fire_detected\": %d,\r\n"
            "  \"smoke_detected\": %d,\r\n"
            "  \"temp_detected\": %d,\r\n"
            "  \"risk_level\": %d,\r\n"
            "  \"confidence\": %d,\r\n"
            "  \"frame_count\": %d\r\n"
            "}\r\n",
            record->timestamp,
            reason,
            record->flame_adc,
            record->flame_do,
            record->smoke_adc,
            record->smoke_ppm,
            record->fire_detected,
            record->smoke_detected,
            record->temp_detected,
            record->risk_level,
            record->confidence,
            record->frame_count);
    
    // 写入文件
    return SD_DataManager_WriteFile(filename, json_data);
}

/**
 * @brief  清理所有过期数据
 * @note   根据保留期限清理各类数据
 */
void SD_DataManager_CleanupOldData(void)
{
    printf("[SD_DataManager] Starting cleanup of old data...\r\n");
    
    SD_DataManager_CleanupRealtimeData();
    SD_DataManager_CleanupDailyData();
    SD_DataManager_CleanupWeeklyData();
    SD_DataManager_CleanupMonthlyData();
    
    printf("[SD_DataManager] Cleanup complete!\r\n");
}

/**
 * @brief  清理实时数据（保留1天）
 */
void SD_DataManager_CleanupRealtimeData(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char filepath[128];
    u32 deleted_count = 0;
    
    res = f_opendir(&dir, DIR_REALTIME);
    if(res != FR_OK)
    {
        printf("[SD_DataManager] Failed to open realtime directory\r\n");
        return;
    }
    
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        // 解析文件名中的日期：realtime_YYYYMMDD.csv
        int year, month, day;
        if(sscanf(fno.fname, "realtime_%4d%2d%2d.csv", &year, &month, &day) == 3)
        {
            // 计算文件日期与当前日期的差值
            RTC_DateTypeDef current_date;
            RTC_GetDate(RTC_Format_BIN, &current_date);
            
            u32 file_days = year * 365 + month * 30 + day;
            u32 current_days = (current_date.RTC_Year + 2000) * 365 + 
                              current_date.RTC_Month * 30 + current_date.RTC_Date;
            
            // 如果超过保留期限，删除文件
            if((current_days - file_days) > RETENTION_REALTIME)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", DIR_REALTIME, fno.fname);
                if(f_unlink(filepath) == FR_OK)
                {
                    printf("[SD_DataManager] Deleted old realtime data: %s\r\n", fno.fname);
                    deleted_count++;
                }
            }
        }
    }
    
    f_closedir(&dir);
    
    if(deleted_count > 0)
    {
        printf("[SD_DataManager] Deleted %lu old realtime files\r\n", deleted_count);
    }
}

/**
 * @brief  清理日报数据（保留1周）
 */
void SD_DataManager_CleanupDailyData(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char filepath[128];
    u32 deleted_count = 0;
    
    res = f_opendir(&dir, DIR_DAILY);
    if(res != FR_OK) return;
    
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        int year, month, day;
        if(sscanf(fno.fname, "daily_%4d%2d%2d.json", &year, &month, &day) == 3)
        {
            RTC_DateTypeDef current_date;
            RTC_GetDate(RTC_Format_BIN, &current_date);
            
            u32 file_days = year * 365 + month * 30 + day;
            u32 current_days = (current_date.RTC_Year + 2000) * 365 + 
                              current_date.RTC_Month * 30 + current_date.RTC_Date;
            
            if((current_days - file_days) > RETENTION_DAILY)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", DIR_DAILY, fno.fname);
                if(f_unlink(filepath) == FR_OK)
                {
                    printf("[SD_DataManager] Deleted old daily data: %s\r\n", fno.fname);
                    deleted_count++;
                }
            }
        }
    }
    
    f_closedir(&dir);
    
    if(deleted_count > 0)
    {
        printf("[SD_DataManager] Deleted %lu old daily files\r\n", deleted_count);
    }
}

/**
 * @brief  清理周报数据（保留1月）
 */
void SD_DataManager_CleanupWeeklyData(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char filepath[128];
    u32 deleted_count = 0;
    
    res = f_opendir(&dir, DIR_WEEKLY);
    if(res != FR_OK) return;
    
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        int year, month, day;
        if(sscanf(fno.fname, "weekly_%4d%2d%2d.json", &year, &month, &day) == 3)
        {
            RTC_DateTypeDef current_date;
            RTC_GetDate(RTC_Format_BIN, &current_date);
            
            u32 file_days = year * 365 + month * 30 + day;
            u32 current_days = (current_date.RTC_Year + 2000) * 365 + 
                              current_date.RTC_Month * 30 + current_date.RTC_Date;
            
            if((current_days - file_days) > RETENTION_WEEKLY)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", DIR_WEEKLY, fno.fname);
                if(f_unlink(filepath) == FR_OK)
                {
                    printf("[SD_DataManager] Deleted old weekly data: %s\r\n", fno.fname);
                    deleted_count++;
                }
            }
        }
    }
    
    f_closedir(&dir);
    
    if(deleted_count > 0)
    {
        printf("[SD_DataManager] Deleted %lu old weekly files\r\n", deleted_count);
    }
}

/**
 * @brief  清理月报数据（保留1年）
 */
void SD_DataManager_CleanupMonthlyData(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char filepath[128];
    u32 deleted_count = 0;
    
    res = f_opendir(&dir, DIR_MONTHLY);
    if(res != FR_OK) return;
    
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        int year, month;
        if(sscanf(fno.fname, "monthly_%4d%2d.json", &year, &month) == 2)
        {
            RTC_DateTypeDef current_date;
            RTC_GetDate(RTC_Format_BIN, &current_date);
            
            u32 file_months = year * 12 + month;
            u32 current_months = (current_date.RTC_Year + 2000) * 12 + current_date.RTC_Month;
            
            if((current_months - file_months) > 12)  // 保留12个月
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", DIR_MONTHLY, fno.fname);
                if(f_unlink(filepath) == FR_OK)
                {
                    printf("[SD_DataManager] Deleted old monthly data: %s\r\n", fno.fname);
                    deleted_count++;
                }
            }
        }
    }
    
    f_closedir(&dir);
    
    if(deleted_count > 0)
    {
        printf("[SD_DataManager] Deleted %lu old monthly files\r\n", deleted_count);
    }
}

/**
 * @brief  删除最旧的文件
 * @param  dir_path: 目录路径
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_DeleteOldestFile(const char* dir_path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char oldest_file[256] = {0};
    u32 oldest_time = 0xFFFFFFFF;
    char filepath[512];
    
    res = f_opendir(&dir, dir_path);
    if(res != FR_OK)
    {
        return 1;
    }
    
    // 查找最旧的文件
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if(fno.fattrib & AM_ARC)  // 是文件
        {
            if(fno.ftime < oldest_time)
            {
                oldest_time = fno.ftime;
                strcpy(oldest_file, fno.fname);
            }
        }
    }
    
    f_closedir(&dir);
    
    // 删除最旧的文件
    if(oldest_file[0] != 0)
    {
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, oldest_file);
        if(f_unlink(filepath) == FR_OK)
        {
            printf("[SD_DataManager] Deleted oldest file: %s\r\n", oldest_file);
            return 0;
        }
    }
    
    return 1;
}

/**
 * @brief  获取剩余空间
 * @retval 剩余空间（字节）
 */
u32 SD_DataManager_GetFreeSpace(void)
{
    FATFS* fs;
    DWORD fre_clust;
    FRESULT res;
    u32 free_space = 0;
    
    res = f_getfree("0:", &fre_clust, &fs);
    if(res == FR_OK)
    {
        free_space = fre_clust * fs->csize * 512;
    }
    
    return free_space;
}

/**
 * @brief  获取总空间
 * @retval 总空间（字节）
 */
u32 SD_DataManager_GetTotalSpace(void)
{
    FATFS* fs;
    DWORD fre_clust;
    FRESULT res;
    u32 total_space = 0;
    
    res = f_getfree("0:", &fre_clust, &fs);
    if(res == FR_OK)
    {
        total_space = (fs->n_fatent - 2) * fs->csize * 512;
    }
    
    return total_space;
}

/**
 * @brief  检查空间状态
 * @retval 0-正常，1-空间不足，2-空间严重不足
 */
u8 SD_DataManager_CheckSpace(void)
{
    u32 free_space_mb = SD_DataManager_GetFreeSpace() / (1024 * 1024);
    
    if(free_space_mb < SD_SPACE_CRITICAL_MB)
    {
        return 2;  // 空间严重不足
    }
    else if(free_space_mb < SD_SPACE_WARNING_MB)
    {
        return 1;  // 空间不足
    }
    
    return 0;  // 正常
}

/**
 * @brief  紧急清理
 * @retval 0-成功，非0-失败
 * @note   当空间严重不足时，删除最旧的数据文件
 */
u8 SD_DataManager_EmergencyCleanup(void)
{
    printf("[SD_DataManager] Emergency cleanup started...\r\n");
    
    u8 result = 0;
    
    // 按优先级删除：实时数据 > 日报 > 周报 > 月报
    // 注意：异常记录和报告不删除
    
    result |= SD_DataManager_DeleteOldestFile(DIR_REALTIME);
    if(SD_DataManager_CheckSpace() == 0) return 0;
    
    result |= SD_DataManager_DeleteOldestFile(DIR_DAILY);
    if(SD_DataManager_CheckSpace() == 0) return 0;
    
    result |= SD_DataManager_DeleteOldestFile(DIR_WEEKLY);
    if(SD_DataManager_CheckSpace() == 0) return 0;
    
    result |= SD_DataManager_DeleteOldestFile(DIR_MONTHLY);
    if(SD_DataManager_CheckSpace() == 0) return 0;
    
    printf("[SD_DataManager] Emergency cleanup complete!\r\n");
    
    return result;
}

/**
 * @brief  获取数据管理器状态
 * @retval 0-正常，非0-异常
 */
u8 SD_DataManager_GetStatus(void)
{
    if(!g_initialized)
    {
        return 1;
    }
    
    if(!g_sd_status.mounted)
    {
        return 2;
    }
    
    return 0;
}

/**
 * @brief  获取统计信息
 * @param  stats: 统计信息结构体指针
 */
void SD_DataManager_GetStats(DataManagerStats* stats)
{
    if(stats != NULL)
    {
        memcpy(stats, &g_dm_stats, sizeof(DataManagerStats));
    }
}

/**
 * @brief  打印状态信息
 */
void SD_DataManager_PrintStatus(void)
{
    printf("\r\n========== SD Data Manager Status ==========\r\n");
    printf("Initialized: %s\r\n", g_initialized ? "Yes" : "No");
    printf("SD Card Mounted: %s\r\n", g_sd_status.mounted ? "Yes" : "No");
    printf("Total Space: %lu MB\r\n", g_sd_status.total_space / (1024 * 1024));
    printf("Free Space: %lu MB\r\n", g_sd_status.free_space / (1024 * 1024));
    printf("Used Space: %lu MB\r\n", g_sd_status.used_space / (1024 * 1024));
    printf("\r\nStatistics:\r\n");
    printf("  Total Records: %lu\r\n", g_dm_stats.total_records);
    printf("  Total Alerts: %lu\r\n", g_dm_stats.total_alerts);
    printf("  Daily Records: %lu\r\n", g_dm_stats.daily_records);
    printf("  Daily Alerts: %lu\r\n", g_dm_stats.daily_alerts);
    printf("  Write Errors: %lu\r\n", g_dm_stats.write_errors);
    printf("  Write Retries: %lu\r\n", g_dm_stats.write_retries);
    printf("============================================\r\n\n");
}

/**
 * @brief  写入文件（带重试机制）
 * @param  filename: 文件名
 * @param  data: 数据内容
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_WriteFile(const char* filename, const char* data)
{
    FIL file;
    FRESULT res;
    UINT bytes_written;
    u8 retry_count = 0;
    
    while(retry_count < WRITE_RETRY_COUNT)
    {
        // 打开文件（追加模式）
        res = f_open(&file, filename, FA_OPEN_ALWAYS | FA_WRITE);
        if(res != FR_OK)
        {
            retry_count++;
            g_dm_stats.write_retries++;
            delay_ms(WRITE_RETRY_DELAY_MS);
            continue;
        }
        
        // 移动到文件末尾
        res = f_lseek(&file, f_size(&file));
        if(res != FR_OK)
        {
            f_close(&file);
            retry_count++;
            g_dm_stats.write_retries++;
            delay_ms(WRITE_RETRY_DELAY_MS);
            continue;
        }
        
        // 写入数据
        res = f_write(&file, data, strlen(data), &bytes_written);
        if(res != FR_OK || bytes_written != strlen(data))
        {
            f_close(&file);
            retry_count++;
            g_dm_stats.write_retries++;
            delay_ms(WRITE_RETRY_DELAY_MS);
            continue;
        }
        
        // 关闭文件
        res = f_close(&file);
        if(res != FR_OK)
        {
            retry_count++;
            g_dm_stats.write_retries++;
            delay_ms(WRITE_RETRY_DELAY_MS);
            continue;
        }
        
        // 写入成功
        return 0;
    }
    
    // 写入失败
    printf("[SD_DataManager] ERROR: Failed to write file after %d retries: %s\r\n", 
           WRITE_RETRY_COUNT, filename);
    return 1;
}

/**
 * @brief  读取文件
 * @param  filename: 文件名
 * @param  buffer: 数据缓冲区
 * @param  size: 缓冲区大小
 * @retval 0-成功，非0-失败
 */
u8 SD_DataManager_ReadFile(const char* filename, char* buffer, u32 size)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    
    // 打开文件
    res = f_open(&file, filename, FA_READ);
    if(res != FR_OK)
    {
        return 1;
    }
    
    // 读取数据
    res = f_read(&file, buffer, size - 1, &bytes_read);
    if(res != FR_OK)
    {
        f_close(&file);
        return 2;
    }
    
    // 关闭文件
    f_close(&file);
    
    // 添加字符串结束符
    buffer[bytes_read] = '\0';
    
    return 0;
}

/**
 * @brief  检查文件是否存在
 * @param  filename: 文件名
 * @retval 0-不存在，1-存在
 */
u8 SD_DataManager_FileExists(const char* filename)
{
    FILINFO fno;
    FRESULT res;
    
    res = f_stat(filename, &fno);
    
    return (res == FR_OK) ? 1 : 0;
}

/**
 * @brief  获取文件大小
 * @param  filename: 文件名
 * @retval 文件大小（字节），0表示文件不存在或获取失败
 */
u32 SD_DataManager_GetFileSize(const char* filename)
{
    FILINFO fno;
    FRESULT res;
    
    res = f_stat(filename, &fno);
    if(res == FR_OK)
    {
        return fno.fsize;
    }
    
    return 0;
}
