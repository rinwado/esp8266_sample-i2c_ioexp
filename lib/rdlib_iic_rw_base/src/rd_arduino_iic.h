/**
 * Arduino IIC ライブラリ
 * 基本的な、IICアクセスに関するAPI
 * 
 * 2024-11-14
 * rd_arduino_iic.h
 * 
 * 2024-11-14
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

#ifndef __RD_ARDUINO_IIC_H__
#define __RD_ARDUINO_IIC_H__

#include <Arduino.h>
#include <Wire.h>

//----------------------------------------------------------------------------------------------------------
enum rd_iic_base_err
{
    IIC_SUCCESS = 0,
    IIC_ERR_RX_TIMEOUT,
    IIC_ERR_ACK_POLL_TIMEOUT,
};


class rd_iic_base
{
    public:
        rd_iic_base(uint8_t addr = 0x00);
        //----------------------------------------------------------------------------------------------------------
        void write_only(char* data, size_t len, uint32_t dly_us);
        int read_only(char* data, size_t len, uint32_t tmout_val, uint32_t* tmout_counter);
        int read_after_write(char* p_wd, size_t wlen, char* p_rd, size_t rlen, uint32_t tmout_val, uint32_t* tmout_counter);
        int poll_ack(uint32_t tmout_val, uint32_t* tmout_counter);
        void add_command_to_sa(uint8_t cmd, uint8_t mask);
    private:
        //----------------------------------------------------------------------------------------------------------
        uint8_t _i2c_7bit_address;
        uint32_t _ref_microsec_time;
        uint32_t _now_microsec_time;

};
#endif  //__RD_ARDUINO_IIC_H__