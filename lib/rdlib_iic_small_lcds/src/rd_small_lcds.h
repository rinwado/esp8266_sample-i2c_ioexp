/**
 * 小規模ＬＣＤライブラリ
 * 対応ＬＣＤ：AQM0802, AQM1602
 * 
 * 2024-11-12  Ver 0.1.0
 * rd_small_lcds.h
 * 
 * 2024-11-12
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */


#ifndef __RD_SMALL_LCDS_H__
#define __RD_SMALL_LCDS_H__

#include <Arduino.h>
#include <Wire.h>
#include "rd_arduino_iic.h"

//----------------------------------------------------------------------------------------------------------
#define LCD_TYPE_AQM0802                (0)
#define LCD_TYPE_AQM1602                (1)
#define IIC_ADDR_AQM0802	            (0x3E)
#define IIC_ADDR_AQM1602	            (0x3E)
#define DEVICE_IS_5V0                   (1)
#define DEVICE_IS_3V3                   (0)
#define LINE_COLUMN08                   (8)
#define LINE_COLUMN16                   (16)
#define LINE1                           (1)
#define LINE2                           (2)
#define LINE_ALL                        (3)
#define LCD_NO_CLR                      (0)
#define LCD_L1BUF_CLR                   (1)
#define LCD_L2BUF_CLR                   (2)
#define LCD_LABUF_CLR                   (3)
#define LCD_BUF_LINE1                   (0) //LCD バッファーアクセス用
#define LCD_BUF_LINE2                   (1) //LCD バッファーアクセス用
#define LCD_NOF_LINE                    (2)


//-----------------------------------------------------------------------------------------------------------
class rd_iic_lcds: public rd_iic_base
{
    public:
        rd_iic_lcds(uint8_t addr = IIC_ADDR_AQM0802, int dv = DEVICE_IS_3V3, int lt = LCD_TYPE_AQM0802);
        //---------------------------------------------------------------------------------------------------
        void initialize(void);
        void display_update(const char* p_str1, uint8_t len1, uint8_t spos1, const char* p_str2, uint8_t len2, uint8_t spos2, uint8_t f_clear, uint8_t f_update);
        void all_clear(void);
        void update_2line(void);
        void LineBuff_Clear(int8_t line_no);

        char lcd_data_buff[LCD_NOF_LINE][20];

    private:
        //---------------------------------------------------------------------------------------------------
        //LCDメニュー関係
        /*
            0011_1000, // 0x38: [Function set] DL(4):1(8-bit) N(3):1(2-line) DH(2):0(5x8 dot) IS(0):0(extension)
            0011_1001, // 0x39: [Function set] DL(4):1(8-bit) N(3):1(2-line) DH(2):0(5x8 dot) IS(0):1(extension)
            0001_0100, // 0x14: [Internal OSC frequency] BS(3):0(1/5blas) F(210):(internal Freq:100)
            0111_0000, // 0x70: [Contrast set] Contrast(3210):0
            0101_0110, // 0x56: [Power/ICON/Contrast control] Ion(3):0(ICON:off) Bon(2):1(booster:on) C5C4(10):10(contrast set)
            0110_1100, // 0x6C: [Follower control] Fon(3):1(on) Rab(210):100
            0011_1000, // 0x38: [Function set] DL(4):1(8-bit) N(3):1(2-line) DH(2):0(5x8 dot) IS(0):0(extension)
            0000_1100, // 0x0C: [Display ON/OFF control] D(2):1(Display:ON) C(1):0(Cursor:OFF) B(0):0(Cursor Blink:OFF)
            0000_0001, // 0x01: [Clear Display]
        */
        char _settings[9] = {0x38, 0x39, 0x14, 0x70, 0x56, 0x6c, 0x38, 0x0c, 0x01};
        int _dev_volt = 0;          //0:3.3V, 1:5.0V
        int _lcd_type = 0;          //0:AQM0802, 1:AQM1602
        int _lcd_line_columns = 8;
    
        //---------------------------------------------------------------------------------------------------
        void _write(char type, char* data, size_t len);
};

#endif  //__RD_SMALL_LCDS_H__