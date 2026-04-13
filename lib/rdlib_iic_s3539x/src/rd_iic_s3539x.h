/**
 * ＩＩＣインターフェイスのリアルタイムクロックＩＣ　Ｓ－３５３９ｘのライブラリ
 * 
 * 2024-11-17
 * Ver 0.2.0            //2026-04-05 S-3590にも利用するため、関数名等を変更
 * rd_iic_s3539x.h
 * 
 * 2024-11-17
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

#ifndef __RD_IIC_S3539X_H__
#define __RD_IIC_S3539X_H__

#include <Arduino.h>
#include <Wire.h>

#include "rd_arduino_iic.h"

//--- S-3539xA
#define IIC_DEV_CODE_S35391         (0x58)  //S-35391Aデバイスコード「0xB」とコマンド3ビットの組み合わせで、７ビット（IICのSAになる）
#define IIC_DEV_CODE_S35390         (0x30)  //S-35390Aデバイスコード「0x6」とコマンド3ビットの組み合わせで、７ビット（IICのSAになる）

#define S3539x_CMD_MASK             (0xF8)  //1111 1000: 下位3ビットがコマンド
#define S3539x_CMD_STATUS1          (0x00)  //ステータスレジスタ1アクセス
#define S3539x_CMD_STATUS2          (0x01)  //ステータスレジスタ2アクセス
#define S3539x_CMD_REAL_TIME_DATA1  (0x02)  //リアルタイムデータ1アクセス、年データ～
#define S3539x_CMD_REAL_TIME_DATA2  (0x03)  //リアルタイムデータ1アクセス、時データ～
#define S3539x_CMD_INT_REGISTER1    (0x04)  //INT1レジスタアクセス
#define S3539x_CMD_INT_REGISTER2    (0x05)  //INT2レジスタアクセス
#define S3539x_CMD_CLOCK_ADJ_REG    (0x06)  //クロック補正レジスタアクセ
#define S3539x_CMD_FREE_REGISTER    (0x07)  //フリーレジスタアクセス

#define S3539x_BIT_RESET            (0x80)
#define S3539x_BIT_12L24H           (0x40)
#define S3539x_BIT_SC0              (0x20)
#define S3539x_BIT_SC1              (0x10)
#define S3539x_BIT_INT1             (0x08)
#define S3539x_BIT_INT2             (0x04)
#define S3539x_BIT_BLD              (0x02)
#define S3539x_BIT_POC              (0x01)

#define AMPM_BIT                    (0x40)   


enum rd_iic_s3539x_err
{
    S3539x_SUCCESS = 0,
    S3539x_ERR_PARAM,
    S3539x_ERR_RX_TIMEOUT,
    S3539x_ERR_ACK_POLL_TIMEOUT,
    S3539x_ERR_REG_READ,
    S3539x_ERR_READ_DATA_COUNT,
};


typedef struct s3539x_time_data
{
    uint8_t year;       //BCD Code 0-99
    uint8_t month;      //BCD Code 1-12
    uint8_t date;       //BCD Code 1-31
    uint8_t week;       //0-6
    uint8_t hour;       //BCD Code 0-23, 0-11, AM/PM
    uint8_t minute;     //BCD Code 0-59
    uint8_t second;     //BCD Code 0-59
    uint16_t year_four_digit;   //2000 + year(0-99)
    bool hour24;        //false: 12時間表示, true: 24時間表示
    bool aml_pmh;       //false: AM, true: PM
} s3539x_time_data_t;




class rd_iic_s3539x: public rd_iic_base
{
    public:
        s3539x_time_data_t s3539x_times;
        char weeks[7][5] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sta"};

        //----------------------------------------------------------------------------------------------------------
        rd_iic_s3539x(uint8_t addr = 0x00);
        //----------------------------------------------------------------------------------------------------------
        int read_register(uint8_t cmd, uint8_t* p_data, uint8_t len);
        int write_register(uint8_t cmd, uint8_t* p_data, uint8_t len);
        int initialization(bool* alm1, bool* alm2);

        int set_time(s3539x_time_data_t* p_data);
        int get_time(s3539x_time_data_t* p_data);
        int set_alarm_time(s3539x_time_data_t* p_data, int alm_no, bool week_en);
        int get_alarm_time(s3539x_time_data_t* p_data, int alm_no);
        int alarm_control(int rl_wh, bool* alm1, bool* alm2);
        int check_for_alarms(bool* alm1, bool* alm2);
        uint16_t BCD_to_int(uint8_t bcd_data);
        uint8_t int_to_BCD(uint16_t int_data);

    private:
        //----------------------------------------------------------------------------------------------------------
        uint8_t byte_mirror_flipping(uint8_t in_data);
        //----------------------------------------------------------------------------------------------------------       

};

#endif  //__RD_IIC_S3539X_H__