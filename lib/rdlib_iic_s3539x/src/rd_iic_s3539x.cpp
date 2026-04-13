/**
 * ＩＩＣインターフェイスのリアルタイムクロックＩＣ　Ｓ－３５３９ｘのライブラリ
 * 
 * 2024-11-17
 * Ver 0.1.2            //2024-12-08 バグ修正、アラーム時刻セット、ゲット、アラーム制御のセット、ゲット　ＡＰＩ追加
 * Ver 0.2.0            //2026-04-05 S-3590にも利用するため、関数名等を変更
 * rd_iic_s3539x.cpp
 * 
 * 2024-11-17
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

//----------------------------------------------------------------------------------------------------------
#include "rd_iic_s3539x.h"

//----------------------------------------------------------------------------------------------------------


//===== パブリック関数 ======================================================================================
rd_iic_s3539x::rd_iic_s3539x(uint8_t addr) : rd_iic_base(addr)	//基底クラスのコンストラクタ名を指定して()の中に引数を指定します。そうするとその引数の内容に当てはまるコンストラクタが呼び出されます。
{
}


/*
 * S-3539x:の指定されたコマンドによるレジスターを読み込みます。
 * cmd          : S-3539xのコマンド
 * p_data       : S-3539xの読込んだデータを格納するポインタ
 * len          : 読込むデータの数 
 * 戻り値       : エラーは負の値、成功で読込んだバイト数
*/
int rd_iic_s3539x::read_register(uint8_t cmd, uint8_t* p_data, uint8_t len)
{
    //s3539x で連続読込バイト数は７バイトが最大
    uint8_t _cmd;

    if((NULL == p_data) || (7 < len) || (0 == len))
        return -S3539x_ERR_PARAM;

    _cmd = cmd & (uint8_t)(~S3539x_CMD_MASK);       //コマンド ビットの下位３ビットを抽出
    add_command_to_sa(_cmd, S3539x_CMD_MASK);       //SA にコマンドが存在する場合に、SA　にコマンドを負荷する。
    return read_only((char*)p_data, (size_t)len, 0, NULL);
}

/*
 * S-3539x:の指定されたコマンドによるレジスターにデータを書き込む。
 * cmd          : S-3539xのコマンド
 * p_data       : S-3539xの書込むデータが格納されているポインタ
 * len          : 書込むデータの数 
 * 戻り値       : エラーは負の値、成功で読込んだバイト数
*/
int rd_iic_s3539x::write_register(uint8_t cmd, uint8_t* p_data, uint8_t len)
{
    //s3539x で連続読込バイト数は７バイトが最大
    uint8_t _cmd;

    if((NULL == p_data) || (7 < len) || (0 == len))
        return -S3539x_ERR_PARAM;

    _cmd = cmd & (uint8_t)(~S3539x_CMD_MASK);       //コマンド ビットの下位３ビットを抽出
    add_command_to_sa(_cmd, S3539x_CMD_MASK);       //SA にコマンドが存在する場合に、SA　にコマンドを負荷する。
    write_only((char*)p_data, (size_t)len, 0);

    return S3539x_SUCCESS;
}

/*
 * S-3539x:の初期化
 * alm1         : アラーム１のフラグがコピーされます
 * alm2         : アラーム２のフラグがコピーされます
 * 
 * 戻り値       : 「０」は電源異常でなく、電源投入検出されなかった。
 * 　　　　　　　　「０」以外は、リセットを掛けたフラグ「bit7」を１にして、BLD、POC　の２ビットの状態が返される
 * 　　　　　　　　Bit7 が「１」の場合は、時計データ等が初期されているので、再設定の必要がある事を示す
*/
int rd_iic_s3539x::initialization(bool* alm1, bool* alm2)
{
    //s3539x で連続読込バイト数は７バイトが最大
    char data[2];
    bool f_reset;

    add_command_to_sa(S3539x_CMD_STATUS1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(2 == read_only(data, 2, 0, NULL))    //STATUS1, 2
    {   //読込成功
        f_reset = false;
        if(data[0] & S3539x_BIT_POC)
        {   delay(500);
            f_reset = true;                     //チップに電源が入った
        }
        else
        {   if(data[0] & S3539x_BIT_BLD)
                f_reset = true;                 //電圧が下がった
        }

        if(f_reset)
        {   //s3539x にリセット掛ける（この時点で、コマンドは STATUS1 を維持）
            data[0] = S3539x_BIT_RESET;
            write_only(&data[0], 1, 0);
            delay(5);
            read_only(&data[0], 1, 0, NULL);    //STATUS1
            return (int)(data[0] | 0x80);       //bit7 でリセットしたことを知らせ、他の状態を知らせる
        }
        else
        {   //s3539x にチップにリセットかける必要はなく、この時点で、コマンドは STATUS1 を維持
            if(NULL != alm1)
            {   //アラーム１確認
                *alm1 = false;
                if(0 != (data[0] & S3539x_BIT_INT1))            //check STATUS1-bit3
                {   //アラーム発生が確認された
                    data[1] = (char)(data[1] & (char)(~0x21));  //clear STATUS2 INT1AE,TEST bit, INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
                    add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
                    write_only(&data[1], 1, 0);
                    *alm1 = true;
                }
            }

            if(NULL != alm2)
            {   //アラーム２確認
                *alm2 = false;
                if(0 != (data[0] & S3539x_BIT_INT2))            //check STATUS1-bit2
                {   //アラーム発生が確認された
                    data[1] = (char)(data[1] & (char)(~0x03));  //clear STATUS2 INT2AE,TEST bit, INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
                    add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
                    write_only(&data[1], 1, 0);
                    *alm2 = true;
                }
            }
            return S3539x_SUCCESS;
        }
    }
    else
    {   return -S3539x_ERR_REG_READ;
    }
}

/*
 * S-35391:のリアルタイムデータ１の７つのレジスターにデータを書き込む。
 * p_data       : S-35391の書込む s3539x_time_data_t 構造体のポインタ
 * 戻り値       : エラーは負の値、成功で「０」
*/
int rd_iic_s3539x::set_time(s3539x_time_data_t* p_data)
{
    char wd[8];

    if(NULL == p_data)
        return -S3539x_ERR_PARAM;

    //12h 24h のセット
    add_command_to_sa(S3539x_CMD_STATUS1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    wd[0] = 0x00;
    if(p_data->hour24)
        wd[0] = S3539x_BIT_12L24H;
    write_only(wd, 1, 0);
    //AM, PM のセット
    if(p_data->aml_pmh)
        p_data->hour |= AMPM_BIT;               //set bit
    else
        p_data->hour &= (uint8_t)(~AMPM_BIT);   //clear bit
    //４桁の西暦値
    p_data->year_four_digit = (uint16_t)(2000 + (int)BCD_to_int(p_data->year));
    //年月日週時分秒のセット
    wd[0] = (char)byte_mirror_flipping(p_data->year);
    wd[1] = (char)byte_mirror_flipping(p_data->month);
    wd[2] = (char)byte_mirror_flipping(p_data->date);
    wd[3] = (char)byte_mirror_flipping(p_data->week);
    wd[4] = (char)byte_mirror_flipping(p_data->hour);
    wd[5] = (char)byte_mirror_flipping(p_data->minute);
    wd[6] = (char)byte_mirror_flipping(p_data->second);
    //RTCへのデータ書き込み
    add_command_to_sa(S3539x_CMD_REAL_TIME_DATA1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    write_only(wd, 7, 0);

    return S3539x_SUCCESS;
}


/*
 * S-35391:のリアルタイムデータ１の７つのレジスターからデータを読込、s3539x_time_data_t 構造体に入れる。
 * p_data       : S-35391の書込む s3539x_time_data_t 構造体のポインタ
 * 戻り値       : エラーは負の値、成功で「０」
*/
int rd_iic_s3539x::get_time(s3539x_time_data_t* p_data)
{
    int ret;
    char rd[8];

    if(NULL == p_data)
        return -S3539x_ERR_PARAM;

    ret = 0;
    //RTCからデータの読込み
    add_command_to_sa(S3539x_CMD_REAL_TIME_DATA1, S3539x_CMD_MASK);       //SA にコマンドが存在する場合に、SA　にコマンドを負荷する。
    ret += read_only(rd, 7, 0, NULL);
    if(7 == ret)
    {   //読込成功
        //年月日週時分秒のセット
        p_data->year   = byte_mirror_flipping((uint8_t)rd[0]);
        p_data->month  = byte_mirror_flipping((uint8_t)(rd[1] & 0xF8)); //不要なビットは「０」にする
        p_data->date   = byte_mirror_flipping((uint8_t)(rd[2] & 0xFC)); //不要なビットは「０」にする
        p_data->week   = byte_mirror_flipping((uint8_t)(rd[3] & 0xE0)); //不要なビットは「０」にする
        p_data->hour   = byte_mirror_flipping((uint8_t)(rd[4] & 0xFC)); //不要なビットは「０」にする
        p_data->minute = byte_mirror_flipping((uint8_t)(rd[5] & 0xFE)); //不要なビットは「０」にする
        p_data->second = byte_mirror_flipping((uint8_t)(rd[6] & 0xFE)); //不要なビットは「０」にする
        //AM, PM のセット
        if(rd[4] & 0x02)    //check *AM/PM　bit
            p_data->aml_pmh = true;
        else
            p_data->aml_pmh = false;
        //４桁の西暦値
        p_data->year_four_digit = (uint16_t)(2000 + (int)BCD_to_int(p_data->year));    
    }

    //12h 24h の読込
    add_command_to_sa(S3539x_CMD_STATUS1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(1 == read_only(rd, 1, 0, NULL))
    {   //読込成功
        ret++;
        if(rd[0] & S3539x_BIT_12L24H)
            p_data->hour24 = true;
        else
            p_data->hour24 = false;
    }
    
    if(8 == ret)
        return S3539x_SUCCESS;
    else
        return S3539x_ERR_READ_DATA_COUNT;
}

/*
 * S-35391:のアラームを設定する。
 * p_data       : S-35391の書込む s3539x_time_data_t 構造体のポインタ
 * 　　　　　　　　利用するメンバー変数は、week/hour/minute/aml_pmh で、この変数を元にアラーム割込みを設定する
 *                注意：時刻設定で、１２時、２４時表示の設定合わせたアラーム時刻を設定する必要がある。
 * alm_no       : アラーム番号　１，２
 * week_en      : 週アラームをするかしないか true:する
 * 
 * 戻り値       : エラーは負の値、成功で「０」
*/
int rd_iic_s3539x::set_alarm_time(s3539x_time_data_t* p_data, int alm_no, bool week_en)
{
    char status2;
    char wd[4];

    if((NULL == p_data) || (1 > alm_no) || (2 < alm_no))
        return -S3539x_ERR_PARAM;

    //RTCへのアラーム割込みモード書き込み
    add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(1 == read_only(&status2, 1, 0, NULL))                 //Read STATUS2
    {   //読込成功 STATUS2
        if(1 == alm_no)
            status2 = (char)(status2 & 0x0E) | 0x20; //0010 0000: INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
        else
            status2 = (char)(status2 & 0xF0) | 0x02; //0000 0010: INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
        add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
        write_only(&status2, 1, 0);

        //AM, PM のセット
        if(p_data->aml_pmh)
            p_data->hour |= AMPM_BIT;               //set bit
        else
            p_data->hour &= (uint8_t)(~AMPM_BIT);   //clear bit

        //週時分のセット
        if(week_en)
            p_data->week |= (uint8_t)(0x80);        //週の設定をイネーブルにする　AxWE
        else
            p_data->week &= (uint8_t)(~0x80);       //週の設定を無効にする

        p_data->hour |= (uint8_t)(0x80);            //時の設定をイネーブルにする  AxHE
        p_data->minute |= (uint8_t)(0x80);          //分の設定をイネーブルにする  AxME

        wd[0] = (char)byte_mirror_flipping(p_data->week);
        wd[1] = (char)byte_mirror_flipping(p_data->hour);
        wd[2] = (char)byte_mirror_flipping(p_data->minute);

        //RTCへのアラーム時刻データ書き込み
        if(1 == alm_no)
            add_command_to_sa(S3539x_CMD_INT_REGISTER1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
        else
            add_command_to_sa(S3539x_CMD_INT_REGISTER2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
        write_only(wd, 3, 0);

        return S3539x_SUCCESS;
    }
    else
    {   return -S3539x_ERR_REG_READ;
    }

}

/*
 * S-35391:のセットされているアラームを読み取る。
 * p_data       : S-35391の書込む s3539x_time_data_t 構造体のポインタ
 * 　　　　　　　　利用するメンバー変数は、week/hour/minute/aml_pmh で、この変数を元にアラーム割込みを設定する
 *                注意：時刻設定で、１２時、２４時表示の設定合わせたアラーム時刻を設定する必要がある。
 * alm_no       : アラーム番号　１，２
 * 
 * 戻り値       : エラーは負の値、成功で「０」
*/
int rd_iic_s3539x::get_alarm_time(s3539x_time_data_t* p_data, int alm_no)
{
    char status2;
    char data[4];

    if((NULL == p_data) || (1 > alm_no) || (2 < alm_no))
        return -S3539x_ERR_PARAM;

    //status2 の読込
    status2 = 0;
    add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(1 == read_only(&status2, 1, 0, NULL))                 //Read STATUS2
    {   //読込成功 STATUS2
        //RTCへのアラーム割込みモード書き込み
        if(1 == alm_no)
        {   //アラーム１
            if(0x20 != (status2 & 0xF0))  //INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
            {   //アラーム１が割込みモードでないので、このモードにする
                data[0] = 0x20; //0010 0000: INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
                add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
                write_only(data, 1, 0);
            }
            data[0] = data[1] = data[2] = data[3] = 0x00;
            add_command_to_sa(S3539x_CMD_INT_REGISTER1, S3539x_CMD_MASK);   //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
            if(3 == read_only(data, 3, 0, NULL))                            //Read INT REGISTER1
            {   //読込成功
                p_data->week   = (char)byte_mirror_flipping((uint8_t)data[0]);
                p_data->hour   = (char)byte_mirror_flipping((uint8_t)data[1]);
                p_data->minute = (char)byte_mirror_flipping((uint8_t)data[2]);
            }
            else
            {   return -S3539x_ERR_REG_READ;
            }
        }
        else
        {   //アラーム２
            if(0x02 != (status2 & 0x0F))  //INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
            {   //アラーム２が割込みモードでないので、このモードにする
                data[0] = 0x02; //0000 0010: INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
                add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
                write_only(data, 1, 0);
            }
            data[0] = data[1] = data[2] = data[3] = 0x00;
            add_command_to_sa(S3539x_CMD_INT_REGISTER2, S3539x_CMD_MASK);   //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
            if(3 == read_only(data, 3, 0, NULL))                            //Read INT REGISTER2
            {   //読込成功
                p_data->week   = (char)byte_mirror_flipping((uint8_t)data[0]);
                p_data->hour   = (char)byte_mirror_flipping((uint8_t)data[1]);
                p_data->minute = (char)byte_mirror_flipping((uint8_t)data[2]);
            }
            else
            {   return -S3539x_ERR_REG_READ;
            }
        }

        //元の「status2」の値に戻す
        add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
        write_only(&status2, 1, 0);        
        return S3539x_SUCCESS;
    }
    else
    {   return -S3539x_ERR_REG_READ;
    }
}

/*
 * S-35391:のアラームをON/OFF設定、確認。
 * rw           : S-35391のアラームを設定するか、読込むか。0:read, 0以外:write
 * alm1         : アラーム番号１ true: 有効、false:無効
 * alm2         : アラーム番号２ true: 有効、false:無効
 * 
 * 戻り値       : エラーは負の値、成功で「０」
*/
int rd_iic_s3539x::alarm_control(int rl_wh, bool* alm1, bool* alm2)
{
    char status2;

    if((NULL == alm1) || (NULL == alm2))
        return -S3539x_ERR_PARAM;

    add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(1 == read_only(&status2, 1, 0, NULL))                 //Read STATUS2
    {   //読込成功 STATUS2
        //status2,  0000 0010: INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
        if(0 == rl_wh)
        {   //読込のみ
            *alm1 = (0x20 == (status2 & 0xF0))? true : false;
            *alm2 = (0x02 == (status2 & 0x0E))? true : false;
        }
        else
        {   //書込み
            if(*alm1)
               status2 = (char)((status2 & 0x0F) | 0x20);
            else
               status2 = (char)(status2 & 0x0F);
            
            if(*alm2)
               status2 = (char)((status2 & 0xF0) | 0x02);
            else
               status2 = (char)(status2 & 0xF0);

            add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
            write_only(&status2, 1, 0);
        }

        return S3539x_SUCCESS;
    }
    else
    {   return -S3539x_ERR_REG_READ;
    }

}


/*
 * S-35391:で設定したアラームが発生したかをチェックする
 * alm1         : アラーム１のフラグがコピーされます
 * alm2         : アラーム２のフラグがコピーされます
 * 戻り値       : アラーム発生で「trur」
*/
int rd_iic_s3539x::check_for_alarms(bool* alm1, bool* alm2)
{
    char data[2];

    if((NULL == alm1) || (NULL == alm2))
        return -S3539x_ERR_PARAM;

    //status1 の読込
    data[0] = data[1] = 0;
    add_command_to_sa(S3539x_CMD_STATUS1, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
    if(2 == read_only(data, 2, 0, NULL))                    //Read STATUS1, 2
    {   //読込成功

        //アラーム１
        if(0 != (data[0] & S3539x_BIT_INT1))            //check STATUS1-bit3
        {   //アラーム発生が確認された
            data[1] = (char)(data[1] & (char)(~0x21));  //clear STATUS2 INT1AE,TEST bit, INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
            add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
            write_only(&data[1], 1, 0);
            *alm1 = true;
        }
        else
        {   *alm1 = false;
        }
        
        //アラーム２
        if(0 != (data[0] & S3539x_BIT_INT2))            //check STATUS1-bit2
        {   //アラーム発生が確認された
            data[1] = (char)(data[1] & (char)(~0x03));  //clear STATUS2 INT2AE,TEST bit, INT1FE INT1ME INT1AE 32kE INT2FE INT2ME INT2AE TEST
            add_command_to_sa(S3539x_CMD_STATUS2, S3539x_CMD_MASK); //SA にコマンドが存在する場合に、SA　にコマンドを付加する。
            write_only(&data[1], 1, 0);
            *alm2 = true;
        }
        else
        {   *alm2 = false;
        }
    }
    else
    {   return -S3539x_ERR_REG_READ;
    }

    return S3539x_SUCCESS;
}

/*
 * 通常言われるＢＣＤコードを整数（uint16_t）に変換する。
 * bcd_data     : ＣＢＤデータ
 * 戻り値       : uint16_t の整数
*/
uint16_t rd_iic_s3539x::BCD_to_int(uint8_t bcd_data)
{   return (uint16_t)(((int)((bcd_data >> 4) & 0x0F) * 10) + (int)(bcd_data & 0x0F));
}


/*
 * 整数(uint16_t)を通常言われるＢＣＤコードに変換する。
 * int_data     : ＣＢＤデータ
 * 戻り値       : uint8_t のＢＣＤコード
*/
uint8_t rd_iic_s3539x::int_to_BCD(uint16_t int_data)
{   return (uint8_t)((uint8_t)(((int)int_data / 10) << 4) | (uint8_t)((uint8_t)((int)int_data % 10) & 0x0F));
}


//===== プライベート関数 ======================================================================================
/*
 * １バイトの入力データを、bit7....bit0 を　ミラー反転　bit0....bit7 として返す
 * S-35391:のリアルタイムデータは、通常言われるＢＣＤコードをビット単位でミラー反転した値である。
 * S-35391:のリアルタイムデータを入力として与えた場合、通常言われるＢＣＤコードとなる.
 * 例 : 2053年 (Y1, Y2, Y4, Y8, Y10, Y20, Y40, Y80) = (1, 1, 0, 0, 1, 0, 1, 0) 
 * in_data      : 入力データ
 * 戻り値       : ビット単位でミラー反転した値
*/
uint8_t rd_iic_s3539x::byte_mirror_flipping(uint8_t in_data)
{   uint8_t check_bit, out_data;
    int r;

    out_data = 0x00;
    check_bit = 0x01;
    for(r=7; r>=0; r--)
    {   if(in_data & check_bit)
            out_data |= (uint8_t)(1 << r);      //ビットが１の場合は、ｒ回左シフトしてＯＲ
        check_bit = (uint8_t)(check_bit << 1);  //次のチェックするビット
    }
    return (uint8_t)out_data;
}

