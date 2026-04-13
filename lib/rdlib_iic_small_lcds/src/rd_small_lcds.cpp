/**
 * 小規模ＬＣＤライブラリ
 * 対応ＬＣＤ：AQM0802, AQM1602
 * 
 * 2024-11-12  Ver 0.1.0
 * rd_small_lcds.c
 * 
 * 2024-11-12
 * Copyright (c) 2024 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

//----------------------------------------------------------------------------------------------------------
#include "rd_small_lcds.h"

//----------------------------------------------------------------------------------------------------------
#define LCD_CMD                     	(0x00)
#define LCD_DATA                    	(0x40)
#define SPACE_CHAR						(0x20)

//===== パブリック関数 ======================================================================================
/**
 * @brief Construct a new rd iic lcds::rd iic lcds object
 * 
 * @param addr		:LCDデバイスのスレーブアドレス
 * @param dv		:デバイスの使用電圧（3.3V or 5.0V）
 * @param lt		:LCDタイプ（AQM0802, AQM1602）
 */
rd_iic_lcds::rd_iic_lcds(uint8_t addr, int dv, int lt) : rd_iic_base(addr)	//基底クラスのコンストラクタ名を指定して()の中に引数を指定します。そうするとその引数の内容に当てはまるコンストラクタが呼び出されます。
{	_dev_volt = dv;
	_lcd_type = lt;
	if(LCD_TYPE_AQM0802 == _lcd_type)
		_lcd_line_columns = LINE_COLUMN08;
	else
	if(LCD_TYPE_AQM1602 == _lcd_type)
		_lcd_line_columns = LINE_COLUMN16;
}

/**
 * @brief LCDの初期化（LCDにクリアコマンドを発行し、LCD用の表示バッファーをスペース文字でで埋めます）
 * 
 */
void rd_iic_lcds::initialize()
{	char _d;

	_d = 0x38;	//Function set IS=0
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x39;	//Function set IS=1
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x14;	//Internal OSC Frequency
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x73;	//Contrast set
	if(DEVICE_IS_5V0 == _dev_volt) _d = 0x7A;
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x56;	//Power, ICON, Contrast
	if(DEVICE_IS_5V0 == _dev_volt) _d = 0x50;
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x6C;	//Foll0wer control
	_write(LCD_CMD, &_d, 1); delay(200);
	_d = 0x38;	//Function set IS=0
	_write(LCD_CMD, &_d, 1); delay(2);
	_d = 0x01;	//Clear Display
	_write(LCD_CMD, &_d, 1); delay(20);
	_d = 0x0C;	//Display ON-OFF Control
	_write(LCD_CMD, &_d, 1); delay(2);
	
	memset(lcd_data_buff, SPACE_CHAR, sizeof(lcd_data_buff));	//スペース文字で埋める
}


/**
 * @brief LCD表示のアップデート
 *        ２ラインに対して、バッファーの更新、クリアの有無とＬＣＤの表示の更新を行う
 * 
 * @param p_str1	:１行目、置き換える文字列のポインタ、NULLの場合は、バッファーの更新はなく、クリアの指示があればそれを行う
 * @param len1		:１行目、更新する文字の数
 * @param spos1		:１行目、更新を開始する先頭の列、１列目は「０」を指定、ｎ列目は「n-1」
 * @param p_str2	:２行目、置き換える文字列のポインタ、NULLの場合は、バッファーの更新はなく、クリアの指示があればそれを行う
 * @param len2		:２行目、更新する文字の数
 * @param spos2		:２行目、更新を開始する先頭の列、１列目は「０」を指定、ｎ列目は「n-1」
 * @param f_clear	:バッファーの行をクリアしてから、操作を行う場合に指定。
 *                   [0] バッファーをクリアしないで文字列を更新
 *                   [1] １行目をクリアしてから文字列を更新、[2] ２行目をクリアしてから文字列を更新、[3] 全てをクリアしてから文字列を更新
 * @param f_update	:ＬＣＤの表示を実際に更新するか否か
 *                   [0] ＬＣＤ更新しない。　更新はバッファーのみとなる
 *                   [1] ＬＣＤの１行目のみを更新、[2] ＬＣＤの２行目のみを更新、[3] ＬＣＤの全てを更新
 */
void rd_iic_lcds::display_update(const char* p_str1, uint8_t len1, uint8_t spos1, const char* p_str2, uint8_t len2, uint8_t spos2, uint8_t f_clear, uint8_t f_update)
{
	char cmd;
	int	r, pos;

	//バッファーの行をクリアしてから、操作を行う場合
	switch(f_clear)
	{	case LINE1:		//Line1
			memset(&lcd_data_buff[0][0], SPACE_CHAR, (size_t)_lcd_line_columns);	//スペース文字で埋める
		break;
		case LINE2:		//Line2
			memset(&lcd_data_buff[1][0], SPACE_CHAR, (size_t)_lcd_line_columns);	//スペース文字で埋める
		break;
		case LINE_ALL:	//Line ALL
			memset(lcd_data_buff, SPACE_CHAR, sizeof(lcd_data_buff));				//スペース文字で埋める
		break;
		default:		//バッファーのクリアしない
		break;
	}
	//１行目のバッファー変更
	if((NULL != p_str1) && (0 != len1))
	{	//NULLポインタでなく、長さが０でない場合、バッファー文字の更新
		for(r=0; r<len1; r++)
		{	pos = r + spos1;
			if(_lcd_line_columns <= pos) break;
			lcd_data_buff[0][pos] = *p_str1;
			p_str1++;
		}
	}
	//２行目のバッファー変更
	if((NULL != p_str2) && (0 != len2))
	{	//NULLポインタでなく、長さが０でない場合、バッファー文字の更新
		for(r=0; r<len2; r++)
		{	pos = r + spos2;
			if(_lcd_line_columns <= pos) break;
			lcd_data_buff[1][pos] = *p_str2;
			p_str2++;
		}
	}

	//ＬＣＤ内容の更新
	if(LINE1 == f_update)
	{	//ライン１のアップデート
		//LCDへ書き出し
		cmd = 0x80;	//書込み開始位置: line1 0x80
		_write(LCD_CMD, &cmd, 1);
		_write(LCD_DATA, &lcd_data_buff[0][0], (size_t)_lcd_line_columns);		//1line 文字分
	} else
	if(LINE2 == f_update)
	{	//ライン２のアップデート
		//LCDへ書き出し
		cmd = 0xC0;	//書込み開始位置: line2 0xC0
		_write(LCD_CMD, &cmd, 1);
		_write(LCD_DATA, &lcd_data_buff[1][0], (size_t)_lcd_line_columns);		//1line 文字分
	} else
	if(LINE_ALL == f_update)
	{	//ライン２のアップデート
		//LCDへ書き出し
		for(r=0; r<LCD_NOF_LINE; r++)
		{   if(0 == r) cmd = 0x80; else cmd = 0xC0;	//書込み開始位置: line1 0x80 , line2 0xC0
			_write(LCD_CMD, &cmd, 1);
			_write(LCD_DATA, &lcd_data_buff[r][0], (size_t)_lcd_line_columns);	//1line 文字分
		}
	}
}

/**
 * @brief LCD全クリア（クリアコマンドを送り、LCD用の表示バッファーをスペース文字で埋めます
 * 
 */
void rd_iic_lcds::all_clear(void) 
{	char cmd;

    cmd = 0x01;	//Clear command
	_write(LCD_CMD, &cmd, 1);
	memset(lcd_data_buff, SPACE_CHAR, sizeof(lcd_data_buff));	//スペース文字で埋める
	delay(20);
}

/**
 * @brief LCDにクリアコマンドを発行した後に、表示バッファーの内容をLCDに反映します	
 * 
 */
void rd_iic_lcds::update_2line(void)
{	char cmd;

	//LCDクリア
    cmd = 0x01; //Clear command
	_write(LCD_CMD, &cmd, 1);
	delay(20);

	//LCDへ書き出し
	for(int i=0; i<LCD_NOF_LINE; i++)
	{   if(0 == i) cmd = 0x80; else cmd = 0xC0;             		//書込み開始位置: line1 0x80 , line2 0xC0
		_write(LCD_CMD, &cmd, 1);
		_write(LCD_DATA, &lcd_data_buff[i][0], _lcd_line_columns);	//1line 文字分
	}
}

/**
 * @brief LCD表示バッファーを行単位でスペース文字で埋めます
 * 
 * @param line_no	:操作するライン番号[LINE1][LINE2][LINE_ALL] これら以外は何もしない。
 */
void rd_iic_lcds::LineBuff_Clear(int8_t line_no)
{   switch(line_no)
    {   case LINE1: //Line1
			memset(&lcd_data_buff[0][0], SPACE_CHAR, _lcd_line_columns);	//スペース文字で埋める
            break;
        case LINE2: //Line2
			memset(&lcd_data_buff[1][0], SPACE_CHAR, _lcd_line_columns);	//スペース文字で埋める
            break;
        case LINE_ALL: //ALL line clear
			memset(lcd_data_buff, SPACE_CHAR, sizeof(lcd_data_buff));		//スペース文字で埋める
            break;
        default: //なにもしない
            break;
    }
}


//===== プライベート関数 ======================================================================================
/**
 * @brief LCDにコマンド、データを書込み操作
 * 
 * @param type		:コマンド[LCD_CMD] or データ[LCD_DATA]
 * @param data		:書き込むデータのポインタ
 * @param len		:書き込むデータの数
 */
void rd_iic_lcds::_write(char type, char* data, size_t len)
{	char wd[2];
	for(size_t r=0; r<len; r++)
	{	wd[0] = type;
		wd[1] = data[r];
		write_only(wd, 2, 100);
	}
}
