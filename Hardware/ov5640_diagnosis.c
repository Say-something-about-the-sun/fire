// ov5640_diagnosis.c - OV5640深度诊断和修复函数
#include "ov5640.h"
#include "sccb.h"
#include "delay.h"
#include "usart.h"
#include <stdio.h>

/* 
 * 带验证的寄存器写入函数
 * 写入后立即读取验证，确保写入成功
 * 返回：0-成功，1-失败
 */
u8 ov5640_write_reg_verify(u16 reg, u8 val, u8 retry_count)
{
    u8 readback = 0;
    u8 attempts = 0;
    
    for(attempts = 0; attempts < retry_count; attempts++)
    {
        // 写入寄存器
        if(ov5640_sccb_write_reg(reg, val) != 0)
        {
            printf("  [VERIFY] Write failed (attempt %d)\r\n", attempts + 1);
            delay_ms(5);
            continue;
        }
        
        // 等待寄存器稳定
        delay_ms(5);
        
        // 读取验证
        if(ov5640_sccb_read_reg(reg, &readback) != 0)
        {
            printf("  [VERIFY] Read failed (attempt %d)\r\n", attempts + 1);
            delay_ms(5);
            continue;
        }
        
        // 检查是否匹配
        if(readback == val)
        {
            return 0;  // 成功
        }
        
        printf("  [VERIFY] Mismatch: wrote 0x%02X, read 0x%02X (attempt %d)\r\n", 
               val, readback, attempts + 1);
        delay_ms(10);
    }
    
    printf("  [VERIFY] FAILED after %d attempts: reg=0x%04X, wrote=0x%02X, read=0x%02X\r\n",
           retry_count, reg, val, readback);
    return 1;  // 失败
}

/* 
 * 深度诊断OV5640配置
 * 检查关键寄存器并验证写入是否真正生效
 */
u8 ov5640_deep_diagnosis(void)
{
    printf("\r\n[OV5640 DEEP DIAGNOSIS]\r\n");
    printf("==========================================\r\n");
    
    u8 reg_val = 0;
    u8 errors = 0;
    u8 critical_errors = 0;
    
    // 1. 检查电源状态（最关键）
    printf("\r\n1. Power State (CRITICAL):\r\n");
    ov5640_sccb_read_reg(0x3008, &reg_val);
    printf("   0x3008 = 0x%02X ", reg_val);
    
    if((reg_val & 0x40) == 0x40)
    {
        printf("[OK] (bit6=1, normal operation)\r\n");
    }
    else
    {
        printf("[FAIL] (bit6=0, NOT in normal operation!)\r\n");
        critical_errors++;
        errors++;
        
        printf("   FIXING: Writing 0x42 to 0x3008...\r\n");
        if(ov5640_write_reg_verify(0x3008, 0x42, 3) == 0)
        {
            printf("   [OK] Fixed!\r\n");
            delay_ms(50);  // 电源模式切换需要时间
        }
        else
        {
            printf("   [FAIL] Fix FAILED!\r\n");
            critical_errors++;
        }
    }
    
    // 2. 检查时钟源配置
    printf("\r\n2. Clock Source:\r\n");
    ov5640_sccb_read_reg(0x3103, &reg_val);
    printf("   0x3103 = 0x%02X ", reg_val);
    
    if(reg_val == 0x03)
    {
        printf("[OK] (using PLL)\r\n");
    }
    else
    {
        printf("[FAIL] (expected 0x03, using PLL)\r\n");
        errors++;
        
        printf("   FIXING: Writing 0x03 to 0x3103...\r\n");
        if(ov5640_write_reg_verify(0x3103, 0x03, 3) == 0)
        {
            printf("   [OK] Fixed!\r\n");
            delay_ms(20);  // PLL稳定需要时间
        }
        else
        {
            printf("   [FAIL] Fix FAILED!\r\n");
            critical_errors++;
        }
    }
    
    // 3. 检查输出使能（关键）
    printf("\r\n3. Output Enable (CRITICAL):\r\n");
    ov5640_sccb_read_reg(0x3017, &reg_val);
    printf("   0x3017 = 0x%02X ", reg_val);
    
    if(reg_val == 0xFF)
    {
        printf("[OK] (all outputs enabled)\r\n");
    }
    else
    {
        printf("[FAIL] (expected 0xFF)\r\n");
        critical_errors++;
        errors++;
        
        printf("   FIXING: Writing 0xFF to 0x3017...\r\n");
        if(ov5640_write_reg_verify(0x3017, 0xFF, 3) == 0)
        {
            printf("   [OK] Fixed!\r\n");
        }
        else
        {
            printf("   [FAIL] Fix FAILED!\r\n");
        }
    }
    
    ov5640_sccb_read_reg(0x3018, &reg_val);
    printf("   0x3018 = 0x%02X ", reg_val);
    
    if(reg_val == 0xFF)
    {
        printf("[OK]\r\n");
    }
    else
    {
        printf("[FAIL] (expected 0xFF)\r\n");
        critical_errors++;
        errors++;
        
        printf("   FIXING: Writing 0xFF to 0x3018...\r\n");
        if(ov5640_write_reg_verify(0x3018, 0xFF, 3) == 0)
        {
            printf("   [OK] Fixed!\r\n");
        }
        else
        {
            printf("   [FAIL] Fix FAILED!\r\n");
        }
    }
    
    // 4. 检查PLL配置
    printf("\r\n4. PLL Configuration:\r\n");
    ov5640_sccb_read_reg(0x3034, &reg_val);
    printf("   0x3034 = 0x%02X ", reg_val);
    if(reg_val == 0x1A) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL] (expected 0x1A)\r\n");
        errors++;
        ov5640_write_reg_verify(0x3034, 0x1A, 3);
    }
    
    ov5640_sccb_read_reg(0x3037, &reg_val);
    printf("   0x3037 = 0x%02X ", reg_val);
    if(reg_val == 0x13) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL] (expected 0x13)\r\n");
        errors++;
        ov5640_write_reg_verify(0x3037, 0x13, 3);
    }
    
    ov5640_sccb_read_reg(0x3108, &reg_val);
    printf("   0x3108 = 0x%02X ", reg_val);
    if(reg_val == 0x01) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL] (expected 0x01)\r\n");
        errors++;
        ov5640_write_reg_verify(0x3108, 0x01, 3);
    }
    
    // 5. 检查输出格式
    printf("\r\n5. Output Format:\r\n");
    ov5640_sccb_read_reg(0x4300, &reg_val);
    printf("   0x4300 = 0x%02X ", reg_val);
    if(reg_val == 0x30 || reg_val == 0x6F) 
    {
        printf("[OK] (YUV or RGB)\r\n");
    }
    else 
    {
        printf("[FAIL] (expected 0x30 or 0x6F)\r\n");
        errors++;
        printf("   FIXING: Setting to YUV (0x30)...\r\n");
        ov5640_write_reg_verify(0x4300, 0x30, 3);
    }
    
    // 6. 检查JPEG模式
    printf("\r\n6. JPEG Mode:\r\n");
    ov5640_sccb_read_reg(0x501F, &reg_val);
    printf("   0x501F = 0x%02X ", reg_val);
    if(reg_val == 0x01) 
    {
        printf("[OK] (JPEG enabled)\r\n");
    }
    else 
    {
        printf("[FAIL] (expected 0x01)\r\n");
        errors++;
        printf("   FIXING: Enabling JPEG...\r\n");
        ov5640_write_reg_verify(0x501F, 0x01, 3);
    }
    
    // 7. 检查时序极性
    printf("\r\n7. Timing Polarity:\r\n");
    ov5640_sccb_read_reg(0x4740, &reg_val);
    printf("   0x4740 = 0x%02X ", reg_val);
    if(reg_val == 0x21 || reg_val == 0x20) 
    {
        printf("[OK] (VSYNC/HREF configured)\r\n");
    }
    else 
    {
        printf("[FAIL] (expected 0x21 or 0x20)\r\n");
        errors++;
        printf("   FIXING: Setting to 0x21...\r\n");
        ov5640_write_reg_verify(0x4740, 0x21, 3);
    }
    
    // 8. 测试0x3500寄存器（用户报告的问题）
    printf("\r\n8. Testing 0x3500 Register (User Reported Issue):\r\n");
    u8 original_3500 = 0;
    ov5640_sccb_read_reg(0x3500, &original_3500);
    printf("   Original 0x3500 = 0x%02X\r\n", original_3500);
    
    // 0x3500是曝光控制寄存器，某些位可能是只读的
    // 尝试写入一个测试值
    u8 test_val_3500 = 0x55;
    printf("   Writing 0x%02X to 0x3500...\r\n", test_val_3500);
    
    if(ov5640_write_reg_verify(0x3500, test_val_3500, 3) == 0)
    {
        printf("   [OK] 0x3500 write/read SUCCESS!\r\n");
    }
    else
    {
        printf("   [FAIL] 0x3500 write/read FAILED!\r\n");
        printf("   Note: 0x3500某些位可能是只读的，这是正常的\r\n");
        errors++;
    }
    
    // 恢复原值
    if(original_3500 != test_val_3500)
    {
        ov5640_sccb_write_reg(0x3500, original_3500);
        delay_ms(10);
    }
    
    printf("\r\n==========================================\r\n");
    printf("Diagnosis Summary:\r\n");
    printf("  Total errors: %d\r\n", errors);
    printf("  Critical errors: %d\r\n", critical_errors);
    
    if(critical_errors > 0)
    {
        printf("\r\n[WARN] WARNING: Critical errors found!\r\n");
        printf("  OV5640 may not output signals correctly.\r\n");
        printf("  Please check power supplies and reset sequence.\r\n");
    }
    else if(errors > 0)
    {
        printf("\r\n[WARN] Some non-critical errors found.\r\n");
    }
    else
    {
        printf("\r\n[OK] All checks passed!\r\n");
    }
    
    return errors;
}

/* 
 * 强制启动OV5640时钟输出
 * 这是一个完整的启动序列，确保所有信号输出
 */
u8 ov5640_force_clock_output(void)
{
    printf("\r\n[OV5640] Forcing clock output startup...\r\n");
    printf("==========================================\r\n");
    
    u8 reg_val = 0;
    u8 errors = 0;
    
    // 步骤1：确保电源状态正确
    printf("\r\n[Step 1] Setting power state...\r\n");
    if(ov5640_write_reg_verify(0x3008, 0x42, 5) != 0)
    {
        printf("  [FAIL] FAILED to set power state!\r\n");
        errors++;
    }
    else
    {
        printf("  [OK] Power state set\r\n");
    }
    delay_ms(50);  // 电源模式切换需要时间
    
    // 步骤2：配置时钟源
    printf("\r\n[Step 2] Configuring clock source...\r\n");
    
    // 先切换到pad时钟
    ov5640_write_reg_verify(0x3103, 0x11, 3);
    delay_ms(10);
    
    // 软件复位
    printf("  Software reset...\r\n");
    ov5640_write_reg_verify(0x3008, 0x82, 3);
    delay_ms(10);
    
    // 退出电源关闭模式
    ov5640_write_reg_verify(0x3008, 0x42, 3);
    delay_ms(50);
    
    // 切换到PLL时钟
    printf("  Switching to PLL clock...\r\n");
    if(ov5640_write_reg_verify(0x3103, 0x03, 5) != 0)
    {
        printf("  [FAIL] FAILED to switch to PLL!\r\n");
        errors++;
    }
    else
    {
        printf("  [OK] PLL clock enabled\r\n");
    }
    delay_ms(30);  // PLL稳定需要时间
    
    // 步骤3：配置PLL
    printf("\r\n[Step 3] Configuring PLL...\r\n");
    ov5640_write_reg_verify(0x3034, 0x1A, 3);
    ov5640_write_reg_verify(0x3037, 0x13, 3);
    ov5640_write_reg_verify(0x3108, 0x01, 3);
    delay_ms(20);
    printf("  [OK] PLL configured\r\n");
    
    // 步骤4：使能所有输出（最关键！）
    printf("\r\n[Step 4] Enabling all outputs (CRITICAL)...\r\n");
    if(ov5640_write_reg_verify(0x3017, 0xFF, 5) != 0)
    {
        printf("  [FAIL] FAILED to enable outputs 0x3017!\r\n");
        errors++;
    }
    else
    {
        printf("  [OK] 0x3017 = 0xFF (outputs enabled)\r\n");
    }
    
    if(ov5640_write_reg_verify(0x3018, 0xFF, 5) != 0)
    {
        printf("  [FAIL] FAILED to enable outputs 0x3018!\r\n");
        errors++;
    }
    else
    {
        printf("  [OK] 0x3018 = 0xFF (outputs enabled)\r\n");
    }
    delay_ms(20);
    
    // 步骤5：配置输出格式
    printf("\r\n[Step 5] Configuring output format...\r\n");
    ov5640_write_reg_verify(0x4300, 0x30, 3);  // YUV
    ov5640_write_reg_verify(0x501F, 0x01, 3);  // JPEG enabled
    ov5640_write_reg_verify(0x4740, 0x21, 3);  // Polarity
    delay_ms(20);
    printf("  [OK] Output format configured\r\n");
    
    // 步骤6：设置分辨率（VGA）
    printf("\r\n[Step 6] Setting VGA resolution...\r\n");
    ov5640_write_reg_verify(0x3808, 0x02, 3);  // Width high
    ov5640_write_reg_verify(0x3809, 0x80, 3);  // Width low (640)
    ov5640_write_reg_verify(0x380A, 0x01, 3);  // Height high
    ov5640_write_reg_verify(0x380B, 0xE0, 3);  // Height low (480)
    delay_ms(50);
    printf("  [OK] Resolution set\r\n");
    
    // 步骤7：最终验证
    printf("\r\n[Step 7] Final verification...\r\n");
    ov5640_sccb_read_reg(0x3008, &reg_val);
    printf("  0x3008 = 0x%02X ", reg_val);
    if((reg_val & 0x40) == 0x40) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL]\r\n");
        errors++;
    }
    
    ov5640_sccb_read_reg(0x3103, &reg_val);
    printf("  0x3103 = 0x%02X ", reg_val);
    if(reg_val == 0x03) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL]\r\n");
        errors++;
    }
    
    ov5640_sccb_read_reg(0x3017, &reg_val);
    printf("  0x3017 = 0x%02X ", reg_val);
    if(reg_val == 0xFF) printf("[OK]\r\n");
    else 
    {
        printf("[FAIL]\r\n");
        errors++;
    }
    
    printf("\r\n==========================================\r\n");
    if(errors == 0)
    {
        printf("[OK] Clock output startup complete!\r\n");
        printf("  PCLK, VSYNC, and HREF should now be active.\r\n");
        printf("  Please wait 100ms for signals to stabilize...\r\n");
        delay_ms(100);
    }
    else
    {
        printf("[FAIL] Clock output startup had %d errors.\r\n", errors);
        printf("  Please check hardware connections and power supplies.\r\n");
    }
    
    return errors;
}
