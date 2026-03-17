// ov5640.h
#ifndef __OV5640_H
#define __OV5640_H

#include "sys.h"
#include "sccb.h"

#define OV5640_ID 0x5640
#define OV5640_ID_REG_H 0x300A
#define OV5640_ID_REG_L 0x300B

// OV5640�Ĵ�����ַ
#define OV5640_CHIPIDH      0x300A  // оƬID���ֽ�
#define OV5640_CHIPIDL      0x300B  // оƬID���ֽ�

// �ֱ���ö��
typedef enum {
    OV5640_RES_QQVGA = 0,  // 160x120
    OV5640_RES_QVGA,       // 320x240
    OV5640_RES_VGA,        // 640x480
    OV5640_RES_SVGA,       // 800x600
    OV5640_RES_XGA,        // 1024x768
    OV5640_RES_720P,       // 1280x720
    OV5640_RES_WXGA,       // 1280x800
    OV5640_RES_SXGA,       // 1280x1024
    OV5640_RES_UXGA,       // 1600x1200
    OV5640_RES_1080P,      // 1920x1080
    OV5640_RES_QSXGA,      // 2592x1944
    OV5640_RES_MAX
} ov5640_resolution_t;

typedef enum {
    OV5640_MODE_RGB565 = 0,
    OV5640_MODE_JPEG,
    OV5640_MODE_YUV
} ov5640_mode_t;

typedef struct {
    u16 width;
    u16 height;
    const char* name;
} ov5640_res_table_t;

typedef struct {
    ov5640_mode_t mode;
    ov5640_resolution_t resolution;
    u16 width;
    u16 height;
} ov5640_config_t;

// ��������
u8 OV5640_Init(void);
void ov5640_set_mode(ov5640_mode_t mode);
void ov5640_set_resolution(ov5640_resolution_t res);
void ov5640_set_custom_size(u16 width, u16 height);
u8 ov5640_test_communication(void);
u16 ov5640_get_id(void);
void ov5640_enable_test_pattern(void);
void ov5640_disable_test_pattern(void);

// ����ԭ�ӷ��ĺ���// 正点原子方法函数
u8 OV5640_WR_Reg(u16 reg, u8 data);
u8 OV5640_RD_Reg(u16 reg);
u8 OV5640_OutSize_Set(u16 offx, u16 offy, u16 width, u16 height);
void OV5640_Flash_Ctrl(u8 sw);

// 图像质量调整函数
void OV5640_Light_Mode(u8 mode);
void OV5640_Color_Saturation(u8 sat);
void OV5640_Brightness(u8 bright);
void OV5640_Contrast(u8 contrast);
void OV5640_Sharpness(u8 sharp);

#endif
