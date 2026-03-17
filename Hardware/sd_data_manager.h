/**
 * @file    sd_data_manager.h
 * @brief   SD卡数据管理器头文件
 * @author  STM32 Fire Monitoring System
 * @version 1.0
 * @date    2026-03-08
 * 
 * @note    功能说明：
 *          - 管理SD卡文件系统结构
 *          - 实时数据记录（CSV格式）
 *          - 日报/周报/月报生成（JSON格式）
 *          - 异常告警记录
 *          - 自动清理过期数据
 *          - 错误重试机制
 * 
 * @par     文件系统结构：
 *          SD卡根目录/
 *          ├─ data/              原始数据目录
 *          │   ├─ realtime/        实时数据（最近1天）
 *          │   ├─ daily/          日报数据（最近1周）
 *          │   ├─ weekly/         周报数据（最近1月）
 *          │   └─ monthly/        月报数据（最近1年）
 *          ├─ reports/           报告文件目录
 *          │   ├─ daily/          日报文件
 *          │   ├─ weekly/         周报文件
 *          │   └─ monthly/        月报文件
 *          ├─ alerts/            异常告警记录
 *          ├─ config/            配置文件
 *          └─ backup/            数据备份
 * 
 * @par     文件命名规则：
 *          - 实时数据：realtime_YYYYMMDD.csv
 *          - 日报：daily_YYYYMMDD.json
 *          - 周报：weekly_YYYYMMDD.json
 *          - 月报：monthly_YYYYMM.json
 *          - 异常：alert_YYYYMMDD_HHMMSS.json
 */

#ifndef __SD_DATA_MANAGER_H
#define __SD_DATA_MANAGER_H

#include "sys.h"
#include "SDCard.h"
#include "RTC.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 版本信息
#define SD_DATA_MANAGER_VERSION     "1.0"
#define SD_DATA_MANAGER_DATE        "2026-03-08"

// 目录路径定义
#define DIR_DATA                    "0:/data"
#define DIR_REALTIME                "0:/data/realtime"
#define DIR_DAILY                   "0:/data/daily"
#define DIR_WEEKLY                  "0:/data/weekly"
#define DIR_MONTHLY                 "0:/data/monthly"
#define DIR_REPORTS                 "0:/reports"
#define DIR_REP_DAILY               "0:/reports/daily"
#define DIR_REP_WEEKLY              "0:/reports/weekly"
#define DIR_REP_MONTHLY             "0:/reports/monthly"
#define DIR_ALERTS                  "0:/alerts"
#define DIR_CONFIG                  "0:/config"
#define DIR_BACKUP                  "0:/backup"

// 数据保留期限（天）
#define RETENTION_REALTIME          1       // 实时数据保留1天
#define RETENTION_DAILY             7       // 日报保留1周
#define RETENTION_WEEKLY            30      // 周报保留1月
#define RETENTION_MONTHLY           365     // 月报保留1年
#define RETENTION_ALERT             0       // 异常记录永久保留（0表示不清理）

// 重试机制配置
#define WRITE_RETRY_COUNT           3       // 写入失败重试次数
#define WRITE_RETRY_DELAY_MS        100     // 重试延迟（毫秒）

// SD卡空间阈值
#define SD_SPACE_WARNING_MB         100     // 空间不足警告阈值（MB）
#define SD_SPACE_CRITICAL_MB        50      // 空间严重不足阈值（MB）

// 数据类型枚举
typedef enum {
    DATA_TYPE_REALTIME = 0,     // 实时数据
    DATA_TYPE_DAILY,            // 日报数据
    DATA_TYPE_WEEKLY,           // 周报数据
    DATA_TYPE_MONTHLY,          // 月报数据
    DATA_TYPE_ALERT             // 异常数据
} DataType;

// 风险等级枚举
typedef enum {
    RISK_NONE = 0,              // 无风险
    RISK_LOW = 1,               // 低风险
    RISK_MEDIUM = 2,            // 中风险
    RISK_HIGH = 3               // 高风险
} RiskLevel;

// 传感器数据结构
typedef struct {
    char timestamp[20];         // 时间戳：2026-03-08 12:00:00
    u16 flame_adc;              // 火焰传感器ADC值
    u8 flame_do;                // 火焰传感器数字输出（0=检测到，1=未检测到）
    u16 smoke_adc;              // 烟雾传感器ADC值
    float smoke_ppm;            // 烟雾浓度（PPM）
    u8 fire_detected;           // 火焰检测结果（0/1）
    u8 smoke_detected;          // 烟雾检测结果（0/1）
    u8 temp_detected;           // 温度检测结果（0/1）
    u8 risk_level;              // 风险等级（0-3）
    u8 confidence;              // 置信度（0-100）
    u32 frame_count;            // 帧计数
} SensorRecord;

// 日报数据结构
typedef struct {
    char date[11];              // 日期：2026-03-08
    u32 total_records;          // 总记录数
    u32 fire_alerts;            // 火灾告警次数
    u32 smoke_alerts;           // 烟雾告警次数
    u32 temp_alerts;            // 温度告警次数
    float avg_smoke_ppm;        // 平均烟雾浓度
    float avg_temperature;      // 平均温度
    u16 max_smoke_adc;          // 最大烟雾ADC值
    u16 max_flame_adc;          // 最大火焰ADC值
    u16 max_temperature;        // 最高温度
    u8 max_risk_level;          // 最高风险等级
} DailyReport;

// 周报数据结构
typedef struct {
    char week_start[11];        // 周开始日期：2026-03-08
    char week_end[11];          // 周结束日期：2026-03-14
    u32 total_records;          // 总记录数
    u32 fire_alerts;            // 火灾告警次数
    u32 smoke_alerts;           // 烟雾告警次数
    u32 temp_alerts;            // 温度告警次数
    float avg_smoke_ppm;        // 平均烟雾浓度
    u8 max_risk_level;          // 最高风险等级
} WeeklyReport;

// 月报数据结构
typedef struct {
    char month[8];              // 月份：2026-03
    u32 total_records;          // 总记录数
    u32 fire_alerts;            // 火灾告警次数
    u32 smoke_alerts;           // 烟雾告警次数
    u32 temp_alerts;            // 温度告警次数
    float avg_smoke_ppm;        // 平均烟雾浓度
    u8 max_risk_level;          // 最高风险等级
} MonthlyReport;

// SD卡状态结构
typedef struct {
    u8 initialized;             // 是否已初始化
    u8 mounted;                 // 是否已挂载
    u32 total_space;            // 总空间（字节）
    u32 free_space;             // 剩余空间（字节）
    u32 used_space;             // 已用空间（字节）
    u8 status;                  // 状态码
} SDCardStatus;

// 统计信息结构
typedef struct {
    u32 total_records;          // 总记录数
    u32 total_alerts;           // 总告警数
    u32 daily_records;          // 今日记录数
    u32 daily_alerts;           // 今日告警数
    u32 write_errors;           // 写入错误次数
    u32 write_retries;          // 写入重试次数
} DataManagerStats;

// 全局变量声明
extern SDCardStatus g_sd_status;
extern DataManagerStats g_dm_stats;

// 函数声明

// 初始化和配置函数
u8 SD_DataManager_Init(void);                                           // 初始化数据管理器
void SD_DataManager_CreateDirectoryStructure(void);                     // 创建目录结构
u8 SD_DataManager_CheckAndInit(void);                                   // 检查并初始化SD卡

// 文件名生成函数
void SD_DataManager_FormatFilename(DataType type, char* filename, u8 size);   // 生成文件名
void SD_DataManager_GetCurrentDateString(char* date_str, u8 size);            // 获取当前日期字符串
void SD_DataManager_GetCurrentTimeString(char* time_str, u8 size);            // 获取当前时间字符串

// 数据保存函数
u8 SD_DataManager_SaveRealtimeData(SensorRecord* record);               // 保存实时数据（CSV）
u8 SD_DataManager_SaveDailyReport(DailyReport* report);                 // 保存日报（JSON）
u8 SD_DataManager_SaveWeeklyReport(WeeklyReport* report);               // 保存周报（JSON）
u8 SD_DataManager_SaveMonthlyReport(MonthlyReport* report);             // 保存月报（JSON）
u8 SD_DataManager_SaveAlert(SensorRecord* record, char* reason);        // 保存异常记录（JSON）

// 数据读取函数
u8 SD_DataManager_ReadRealtimeData(char* date, SensorRecord* records, u16* count);  // 读取实时数据
u8 SD_DataManager_ReadDailyReport(char* date, DailyReport* report);                 // 读取日报
u8 SD_DataManager_ReadAlert(char* filename, SensorRecord* record, char* reason);    // 读取异常记录

// 数据清理函数
void SD_DataManager_CleanupOldData(void);                               // 清理所有过期数据
void SD_DataManager_CleanupRealtimeData(void);                          // 清理实时数据
void SD_DataManager_CleanupDailyData(void);                             // 清理日报数据
void SD_DataManager_CleanupWeeklyData(void);                            // 清理周报数据
void SD_DataManager_CleanupMonthlyData(void);                           // 清理月报数据
u8 SD_DataManager_DeleteOldestFile(const char* dir_path);               // 删除最旧的文件

// 空间管理函数
u32 SD_DataManager_GetFreeSpace(void);                                  // 获取剩余空间
u32 SD_DataManager_GetTotalSpace(void);                                 // 获取总空间
u8 SD_DataManager_CheckSpace(void);                                     // 检查空间状态
u8 SD_DataManager_EmergencyCleanup(void);                               // 紧急清理

// 状态查询函数
u8 SD_DataManager_GetStatus(void);                                      // 获取数据管理器状态
void SD_DataManager_GetStats(DataManagerStats* stats);                  // 获取统计信息
void SD_DataManager_PrintStatus(void);                                  // 打印状态信息

// 辅助函数
u8 SD_DataManager_WriteFile(const char* filename, const char* data);    // 写入文件（带重试）
u8 SD_DataManager_ReadFile(const char* filename, char* buffer, u32 size); // 读取文件
u8 SD_DataManager_FileExists(const char* filename);                     // 检查文件是否存在
u32 SD_DataManager_GetFileSize(const char* filename);                   // 获取文件大小

#endif /* __SD_DATA_MANAGER_H */
