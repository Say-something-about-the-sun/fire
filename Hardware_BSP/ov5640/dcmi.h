// dcmi.h
#ifndef __DCMI_H
#define __DCMI_H

#include "sys.h"

#define DCMI_RGB565_BUFFER_SIZE  (640 * 480 * 2)
#define DCMI_FRAME_TIMEOUT_MS    1000

typedef enum {
    DCMI_STATE_IDLE = 0,
    DCMI_STATE_CAPTURING,
    DCMI_STATE_FRAME_READY,
    DCMI_STATE_ERROR
} dcmi_state_t;

typedef struct {
    u8* buffer;
    u32 size;
    u16 width;
    u16 height;
    u8 is_valid;
} dcmi_frame_t;

typedef struct {
    dcmi_state_t state;
    dcmi_frame_t frame;
    u32 frame_count;
    u32 error_count;
    u32 last_frame_time;
} dcmi_handle_t;

u8 dcmi_init(void);
void dcmi_deinit(void);
u8 dcmi_start_capture(u8* buffer, u32 buffer_size);
void dcmi_stop_capture(void);
dcmi_state_t dcmi_get_state(void);


u8 dcmi_is_frame_ready(void);
dcmi_frame_t* dcmi_get_frame(void);
void dcmi_release_frame(void);


u32 dcmi_get_frame_count(void);
void dcmi_reset_frame_count(void);

void DCMI_IRQHandler(void);
void DMA2_Stream1_IRQHandler(void);

void DCMI_Start(void);
void DCMI_Stop(void);

u32 dcmi_get_error_count(void);
void dcmi_diagnose(void);

void DCMI_DMA_Init(u32 DMA_Memory0BaseAddr,u16 DMA_BufferSize,u32 DMA_MemoryDataSize,u32 DMA_MemoryInc);
void My_DCMI_Init(void);

#endif
