# RDLib_IIC_SmallLcds

I2C I/F のAQMシリーズLCDを制御するライブラリーです

## 概要
このライブラリは、ArduinoでのI2Cデバイス操作によりAQMシリーズLCD(2x8/2x16)を制御できます  
使用する電圧を指定(3.3V/5.0V)することでそれぞれ対応した初期値で初期化されます

## 特徴
- **シンプル**: APIの構成は、初期化、クリア、アップデートなどの単純な構成としております
- **汎用性**: 標準的なI2C通信を用いるあらゆるデバイスで利用可能
- **その他**: このライブラリーの他に「RDLib_IIC_RW_Base」ライブラリーが必要です

## インストール方法
1. GitHubからこのリポジトリをZIP形式でダウンロードします。
2. Arduino IDEを開き、「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール...」を選択します。
3. ダウンロードしたZIPファイルを選択してください。

## 使い方
基本的な使い方は以下の通りです。

```cpp
#include "rd_small_lcds.h"

rd_iic_lcds AQM1602(IIC_ADDR_AQM1602, DEVICE_IS_5V0, LCD_TYPE_AQM1602);

void setup() {

  //--- LCD AQM1602(5V駆動)
  AQM1602.initialize();
  memcpy(AQM1602.lcd_data_buff[LCD_BUF_LINE1], ">>>> Rinwado ...", 16);
  memcpy(AQM1602.lcd_data_buff[LCD_BUF_LINE2], "RRH-G101A IoT-IC", 16);
  delay(300);
  AQM1602.display_update(AQM1602.lcd_data_buff[LCD_BUF_LINE1], 16, 0, AQM1602.lcd_data_buff[LCD_BUF_LINE2], 16, 0, LCD_NO_CLR, LINE_ALL);
}

void loop() {
}

