// jpeg_serial.c - JPEG处理和串口发送集成
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "dcmi.h"
#include "jpeg_decoder.h"
#include "jpeg_serial.h"
#include "image_preprocess.h"
#include "fire_detection.h"
#include "esp8266_report.h"
#include "led.h"
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

extern SemaphoreHandle_t Mutex_USART1;
void Safe_Printf(char *format, ...);

// 内部SRAM JPEG缓冲区配置（双缓冲）
#define JPEG_MAX_SIZE       (24*1024)       // 每帧最大32KB（320*240分辨率）

// 检测缓冲区地址配置（外部SRAM）
#define DETECT_RGB_BUF_ADDR 0x6804B000      // 检测用RGB缓冲区（HSV缓冲区之后）
#define DETECT_HSV_BUF_ADDR 0x68050800      // 检测用HSV缓冲区

// 双缓冲区（乒乓缓冲）
__align(4) u8 jpeg_buf_a[JPEG_MAX_SIZE];  // 占用 24KB
__align(4) u8 jpeg_buf_b[JPEG_MAX_SIZE];  // 占用 24KB

// 双缓冲区控制
volatile BufferControl buf_ctrl_a = {jpeg_buf_a, BUF_IDLE, 0, 0, 0};
volatile BufferControl buf_ctrl_b = {jpeg_buf_b, BUF_IDLE, 0, 0, 0};
volatile BufferControl* current_capture_buf = &buf_ctrl_a;  // 当前采集缓冲区
volatile BufferControl* current_send_buf = NULL;             // 当前发送缓冲区

// 帧信息结构
typedef struct {
    u32 length;          // 长度
    u8  valid;          // 是否有效
} FrameInfo;

// 环形缓冲区管理（简化版）
volatile FrameInfo frame_info;
volatile u8 frame_valid = 0;        // 帧是否有效

// JPEG数据处理相关变量
volatile u8 jpeg_data_ok = 0;
volatile u32 jpeg_data_len = 0;
volatile u8 ovx_mode = 0x01;  // JPEG模式

// Snapshot JPEG采集相关变量
volatile u8 frame_done = 0;
volatile u32 frame_length = 0;

// 丢弃的帧数
volatile u32 dropped_frames = 0;

// 帧计数
volatile u32 frame_count = 0;
volatile u32 g_processed_frame_count = 0;  // 实际处理成功的帧数
volatile u32 fire_detection_frame_count = 0;  // 火焰检测帧计数器

// 火焰检测频率控制（每N帧进行一次检测）
#define FIRE_DETECTION_INTERVAL 20  // 每20帧进行一次火焰检测，减少CPU占用

// 外部变量声明（在dcmi.c中定义）
extern u8 ov_frame;  // 帧计数

// 中断标志和状态变量（用于主循环处理）
volatile u8 dma_transfer_complete = 0;     // DMA传输完成标志
volatile u32 dma_transfer_count = 0;       // DMA传输计数
volatile BufferControl* completed_buffer = NULL;  // 完成采集的缓冲区

// DMA传输完成中断处理函数 - 已禁用，改用DCMI帧中断
// 注意：JPEG模式下数据长度不固定，DMA TCIF可能永远不会触发
void DMA2_Stream1_IRQHandler(void)
{
    // 清除所有DMA中断标志（防止中断挂起）
    DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1 | DMA_IT_HTIF1 | DMA_IT_TEIF1 | DMA_IT_DMEIF1 | DMA_IT_FEIF1);
    
    // 不再处理DMA传输完成中断，由DCMI帧中断处理
}

// 缓冲区切换和DMA重启函数 - 在主循环中调用
void switch_buffer_and_restart_dma(void)
{
    // 检查是否有完成的缓冲区
    if(completed_buffer == NULL) {
        return;
    }
    
    // 切换到另一个缓冲区
    if(current_capture_buf == &buf_ctrl_a) {
        current_capture_buf = &buf_ctrl_b;
    } else {
        current_capture_buf = &buf_ctrl_a;
    }
    
    // 检查新缓冲区是否空闲
    if(current_capture_buf->state != BUF_IDLE) {
        dropped_frames++;
        // 强制重置为空闲状态
        current_capture_buf->state = BUF_IDLE;
    }
    
    // 标记新缓冲区为采集中
    current_capture_buf->state = BUF_CAPTURING;
    
    // 配置DMA到新的缓冲区
    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);
    
    DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;
    
    // 清除DMA标志
    DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
    
    // 重新启动DMA
    DMA_Cmd(DMA2_Stream1, ENABLE);
    
    // 重新启动DCMI捕获
    DCMI_CaptureCmd(ENABLE);
    
    // 清除完成标志
    completed_buffer = NULL;
    dma_transfer_complete = 0;
}

// 串口显示初始化函数
void serial_display_init(void)
{
    // 串口已经在usart.c中初始化，这里可以添加额外的配置
    printf("[Serial Display] Initialized\r\n");
    printf("[Serial Display] Ready to send JPEG data\r\n");
}

// 下采样函数：将320×240的RGB565图像下采样到160×120
// 原因：降低检测分辨率以提高处理速度，像素数减少4倍
void downsample_rgb565(u16* src, u16 src_w, u16 src_h,
                       u16* dst, u16 dst_w, u16 dst_h)
{
    u16 x, y, dx, dy;
    u32 r, g, b;
    u16 pixel;
    u16 src_x, src_y;
    
    // 简单的2×2平均下采样
    for(y = 0; y < dst_h; y++)
    {
        for(x = 0; x < dst_w; x++)
        {
            src_x = x * 2;
            src_y = y * 2;
            
            // 取2×2区域的平均值
            r = 0; g = 0; b = 0;
            for(dy = 0; dy < 2; dy++)
            {
                for(dx = 0; dx < 2; dx++)
                {
                    pixel = src[(src_y + dy) * src_w + (src_x + dx)];
                    r += (pixel >> 11) & 0x1F;
                    g += (pixel >> 5) & 0x3F;
                    b += pixel & 0x1F;
                }
            }
            
            r /= 4;
            g /= 4;
            b /= 4;
            
            dst[y * dst_w + x] = (r << 11) | (g << 5) | b;
        }
    }
}

// 串口发送JPEG数据函数（使用DMA发送JPEG数据，头部和校验和使用轮询）
void serial_send_jpeg_data(u8* jpeg_data, u32 jpeg_len)
{
    u32 i;
    u8 checksum = 0;
    
    // 计算校验和
    for(i = 0; i < jpeg_len; i++)
    {
        checksum += jpeg_data[i];
    }
    
    // 发送JPEG数据头（轮询方式）
    uart_send_byte(0x00);
    uart_send_byte(0xFF);
    uart_send_byte(0x01);
    
    // 发送JPEG数据长度（4字节，大端模式，轮询方式）
    uart_send_byte((jpeg_len >> 24) & 0xFF);
    uart_send_byte((jpeg_len >> 16) & 0xFF);
    uart_send_byte((jpeg_len >> 8) & 0xFF);
    uart_send_byte(jpeg_len & 0xFF);
    
    // 发送JPEG数据（使用DMA方式）
    usart1_dma_send(jpeg_data, (uint16_t)jpeg_len);
    
    // 发送校验和（轮询方式）
    uart_send_byte(checksum);
    
    //printf("[Serial Display] JPEG data sent: %d bytes (DMA)\r\n", jpeg_len);
}



//JPEG数据处理函数（参考探索者项目实现）
void jpeg_data_process(void)
{
    if(!(ovx_mode & 0x01)) return;   // 仅JPEG模式处理

    u32 actual_bytes;

    if(jpeg_data_ok == 0)  // 数据未处理，开始处理
    {
        /* 1️⃣ 停止DCMI捕获 */
        DCMI_CaptureCmd(DISABLE);
        while(DCMI->CR & DCMI_CR_CAPTURE);   // 等待完全停止

        /* 2️⃣ 关闭DMA */
        DMA_Cmd(DMA2_Stream1, DISABLE);
        while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);

        /* 3️⃣ 计算本帧实际长度 */
        u32 words_transferred = (JPEG_MAX_SIZE / 4) - DMA_GetCurrDataCounter(DMA2_Stream1);
        actual_bytes = words_transferred * 4;

        jpeg_data_len = actual_bytes;

        /* 4️⃣ 保存帧信息 */
        if(actual_bytes > 0 && actual_bytes <= JPEG_MAX_SIZE)
        {
            frame_info.length = actual_bytes;
            frame_info.valid = 1;
            frame_valid = 1;
            jpeg_data_ok = 1;  // 数据处理完成，等待应用处理
        }
    }
    if(jpeg_data_ok == 2)  // 准备下一次捕获
    {
        /* 5️⃣ 重新配置DMA长度 */
        DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;

        /* 6️⃣ 清除DMA标志 */
        DMA_ClearFlag(DMA2_Stream1,
                      DMA_FLAG_TCIF1 |
                      DMA_FLAG_HTIF1 |
                      DMA_FLAG_TEIF1 |
                      DMA_FLAG_DMEIF1 |
                      DMA_FLAG_FEIF1);

        /* 7️⃣ 重新启动DMA */
        DMA_Cmd(DMA2_Stream1, ENABLE);

        /* 8️⃣ 重新启动DCMI捕获 */
        DCMI_CaptureCmd(ENABLE);
        jpeg_data_ok = 0;  // 重置状态
    }
}



// Snapshot JPEG采集 - 启动一帧采集（双缓冲版本）
void DCMI_StartOneFrame(void)
{
    frame_done = 0;

    // 设置当前采集缓冲区状态
    current_capture_buf->state = BUF_CAPTURING;

    /* 1️⃣ 配置DMA地址 */
    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);

    DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;

    DMA_ClearFlag(DMA2_Stream1,
                  DMA_FLAG_TCIF1 |
                  DMA_FLAG_HTIF1 |
                  DMA_FLAG_TEIF1 |
                  DMA_FLAG_DMEIF1 |
                  DMA_FLAG_FEIF1);

    /* 2️⃣ 启动DMA */
    DMA_Cmd(DMA2_Stream1, ENABLE);

    /* 3️⃣ 启动DCMI捕获 */
    DCMI_CaptureCmd(ENABLE);
}

// Snapshot JPEG采集 - 获取帧长度（双缓冲版本）
u32 DCMI_GetFrameLength(void)
{
    u32 words;

    /* 停止DMA */
    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);

    words = (JPEG_MAX_SIZE / 4) - DMA_GetCurrDataCounter(DMA2_Stream1);
    
    // 保存数据长度到当前采集缓冲区
    current_capture_buf->data_len = words * 4;
    
    // 标记缓冲区为准备好
    current_capture_buf->state = BUF_READY;
    
    return current_capture_buf->data_len;
}


/**
 * @brief  处理JPEG帧数据（双缓冲版本）
 * @param  do_fire_detection: 是否进行火焰检测（1=检测，0=只发送图像）
 * @retval 处理结果：0-成功，非0-失败
 * @note   方案二+三组合：每N帧进行一次火焰检测，使用低分辨率检测
 */
u8 process_jpeg_frame(u8 do_fire_detection)
{
    // 增加帧计数
    frame_count++;
    
    // 关键修改：不再依赖completed_buffer
    // 直接判断当前哪个缓冲区是READY状态
    volatile BufferControl* ready_buf = NULL;
    
    if(buf_ctrl_a.state == BUF_READY) {
        ready_buf = &buf_ctrl_a;
    } else if(buf_ctrl_b.state == BUF_READY) {
        ready_buf = &buf_ctrl_b;
    } else {
        return 1;  // 没有准备好的缓冲区
    }
    
    // 标记为发送中
    ready_buf->state = BUF_SENDING;
    
    u8* p = ready_buf->buffer;
    u32 jpeg_start = 0;
    u32 jpeg_end = 0;
    u32 jpeg_size = 0;
    
    // 获取帧长度
    frame_length = ready_buf->data_len;
    
    // 直接使用原始JPEG数据（内部SRAM不需要重排）
    if(frame_length > 0 && frame_length <= JPEG_MAX_SIZE) {
        // 恢复搜索逻辑，查看FF D8到底偏移了多少
        for(u32 i = 0; i < 512 && i < frame_length - 1; i++) {
            if(p[i] == 0xFF && p[i+1] == 0xD8) {
                jpeg_start = i;
                break;
            }
        }
        
        // 检查是否找到JPEG头
        if(jpeg_start == 0 && (p[0] != 0xFF || p[1] != 0xD8)) {
            // 未找到JPEG头，跳过此帧
            ready_buf->state = BUF_IDLE;
            return 1;
        }
        
        // 搜索JPEG文件尾（0xFF 0xD9）
        for(u32 i = jpeg_start; i < frame_length - 1; i++) {
            if(p[i] == 0xFF && p[i+1] == 0xD9) {
                jpeg_end = i + 2;
                break;
            }
        }
        
        // 强制发送数据，不管是否找到JPEG尾
        // 如果找到JPEG尾，使用找到的；否则使用从jpeg_start开始到帧结束的所有数据
        if(jpeg_end > jpeg_start) {
            jpeg_size = jpeg_end - jpeg_start;
        } else {
            // 没找到JPEG尾，使用从jpeg_start开始到帧结束的所有数据
            jpeg_size = frame_length - jpeg_start;
        }
        
        // 确保数据大小合理（至少100字节）
        if(jpeg_size > 100) {
            
					// 🚨 【加上这三行】：拿锁 -> 发图片指令 -> 瞬间还锁
            // 这样能保证在启动 DMA 的瞬间，绝对没有人在 printf
            if(Mutex_USART1 != NULL) xSemaphoreTake(Mutex_USART1, portMAX_DELAY);
            
            serial_send_jpeg_data(&p[jpeg_start], jpeg_size);
            
            if(Mutex_USART1 != NULL) xSemaphoreGive(Mutex_USART1);
            // 增加处理成功帧计数
            g_processed_frame_count++;
            
            // 翻转LED0状态作为处理成功的指示
            static u8 led_state = 0;
            if(led_state == 0) {
                led_on(led0);
                led_state = 1;
            } else {
                led_off(led0);
                led_state = 0;
            }
            
            // 根据参数决定是否进行火焰检测
            if(do_fire_detection) {
                fire_detection_frame_count++;
                
                // 🚨 修复一：暂时注释掉内部限流，只要外部让测，我们就测！
                // if(fire_detection_frame_count % FIRE_DETECTION_INTERVAL == 0) {
                    
                    printf("\r\n[Vision] 1. Start decoding JPEG...\r\n");
                    
                    // 1. 解码JPEG
                    int decode_res = jpeg_decoder_process(&p[jpeg_start], jpeg_size);
                    
                    if(decode_res == 0) {
                        printf("[Vision] 2. Decode SUCCESS! Processing pixels...\r\n");
                        
                        JpegDecodeState* dec = jpeg_decoder_get_state();
                        
                        // 3. RGB转HSV
                        image_preprocess_frame_rgb_to_hsv(dec->dst_buf, 
                                                         (hsv_t*)DETECT_HSV_BUF_ADDR, 
                                                         dec->width, dec->height);
                        
                        // 4. 火焰检测
                        FireDetectionResult fire_result;
                        fire_detection_process_frame((hsv_t*)DETECT_HSV_BUF_ADDR, 
                                                    dec->width, dec->height, 
                                                    &fire_result);
                        
                        // 5. 更新火焰检测结果到报告模块
                        ESP8266_Report_UpdateFireDetectionResult(&fire_result);
                        
                        printf("[Vision] 3. Algorithm done! Fire: %d, Area: %d\r\n", 
                               fire_result.fire_detected, fire_result.fire_area);
                    } 
                    else {
                        // 🚨 修复二：如果解码失败，大声喊出来错在哪！
                        printf("[Vision] ERROR: JPEG Decode FAILED! Error Code: %d\r\n", decode_res);
                    }
                // } 
            }
                    }
                }
            
        
    
    
    // 处理完成后标记为空闲
    ready_buf->state = BUF_IDLE;
    
    return 0;
}

/**
 * @brief  初始化JPEG串口系统（双缓冲版本）
 * @param  无
 * @retval 初始化结果：0-成功，非0-失败
 */
u8 jpeg_serial_init(void)
{
    // 初始化双缓冲区
    memset(jpeg_buf_a, 0, JPEG_MAX_SIZE);
    memset(jpeg_buf_b, 0, JPEG_MAX_SIZE);
    
    // 初始化缓冲区控制
    buf_ctrl_a.state = BUF_IDLE;
    buf_ctrl_b.state = BUF_IDLE;
    current_capture_buf = &buf_ctrl_a;
    current_send_buf = NULL;
    
    // 初始化帧信息
    frame_info.length = 0;
    frame_info.valid = 0;
    frame_valid = 0;
    dropped_frames = 0;
    frame_count = 0;
    
    // 配置DMA（写入内部SRAM，使用32位Word传输）
    DCMI_DMA_Init((u32)jpeg_buf_a, JPEG_MAX_SIZE / 4, DMA_MemoryDataSize_Word, DMA_MemoryInc_Enable);
    
    // 配置DMA中断
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 启用DMA传输完成中断
    DMA_ITConfig(DMA2_Stream1, DMA_IT_TC, ENABLE);
    
    // 初始化解码器
    jpeg_decoder_init();
    
    // 初始化火焰检测
    FireDetectionConfig fire_config;
    fire_config.h_min = 0;
    fire_config.h_max = 35;
    fire_config.s_min = 80;
    fire_config.v_min = 100;
    fire_config.min_area = 12;  // 调整为12（原50/4），适应160×120低分辨率
    fire_config.flicker_threshold = 3;
    fire_detection_init(&fire_config);
    printf("[JPEG Serial] Fire Detection initialized!\r\n");
    
    // 初始化串口显示
    printf("[JPEG Serial] Initializing Serial Display...\r\n");
    serial_display_init();
    
    // 初始化USART1 DMA发送
    printf("[JPEG Serial] Initializing USART1 DMA TX...\r\n");
    usart1_dma_init();
    printf("[JPEG Serial] USART1 DMA TX initialized!\r\n");
    
    // 启动第一帧采集
    printf("[JPEG Serial] Starting first frame capture...\r\n");
    DCMI_StartOneFrame();
    printf("[JPEG Serial] First frame capture started!\r\n");
    
    printf("[JPEG Serial] System initialized successfully!\r\n");
    printf("[JPEG Serial] Dual buffer mode enabled\r\n");
    
    return 0;
}

/**
 * @brief  获取帧计数
 * @param  无
 * @retval 帧计数
 */
u32 jpeg_serial_get_frame_count(void)
{
    return frame_count;
}

/**
 * @brief  增加帧计数
 * @param  无
 * @retval 无
 */
void jpeg_serial_inc_frame_count(void)
{
    frame_count++;
}

/**
 * @brief  获取丢弃的帧数
 * @param  无
 * @retval 丢弃的帧数
 */
u32 jpeg_serial_get_dropped_frames(void)
{
    return dropped_frames;
}

/**
 * @brief  增加丢弃的帧数
 * @param  无
 * @retval 无
 */
void jpeg_serial_inc_dropped_frames(void)
{
    dropped_frames++;
}
