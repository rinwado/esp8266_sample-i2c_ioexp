/**
 * Arduino IIC ライブラリ
 * 基本的な、IICアクセスに関するAPI
 * 
 * 2024-11-14
 * rd_arduino_iic.cpp
 *
 * 2024-11-14
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

//----------------------------------------------------------------------------------------------------------
#include "rd_arduino_iic.h"

#define IIC_STOP_END            (1)
#define IIC_NON_STOP_END        (0)

//----------------------------------------------------------------------------------------------------------
rd_iic_base::rd_iic_base(uint8_t addr)
{	_i2c_7bit_address = addr;
}


//===== パブリック関数 ======================================================================================
/*
 * IIC書込
 * sa		: IICデバイスのアドレス
 * data		: 書き込むデータのポインタ
 * len		: 書き込むデータの数
 * dly_us   : データ送信後の待ち時間、０または５０００を超えた場合は待ち時間なし
 *
*/
void rd_iic_base::write_only(char* p_data, size_t len, uint32_t dly_us)
{   int r;

    Wire.beginTransmission(_i2c_7bit_address);
    for(r=0; r<(int)len; r++)	
		Wire.write((byte)p_data[r]);
	Wire.endTransmission((uint8_t)IIC_STOP_END); //最後にSTOPコンディション
    if((0 != dly_us) && (5000 >= dly_us))
    {   if(50 > dly_us)
        {   delayMicroseconds(dly_us);
        }
        else
        {   _ref_microsec_time = micros();
            while((micros() - _ref_microsec_time) < dly_us)
            {   yield();   //for esp
            }
        }
    }
}


/*
 * IIC読込（読込みのみで、）
 * sa			: IICデバイスのアドレス
 * data			: 受取るデータのポインタ
 * len			: 読込むデータの数
 * tmout_val	: タイムアウト数、「０」の場合はタイムアウトチェックなし、それ以外は「tmout_counter」ポインターで指定されたカウンタと比較されタイムアウトをチェックする。
 * tmout_counter: タイムアウトカウンターのポインター（別プログラムでフリーランニングカウントされる必要がある）
 *
*/
int rd_iic_base::read_only(char* p_data, size_t len, uint32_t tmout_val, uint32_t* tmout_counter)
{
    bool f_tmout;
    int _len, rx_len, r;
    char* _p_data;

    _p_data = p_data;
    _len = (int)len;
    f_tmout = false;
    rx_len = Wire.requestFrom((int)_i2c_7bit_address, _len, (int)IIC_STOP_END);  //最後にSTOPコンディション
    if(tmout_counter) *tmout_counter = 0;               //タイムアウトカウンタクリア
    f_tmout = false;
    while(_len > Wire.available())
    {   if(tmout_counter)
        {   if((0 < tmout_val) && (tmout_val <= *tmout_counter))
            {   //タイムアウトチェックをする
                f_tmout = true;
                break;
            }
        }
        delay(1);
    }

    if((!f_tmout) && (_len <= rx_len))
    {   //タイムアウト発生していなく、受信バイトが要求した数以上の場合
        for(r=0; r<rx_len; r++)
        {   if(r < _len)
               *_p_data++ = Wire.read();  //１バイトデータを読込保存
            else
                Wire.read();            //要求数より多くのデータはダミー読込
        }
        return rx_len;                  //読込んだデータ数を返す（要求より多い可能性もあり）
    }
    else
    {   //タイムアウト発生
        return -IIC_ERR_RX_TIMEOUT;
    }
}


/*
 * IIC読込（書込み操作の後に、読込み）
 * sa			: IICデバイスのアドレス
 * p_wd		    : 書込むデータのポインタ
 * wlen			: 書込むデータの数
 * p_rd		    : 読込むデータのポインタ
 * rlen			: 読込むデータの数
 * tmout_val	: タイムアウト数、「０」の場合はタイムアウトチェックなし、それ以外は「tmout_counter」ポインターで指定されたカウンタと比較されタイムアウトをチェックする。
 * tmout_counter: タイムアウトカウンターのポインター（別プログラムでフリーランニングカウントされる必要がある）
 *
*/
int rd_iic_base::read_after_write(char* p_wd, size_t wlen, char* p_rd, size_t rlen, uint32_t tmout_val, uint32_t* tmout_counter)
{
    bool f_tmout;
    int _rlen, rx_len, r;
    char* _p_rd;

    Wire.beginTransmission(_i2c_7bit_address);
    for(r=0; r<(int)wlen; r++)
		Wire.write((byte)p_wd[r]);
	Wire.endTransmission((uint8_t)IIC_NON_STOP_END); //リスタート

    _p_rd = p_rd;
    _rlen = (int)rlen;
    f_tmout = false;
    rx_len = Wire.requestFrom((int)_i2c_7bit_address, _rlen, (int)IIC_STOP_END); //最後にSTOPコンディション
    if(tmout_counter) *tmout_counter = 0;       //タイムアウトカウンタクリア
    f_tmout = false;
    while(_rlen > Wire.available())
    {   if(tmout_counter)
        {   if((0 < tmout_val) && (tmout_val <= *tmout_counter))
            {   //タイムアウトチェックをする
                f_tmout = true;
                break;
            }
        }
        delay(1);
    }

    if((!f_tmout) && (_rlen <= rx_len))
    {   //タイムアウト発生していなく、受信バイトが要求した数以上の場合
        for(r=0; r<rx_len; r++)
        {   if(r < _rlen)
               *_p_rd++ = Wire.read();  //１バイトデータを読込保存
            else
                Wire.read();            //要求数より多くのデータはダミー読込
        }
        return rx_len;                  //読込んだデータ数を返す（要求より多い可能性もあり）
    }
    else
    {   //タイムアウト発生
        return -IIC_ERR_RX_TIMEOUT;
    }
}

/*
 * IIC ACKポーリング　IIC EEPROM用
 * sa		: IICデバイスのアドレス
 * data		: 書き込むデータのポインタ
 * len		: 書き込むデータの数
 * dly_us   : データ送信後の待ち時間（０）は待ち時間なし
 * 戻り値    : 成功時、書込みにかかったループカウント（１カウント１ｍｓ） 
 * 　　　　　　タイムアウト時は、「IIC_ERR_ACK_POLL_TIMEOUT」のマイナス値
 * 
 * endTransmission (戻り値)
 * 0: 成功
 * 1: 送ろうとしたデータが送信バッファのサイズを超えた
 * 2: アドレスを送信し、NACKを受信した
 * 3: データを送信し、NACKを受信した
 * 4: その他のエラー
 * 5: タイムアウト
 * 
*/
int rd_iic_base::poll_ack(uint32_t tmout_val, uint32_t* tmout_counter)
{   int ret, cnt;
    bool f_tmout;

    cnt = 0;

    f_tmout = false;
    if(tmout_counter) *tmout_counter = 0;
    do
	{   delay(1); cnt++;
        if(tmout_counter)
        {   if((0 < tmout_val) && (tmout_val <= *tmout_counter))
            {   //time out
                f_tmout = true;
                break;
            }
        }
        Wire.beginTransmission(_i2c_7bit_address);
		ret = Wire.endTransmission((uint8_t)IIC_STOP_END); //最後にSTOPコンディション
	} while(0 != ret);

    if(f_tmout)
        return -IIC_ERR_ACK_POLL_TIMEOUT;
    else
        return cnt;
}


/*
 * IIC スレーブアドレスにコマンドが含まれる場合の「SA」にコマンド情報を付加する
 * cmd		: コマンドデータ
 * mask		: SAのマスクビット
 *
*/
void rd_iic_base::add_command_to_sa(uint8_t cmd, uint8_t mask)
{
    _i2c_7bit_address = (uint8_t)((uint8_t)(_i2c_7bit_address & mask) | (uint8_t)(cmd & (uint8_t)(~mask)));
}

//===== プライベート関数 ======================================================================================

