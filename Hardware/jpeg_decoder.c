// jpeg_decoder.c - JPEG解码到外部SRAM（带行缓存优化）
// 实现方案：JPEG缓冲区在内部SRAM，RGB缓冲区在外部SRAM，使用行缓存优化访问速度

#include "sys.h"
#include "delay.h"
#include "tjpgd.h"
#include "extsram.h"


// 外部SRAM RGB缓冲区配置
#define RGB_BUF_SIZE       (320 * 240 * 2)       // RGB565格式：150KB
#define RGB_BUF_ADDR       0x68000000            // 外部SRAM起始地址（FSMC映射）

// 行缓存配置（内部SRAM）
#define ROW_BUFFER_HEIGHT  8                      // 缓存8行
#define ROW_BUFFER_WIDTH   320                    // 每行320像素
#define ROW_BUFFER_SIZE    (ROW_BUFFER_WIDTH * ROW_BUFFER_HEIGHT * 2)  // 8×320×2=5120字节
#define JPEG_MAX_SIZE       (32*1024)



extern uint8_t jpeg_buf_a;

// 外部SRAM RGB缓冲区
#define rgb_buffer  ((u16*)RGB_BUF_ADDR)  // RGB565格式（外部SRAM）

// 行缓存（内部SRAM）
__align(4) u16 row_buffer[ROW_BUFFER_HEIGHT][ROW_BUFFER_WIDTH];

// TJpgDec解码工作区
__align(4) uint8_t work[ 16384];  // 解码工作区

// 解码状态
typedef struct {
    uint16_t width;          // 图像宽度
    uint16_t height;         // 图像高度
    uint16_t scale;          // 缩放因子
    uint16_t* dst_buf;       // 目标缓冲区
    uint32_t dst_pos;        // 目标缓冲区位置
    uint8_t  decode_status;  // 解码状态
} JpegDecodeState;


// 定义流数据结构
typedef struct {
    const uint8_t* data;  // JPEG数据指针
    uint32_t size;        // 数据总大小
    uint32_t offset;      // 当前读取位置
} jpeg_stream_t;



// 全局解码状态
static JpegDecodeState decode_state;

// TJpgDec回调函数
static uint32_t input_func(JDEC* jd, uint8_t* buff, uint32_t nbyte)
{
    jpeg_stream_t* stream = (jpeg_stream_t*)jd->device;
    uint32_t remain = stream->size - stream->offset;
    
    if (buff) {
        uint32_t rd = (nbyte < remain) ? nbyte : remain;
        if (rd > 0) {
            memcpy(buff, stream->data + stream->offset, rd);
        }
        stream->offset += rd;
        return rd;
    } else {
        uint32_t sk = (nbyte < remain) ? nbyte : remain;
        stream->offset += sk;
        return sk;
    }
}

// TJpgDec输出回调函数
static uint32_t output_func(JDEC* jd, void* bitmap, JRECT* rect)
{
    uint16_t* src = (uint16_t*)bitmap;
    uint16_t* dst;
    uint16_t y, x;
    
    // 计算目标位置
    for (y = rect->top; y <= rect->bottom; y++) {
        dst = decode_state.dst_buf + y * decode_state.width + rect->left;
        for (x = rect->left; x <= rect->right; x++) {
            *dst++ = *src++;
        }
    }
    
    return 1;  // 成功
}

/**
 * @brief  初始化JPEG解码器
 * @param  无
 * @retval 无
 */
void jpeg_decoder_init(void)
{
    // 确保外部SRAM已初始化
    Extsram_Init();
    
    // 初始化解码状态
    decode_state.width = 0;
    decode_state.height = 0;
    decode_state.scale = 0;
    decode_state.dst_buf = (uint16_t*)RGB_BUF_ADDR;
    decode_state.dst_pos = 0;
    decode_state.decode_status = 0;
    
    // printf("[JPEG Decoder] Initialized with:\r\n");
    // printf("  RGB buffer: External SRAM @ 0x%08X (%dKB)\r\n", 
    //        (uint32_t)RGB_BUF_ADDR, RGB_BUF_SIZE / 1024);
    // printf("  Row buffer: Internal SRAM (%dx%d rows, %dKB)\r\n", 
    //        ROW_BUFFER_WIDTH, ROW_BUFFER_HEIGHT, ROW_BUFFER_SIZE / 1024);
}

/**
 * @brief  解码JPEG数据到外部SRAM
 * @param  jpeg_data: JPEG数据指针
 * @param  jpeg_size: JPEG数据大小
 * @retval 解码结果：0-成功，非0-失败
 */
u8 jpeg_decoder_decode(const uint8_t* jpeg_data, uint32_t jpeg_size)
{
    JDEC jdec;
    uint8_t result;
    uint32_t start_time, end_time;
    jpeg_stream_t stream;

	// 🚨 终极探针：检查是否发生了恐怖的“DMA内存踩踏”！
    printf("\r\n--- MEMORY CHECK ---\r\n");
    printf("JPEG Data Addr (DMA) : 0x%08X\r\n", (uint32_t)jpeg_data);
    printf("TJpgDec Work Addr    : 0x%08X\r\n", (uint32_t)work);
    
    // 计算地址差值
    int diff = (int)((uint32_t)work - (uint32_t)jpeg_data);
    if(diff < 0) diff = -diff;
    printf("Distance             : %d bytes\r\n", diff);
    
    // 如果工作区和DMA缓冲区的距离小于32KB，说明互相重叠了！
    if(diff < 32768) {
        printf("🚨 DANGER: Memory Overlap Detected! DMA is crushing the workspace!\r\n");
    }
    printf("--------------------\r\n");
		
     // 初始化stream
    stream.data = jpeg_data;      // 直接使用传入的jpeg_data
    stream.size = jpeg_size;
    stream.offset = 0;
    
    // 准备解码 - 传入stream的地址
    // printf("jd_prepare...\n");
    result = jd_prepare(&jdec, input_func, work, sizeof(work), (void*)&stream);
    // printf("jd_prepare result: %d\n", result);
		
	if (result != JDR_OK) {
        printf("[JPEG Decoder] Prepare failed: %d\r\n", result);
        return result;
    }
    
    // 更新解码状态 - 使用1/2缩放直接输出160x120
    decode_state.width = jdec.width / 2;
    decode_state.height = jdec.height / 2;
    decode_state.scale = 1;  // 1/2缩放
    
    // printf("[JPEG Decoder] Original size: %dx%d, Scaled size: %dx%d\r\n", 
    //        jdec.width, jdec.height, decode_state.width, decode_state.height);
    
    // 开始解码到外部SRAM
    // printf("jd_decomp start...\n");
    result = jd_decomp(&jdec, output_func, decode_state.scale);
    // printf("jd_decomp result: %d\n", result);
    if (result != JDR_OK) {
        printf("[JPEG Decoder] Decompress failed: %d\r\n", result);
        return result;
    }
    
    end_time = millis();
    // printf("[JPEG Decoder] Decode completed in %dms\r\n", end_time - start_time);
    decode_state.decode_status = 1;
    
    return JDR_OK;
}

/**
 * @brief  从外部SRAM加载一行到行缓存
 * @param  row: 行号
 * @param  buffer_index: 缓存索引（0-ROW_BUFFER_HEIGHT-1）
 * @retval 加载结果：0-成功，非0-失败
 */
u8 jpeg_decoder_load_row(uint16_t row, uint8_t buffer_index)
{
    if (row >= decode_state.height || buffer_index >= ROW_BUFFER_HEIGHT) {
        return 1;
    }
    
    // 从外部SRAM批量读取一行数据到行缓存
    uint16_t* src = (uint16_t*)RGB_BUF_ADDR + row * decode_state.width;
    uint16_t* dst = row_buffer[buffer_index];
    
    // 批量复制（利用FSMC连续访问速度快的特点）
    for (uint16_t x = 0; x < decode_state.width; x++) {
        *dst++ = *src++;
    }
    
    return 0;
}

/**
 * @brief  从行缓存加载多行
 * @param  start_row: 起始行号
 * @param  row_count: 行数
 * @retval 加载结果：0-成功，非0-失败
 */
u8 jpeg_decoder_load_rows(uint16_t start_row, uint8_t row_count)
{
    if (start_row + row_count > decode_state.height || row_count > ROW_BUFFER_HEIGHT) {
        return 1;
    }
    
    // 批量加载多行
    for (uint8_t i = 0; i < row_count; i++) {
        if (jpeg_decoder_load_row(start_row + i, i) != 0) {
            return 1;
        }
    }
    
    return 0;
}


/**
 * @brief  从行缓存获取像素
 * @param  buffer_index: 缓存索引
 * @param  x: X坐标
 * @param  y_in_buffer: 缓存内的Y坐标
 * @retval RGB565像素值
 */
u16 jpeg_decoder_get_pixel(uint8_t buffer_index, uint16_t x, uint16_t y_in_buffer)
{
    if (buffer_index >= ROW_BUFFER_HEIGHT || 
        x >= decode_state.width || 
        y_in_buffer >= ROW_BUFFER_HEIGHT) {
        return 0;
    }
    
    return row_buffer[buffer_index][x];
}

/**
 * @brief  获取RGB缓冲区地址
 * @param  无
 * @retval RGB缓冲区地址
 */
u16* jpeg_decoder_get_rgb_buffer(void)
{
    return (u16*)RGB_BUF_ADDR;
}

/**
 * @brief  获取解码状态
 * @param  无
 * @retval 解码状态
 */
JpegDecodeState* jpeg_decoder_get_state(void)
{
    return &decode_state;
}

/**
 * @brief  示例：使用行缓存处理图像
 * @param  无
 * @retval 处理结果：0-成功，非0-失败
 */
u8 jpeg_decoder_process_with_row_buffer(void)
{
    if (!decode_state.decode_status) {
        printf("[JPEG Decoder] No decoded image\r\n");
        return 1;
    }
    
    uint16_t row, x;
    uint8_t buffer_index;
    uint32_t process_time = 0;
    
    // printf("[JPEG Decoder] Processing image with row buffer...\r\n");
    
    // 逐块处理图像
    for (row = 0; row < decode_state.height; row += ROW_BUFFER_HEIGHT) {
        uint8_t rows_to_process = ROW_BUFFER_HEIGHT;
        if (row + rows_to_process > decode_state.height) {
            rows_to_process = decode_state.height - row;
        }
        
        // 加载块到行缓存
        uint32_t load_start = millis();
        if (jpeg_decoder_load_rows(row, rows_to_process) != 0) {
            return 1;
        }
        process_time += millis() - load_start;
        
        // 处理行缓存中的数据
        for (buffer_index = 0; buffer_index < rows_to_process; buffer_index++) {
            // 这里可以添加实际的图像处理算法
            // 例如：边缘检测、颜色分析、目标识别等
            
            // 示例：简单的像素统计
            uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
            for (x = 0; x < decode_state.width; x++) {
                u16 pixel = row_buffer[buffer_index][x];
                u8 r = (pixel >> 11) & 0x1F;
                u8 g = (pixel >> 5) & 0x3F;
                u8 b = pixel & 0x1F;
                sum_r += r;
                sum_g += g;
                sum_b += b;
            }
            
            // 每10行打印一次处理信息
            // if ((row + buffer_index) % 10 == 0) {
            //     printf("[Row %d] Avg R: %d, G: %d, B: %d\r\n", 
            //            row + buffer_index, 
            //            sum_r / decode_state.width, 
            //            sum_g / decode_state.width, 
            //            sum_b / decode_state.width);
            // }
        }
    }
    
    // printf("[JPEG Decoder] Processing completed in %dms\r\n", process_time);
    return 0;
}



/**
 * @brief  处理JPEG数据（解码+行缓存处理）
 * @param  jpeg_data: JPEG数据指针
 * @param  jpeg_size: JPEG数据大小
 * @retval 处理结果：0-成功，非0-失败
 */
u8 jpeg_decoder_process(const uint8_t* jpeg_data, uint32_t jpeg_size)
{
    // 解码JPEG数据
    if(jpeg_decoder_decode(jpeg_data, jpeg_size) != 0) {
        return 1;
    }
    
    // 使用行缓存处理图像
    jpeg_decoder_process_with_row_buffer();
    
    return 0;
}

/**
 * @brief  清除解码状态
 * @param  无
 * @retval 无
 */
void jpeg_decoder_clear(void)
{
    memset(&decode_state, 0, sizeof(JpegDecodeState));
    memset(rgb_buffer, 0, sizeof(rgb_buffer));
    memset(row_buffer, 0, sizeof(row_buffer));
}
