// ov5640.c - 参考正点原子成功案例编写
#include "ov5640.h"
#include "ov5640cfg.h"
#include "delay.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "sccb.h"

//OV5640写寄存器 - 16位地址
//返回值:0,成功;1,失败.
u8 OV5640_WR_Reg(u16 reg,u8 data)
{
    u8 res=0;
    SCCB_Start();                   
    if(SCCB_WR_Byte(OV5640_ADDR))res=1;  
    delay_us(100);
    if(SCCB_WR_Byte(reg>>8))res=1;   // 写寄存器地址高8位
    delay_us(100);
    if(SCCB_WR_Byte(reg))res=1;      // 写寄存器地址低8位
    delay_us(100);
    if(SCCB_WR_Byte(data))res=1;     
    SCCB_Stop();      
    return res;
}

//OV5640读寄存器 - 16位地址
//返回值:读到的寄存器值
u8 OV5640_RD_Reg(u16 reg)
{
    u8 val=0;
    SCCB_Start();               
    SCCB_WR_Byte(OV5640_ADDR);      
    delay_us(100);     
    SCCB_WR_Byte(reg>>8);           // 写寄存器地址高8位
    delay_us(100);
    SCCB_WR_Byte(reg);              // 写寄存器地址低8位
    delay_us(100);      
    SCCB_Stop();   
    delay_us(100);       
    SCCB_Start();
    SCCB_WR_Byte(OV5640_ADDR|0X01);  
    delay_us(100);
    val=SCCB_RD_Byte();         
    SCCB_No_Ack();
    SCCB_Stop();
    return val;
}

//读取OV5640 ID
u16 OV5640_Read_ID(void)
{
    u8 id_h, id_l;
    u16 id;
    
    id_h = OV5640_RD_Reg(0x300A);  // ID高字节
    id_l = OV5640_RD_Reg(0x300B);  // ID低字节
    
    id = ((u16)id_h << 8) | id_l;
    
    return id;
}



// �ֱ��ʱ�
static const ov5640_res_table_t resolution_table[OV5640_RES_MAX] = {
    {160,   120,  "QQVGA"},
    {320,   240,  "QVGA"},
    {640,   480,  "VGA"},
    {800,   600,  "SVGA"},
    {1024,  768,  "XGA"},
    {1280,  720,  "720P"},
    {1280,  800,  "WXGA"},
    {1280,  1024, "SXGA"},
    {1600,  1200, "UXGA"},
    {1920,  1080, "1080P"},
    {2592,  1944, "QSXGA"}
};



// 应用JPEG模式寄存器表
static void ov5640_apply_jpeg_regs(void)
{
    u16 i;
    
    printf("[OV5640] Applying JPEG register table...\r\n");
    
    // 写入JPEG模式寄存器表
    for(i = 0; i < sizeof(ov5640_jpeg_reg_tbl)/4; i++)
    {
        OV5640_WR_Reg(ov5640_jpeg_reg_tbl[i][0], ov5640_jpeg_reg_tbl[i][1]);
        delay_us(100);
    }
    
    delay_ms(10);
}

// 设置输出尺寸（正点原子的关键函数）
// offx, offy: 输出图像的偏移量
// width, height: 实际输出图像的宽度和高度
u8 OV5640_OutSize_Set(u16 offx, u16 offy, u16 width, u16 height)
{
    OV5640_WR_Reg(0X3212, 0X03);   // 启动组3
    // 设置输出图像的实际尺寸(输出尺寸)
    OV5640_WR_Reg(0x3808, width>>8);  // 设置实际输出宽度高字节
    OV5640_WR_Reg(0x3809, width&0xff); // 设置实际输出宽度低字节
    OV5640_WR_Reg(0x380a, height>>8); // 设置实际输出高度高字节
    OV5640_WR_Reg(0x380b, height&0xff); // 设置实际输出高度低字节
    // 设置输出图像的窗口尺寸(ISP取像范围)
    // 范围:xsize-2*offx,ysize-2*offy
    OV5640_WR_Reg(0x3810, offx>>8);   // 设置X offset高字节
    OV5640_WR_Reg(0x3811, offx&0xff); // 设置X offset低字节
    
    OV5640_WR_Reg(0x3812, offy>>8);   // 设置Y offset高字节
    OV5640_WR_Reg(0x3813, offy&0xff); // 设置Y offset低字节
    
    OV5640_WR_Reg(0X3212, 0X13);     // 结束组3
    OV5640_WR_Reg(0X3212, 0Xa3);     // 启动组3更新
    return 0;
}

// 补光灯控制（正点原子的关键函数）
void OV5640_Flash_Ctrl(u8 sw)
{
    OV5640_WR_Reg(0x3016, 0X02);
    OV5640_WR_Reg(0x301C, 0X02);
    if(sw) OV5640_WR_Reg(0X3019, 0X02);
    else OV5640_WR_Reg(0X3019, 0X00);
}



static ov5640_config_t current_config = {
    .mode = OV5640_MODE_RGB565,
    .resolution = OV5640_RES_QQVGA,
    .width = 160,
    .height = 120
};

// д�Ĵ�����
static void ov5640_write_reg_table(const u16 (*reg_table)[2], u32 count)
{
    u32 i;
    u8 error_count = 0;
    
    for(i = 0; i < count; i++)
    {
        if(OV5640_WR_Reg(reg_table[i][0], reg_table[i][1]) != 0)
        {
            error_count++;
            printf("[OV5640] Reg write error: 0x%04X=0x%02X\r\n", 
                   reg_table[i][0], reg_table[i][1]);
        }
        delay_us(100);  // ��΢�ӵ���ʱ��ȷ���ȶ�
    }
    
    if(error_count > 0)
    {
        printf("[OV5640] Warning: %d register write errors\r\n", error_count);
    }
}

// 应用RGB565模式寄存器表
static void ov5640_apply_rgb565_regs(void)
{
    u16 i;
    
    printf("[OV5640] Applying RGB565 register table...\r\n");
    
    // 写入RGB565模式寄存器表
    for(i = 0; i < sizeof(ov5640_rgb565_reg_tbl)/4; i++)
    {
        OV5640_WR_Reg(ov5640_rgb565_reg_tbl[i][0], ov5640_rgb565_reg_tbl[i][1]);
        delay_us(100);
    }
    
    delay_ms(10);
}

// 正点原子参考的初始化
u8 OV5640_Init(void)
{
    u16 i;
    u16 reg;
    
    printf("\r\n========================================\r\n");
    printf("OV5640 Camera Initialization\r\n");
    printf("========================================\r\n\r\n");
    
    // 硬件复位（使用sccb.c中的函数）
    printf("[1] Hardware reset...\r\n");
    OV5640_HW_Reset();
    
    // SCCB初始化
    printf("[2] SCCB initialization...\r\n");
    SCCB_Init();
    delay_ms(5); 
    
    // 读取ID验证
    printf("[3] Reading camera ID...\r\n");
    reg = OV5640_Read_ID();
    printf("    Camera ID: 0x%04X\r\n", reg);
    
    if(reg != OV5640_ID)
    {
        printf("[ERROR] Camera ID mismatch!\r\n");
        return 1;
    }
    
    // 软件复位
    printf("[4] Software reset...\r\n");
    OV5640_WR_Reg(0x3103, 0x11);
    OV5640_WR_Reg(0x3008, 0x82);
    delay_ms(50);
    
    // 写入初始化寄存器表
    printf("[5] Writing initialization registers...\r\n");
    for(i = 0; i < sizeof(ov5640_init_reg_tbl)/4; i++)
    {
        OV5640_WR_Reg(ov5640_init_reg_tbl[i][0], ov5640_init_reg_tbl[i][1]);
        delay_us(100);
    }
    
    // 打开补光灯（持续打开，不是测试闪烁）
    printf("[6] Enabling flash light...\r\n");
    OV5640_Flash_Ctrl(1);
    delay_ms(50);
    
    
    
    // 再设置为JPEG模式
    printf("[8] Setting JPEG mode (640x480)...\r\n");
    ov5640_apply_jpeg_regs();
    delay_ms(50);
    
    // 设置图像质量参数（成功案例的关键步骤）
    printf("[9] Setting image quality parameters...\r\n");
    OV5640_Light_Mode(0);        // 自动模式
    OV5640_Color_Saturation(0);   // 色彩饱和度0（默认）
    OV5640_Brightness(4);         // 亮度4（默认）
    OV5640_Contrast(3);           // 对比度3（默认）
    OV5640_Sharpness(15);         // 锐度15（适中）
    delay_ms(50);
    
    // 动态设置输出尺寸（正点原子的关键步骤）
    printf("[10] Setting output size (640x480)...\r\n");
    OV5640_OutSize_Set(4, 0, 320, 240);
    
    // 使能输出
    printf("[11] Enabling output...\r\n");
    OV5640_WR_Reg(0x3008, 0x02);
    delay_ms(50);
    
    // 验证关键寄存器
    printf("[12] Verifying settings...\r\n");
    u8 reg_val;
    reg_val = OV5640_RD_Reg(0x4740);
    printf("    0x4740 (polarity) = 0x%02X\r\n", reg_val);
    reg_val = OV5640_RD_Reg(0x4300);
    printf("    0x4300 (format) = 0x%02X\r\n", reg_val);
    reg_val = OV5640_RD_Reg(0x3008);
    printf("    0x3008 (control) = 0x%02X\r\n", reg_val);
    
    printf("\r\n[OV5640] Initialization successful!\r\n");
    printf("========================================\r\n\r\n");
    
    return 0;
}

// �򻯰��ʼ�������ڵ��ԣ�
u8 ov5640_simple_init(void)
{
    u16 reg;
    
    printf("\r\n=== OV5640 Simple Init ===\r\n");
    
    // 硬件复位
    OV5640_HW_Reset();
    
    SCCB_Init();
    delay_ms(5); 
    
    // 读取ID
    reg = OV5640_Read_ID();
    printf("Camera ID: 0x%04X\r\n", reg);
    
    if(reg != OV5640_ID) return 1;
    
    // 软件复位
    OV5640_WR_Reg(0x3103, 0x11);
    OV5640_WR_Reg(0x3008, 0x82);
    delay_ms(50);
    
    OV5640_WR_Reg(0x3017, 0xFF);
    OV5640_WR_Reg(0x3018, 0xFF);
    OV5640_WR_Reg(0x3034, 0x1A);
    OV5640_WR_Reg(0x3035, 0x21);
    OV5640_WR_Reg(0x3036, 0x69);
    OV5640_WR_Reg(0x3037, 0x13);
    OV5640_WR_Reg(0x3108, 0x01);
    OV5640_WR_Reg(0x3824, 0x02);
    OV5640_WR_Reg(0x4740, 0x20);
    OV5640_WR_Reg(0x4300, 0x61);
    OV5640_WR_Reg(0x501F, 0x00);
    OV5640_WR_Reg(0x3808, 0x00);
    OV5640_WR_Reg(0x3809, 0xA0);
    OV5640_WR_Reg(0x380a, 0x00);
    OV5640_WR_Reg(0x380b, 0x78);
    OV5640_WR_Reg(0x3008, 0x02);
    
    printf("Simple init done\r\n");
    return 0;
}

// ʹ�ܲ���ͼ��
void ov5640_enable_test_pattern(void)
{
    printf("\r\n=== Enabling Test Pattern ===\r\n");
    OV5640_WR_Reg(0x503D, 0x80);  // ����ͼ��ʹ��
    OV5640_WR_Reg(0x503E, 0x00);  // ����ģʽ
    printf("Test pattern enabled\r\n");
}

void ov5640_disable_test_pattern(void)
{
    OV5640_WR_Reg(0x503D, 0x00);
    printf("Test pattern disabled\r\n");
}

void ov5640_set_mode(ov5640_mode_t mode)
{
    current_config.mode = mode;
    
    switch(mode)
    {
        case OV5640_MODE_JPEG:
            ov5640_apply_jpeg_regs();
            break;
        case OV5640_MODE_RGB565:
            ov5640_apply_rgb565_regs();
            break;
        default:
            printf("[OV5640] Unsupported mode: %d\r\n", mode);
            break;
    }
}

void ov5640_set_resolution(ov5640_resolution_t res)
{
    if(res >= OV5640_RES_MAX)
    {
        printf("[OV5640] Invalid resolution: %d\r\n", res);
        return;
    }
    
    current_config.resolution = res;
    current_config.width = resolution_table[res].width;
    current_config.height = resolution_table[res].height;
    
    printf("[OV5640] Setting resolution to %s (%dx%d)...\r\n", 
           resolution_table[res].name, current_config.width, current_config.height);
    
    OV5640_WR_Reg(0x3808, (current_config.width >> 8) & 0xFF);
    OV5640_WR_Reg(0x3809, current_config.width & 0xFF);
    OV5640_WR_Reg(0x380a, (current_config.height >> 8) & 0xFF);
    OV5640_WR_Reg(0x380b, current_config.height & 0xFF);
}

void ov5640_set_custom_size(u16 width, u16 height)
{
    current_config.width = width;
    current_config.height = height;
    
    printf("[OV5640] Setting custom size to %dx%d...\r\n", width, height);
    
    OV5640_WR_Reg(0x3808, (width >> 8) & 0xFF);
    OV5640_WR_Reg(0x3809, width & 0xFF);
    OV5640_WR_Reg(0x380a, (height >> 8) & 0xFF);
    OV5640_WR_Reg(0x380b, height & 0xFF);
}

u8 ov5640_test_communication(void)
{
    u16 camera_id = OV5640_Read_ID();
    return (camera_id == OV5640_ID) ? 0 : 1;
}

u16 ov5640_get_id(void)
{
    return OV5640_Read_ID();
}

ov5640_config_t* ov5640_get_config(void)
{
    return &current_config;
}

// 饱和度调整表
const u8 OV5640_SATURATION_TBL[7][6] = {
    {0x10, 0x40, 0x04, 0x10, 0x08, 0x04}, // -3 (低饱和度)
    {0x16, 0x50, 0x05, 0x18, 0x0C, 0x06}, // -2
    {0x1A, 0x56, 0x05, 0x1E, 0x10, 0x07}, // -1
    {0x1C, 0x5A, 0x06, 0x20, 0x12, 0x08}, // 0 (默认)
    {0x20, 0x60, 0x07, 0x24, 0x14, 0x09}, // +1
    {0x24, 0x66, 0x08, 0x28, 0x18, 0x0A}, // +2
    {0x28, 0x70, 0x09, 0x2C, 0x1C, 0x0C}  // +3 (高饱和度)
};

// 图像质量调整函数
// 光线模式
// mode: 0-自动模式, 1-晴天, 2-阴天, 3-荧光灯, 4-白炽灯
void OV5640_Light_Mode(u8 mode)
{
    // 这里可以根据需要实现不同光线模式的设置
    // 暂时设置为自动模式
    OV5640_WR_Reg(0x3212, 0x03);   // 启动组3
    OV5640_WR_Reg(0x558f, mode);   // 设置光线模式
    OV5640_WR_Reg(0x3212, 0x13);   // 结束组3
    OV5640_WR_Reg(0x3212, 0xa3);   // 启动组3
}

// 色彩饱和度调整
// sat: 0~6, 对应-3~3
void OV5640_Color_Saturation(u8 sat)
{
    u8 i;
    if(sat > 6) sat = 6;
    
    OV5640_WR_Reg(0x3212, 0x03);   // 启动组3
    OV5640_WR_Reg(0x5381, 0x1e);   // 正确的CMX1值
    OV5640_WR_Reg(0x5382, 0x5b);   // 正确的CMX2值
    OV5640_WR_Reg(0x5383, 0x08);   // 正确的CMX3值
    for(i = 0; i < 6; i++)
        OV5640_WR_Reg(0x5384 + i, OV5640_SATURATION_TBL[sat][i]); // 设置饱和度
    OV5640_WR_Reg(0x538b, 0x98);   // 正确的sign[8:1]值
    OV5640_WR_Reg(0x538a, 0x01);   // 正确的sign[9]值
    OV5640_WR_Reg(0x3212, 0x13);   // 结束组3
    OV5640_WR_Reg(0x3212, 0xa3);   // 启动组3
}

// 亮度调整
// bright: 0~8, 对应-4~4
void OV5640_Brightness(u8 bright)
{
    u8 brtval;
    if(bright < 4) brtval = 4 - bright;
    else brtval = bright - 4;
    
    OV5640_WR_Reg(0x3212, 0x03);   // 启动组3
    OV5640_WR_Reg(0x5587, brtval << 4);
    if(bright < 4) OV5640_WR_Reg(0x5588, 0x09);
    else OV5640_WR_Reg(0x5588, 0x01);
    OV5640_WR_Reg(0x3212, 0x13);   // 结束组3
    OV5640_WR_Reg(0x3212, 0xa3);   // 启动组3
}

// 对比度调整
// contrast: 0~6, 对应-3~3
void OV5640_Contrast(u8 contrast)
{
    u8 reg0val = 0X00; // contrast=3, 默认对比度
    u8 reg1val = 0X20;
    
    switch(contrast)
    {
        case 0: // -3
            reg1val = reg0val = 0X14;
            break;
        case 1: // -2
            reg1val = reg0val = 0X18;
            break;
        case 2: // -1
            reg1val = reg0val = 0X1C;
            break;
        case 4: // +1
            reg0val = 0X10;
            reg1val = 0X24;
            break;
        case 5: // +2
            reg0val = 0X18;
            reg1val = 0X28;
            break;
        case 6: // +3
            reg0val = 0X1C;
            reg1val = 0X2C;
            break;
    }
    
    OV5640_WR_Reg(0x3212, 0x03);   // 启动组3
    OV5640_WR_Reg(0x5585, reg0val);
    OV5640_WR_Reg(0x5586, reg1val);
    OV5640_WR_Reg(0x3212, 0x13);   // 结束组3
    OV5640_WR_Reg(0x3212, 0xa3);   // 启动组3
}

// 锐度调整
// sharp: 0~33, 0-关闭; 33-auto; 其他值-具体级别
void OV5640_Sharpness(u8 sharp)
{
    if(sharp < 33) // 具体锐度值
    {
        OV5640_WR_Reg(0x5308, 0x65);
        OV5640_WR_Reg(0x5302, sharp);
    } else // 自动锐度
    {
        OV5640_WR_Reg(0x5308, 0x25);
        OV5640_WR_Reg(0x5300, 0x08);
        OV5640_WR_Reg(0x5301, 0x30);
        OV5640_WR_Reg(0x5302, 0x10);
        OV5640_WR_Reg(0x5303, 0x00);
        OV5640_WR_Reg(0x5309, 0x08);
        OV5640_WR_Reg(0x530a, 0x30);
    }
}
