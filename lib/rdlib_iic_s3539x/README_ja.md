# RDLib_IIC_S3539x

I2C I/F のABLIC社製リアルタイムクロックS-35390/1を制御するライブラリーです

## 概要
このライブラリは、ArduinoでのI2Cデバイス操作によりABLIC社製リアルタイムクロックS-35390/1を制御できます  
このチップ特有の「スレーブアドレス内にコマンドを埋め込む特殊な通信方式」をライブラリ内部で自動的に処理するため  
負担のすくない操作で時刻管理が可能です

## 特徴
- **シンプル**: APIの構成は、初期化、時刻やアラームのセット・取得、レジスタの読書きなどの単純な構成としております
- **汎用性**: 標準的なI2C通信を用いるあらゆるデバイスで利用可能
- **その他**: このライブラリーの他に「RDLib_IIC_RW_Base」ライブラリーが必要です

## インストール方法
1. GitHubからこのリポジトリをZIP形式でダウンロードします。
2. Arduino IDEを開き、「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール...」を選択します。
3. ダウンロードしたZIPファイルを選択してください。

## 使い方
基本的な使い方は以下の通りですが、ネット－ワークへの接続等は省略しています

```cpp
#include "rd_iic_s3539x.h"

rd_iic_s3539x RTC(IIC_DEV_CODE_S35390);   //RTC[S35390A]
struct tm* p_timeInfo;
bool f_rtc_alarm[2] = {false, false};
char ampm[4];

void setup() {
  //--- シリアル
  Serial.begin(115200);

  //--- S35390A
  //初期化時に、過去にアラームが発生しセットされているアラームがあるか確認している
  //アラームフラグを読込むので、RTCのアラームフラグは「クリアされる」
  if(0 == RTC.initialization(&f_rtc_alarm[0], &f_rtc_alarm[1]))
  {   //成功
    Serial.printf_P(PSTR("[i] S-35390A initialization ALM1=%d, ALM2=%d\r\n"), (int)f_rtc_alarm[0], (int)f_rtc_alarm[1]);

    time_t epoch = NTP_client->getEpochTime(); 
    p_timeInfo = localtime(&epoch);

    RTC.s3539x_times.hour24 = true;
    if((0 <= p_timeInfo->tm_hour) && (12 > p_timeInfo->tm_hour))
      RTC.s3539x_times.aml_pmh = false;
    else
      RTC.s3539x_times.aml_pmh = true;

    RTC.s3539x_times.week   = RTC.int_to_BCD((uint16_t)p_timeInfo->tm_wday);
    RTC.s3539x_times.year   = RTC.int_to_BCD((uint16_t)(p_timeInfo->tm_year - 100)); //+1900-2000=-100
    RTC.s3539x_times.month  = RTC.int_to_BCD((uint16_t)(p_timeInfo->tm_mon + 1));
    RTC.s3539x_times.date   = RTC.int_to_BCD((uint16_t)p_timeInfo->tm_mday);
    RTC.s3539x_times.hour   = RTC.int_to_BCD((uint16_t)p_timeInfo->tm_hour);
    RTC.s3539x_times.minute = RTC.int_to_BCD((uint16_t)p_timeInfo->tm_min);
    RTC.s3539x_times.second = RTC.int_to_BCD((uint16_t)p_timeInfo->tm_sec);

    if(S3539x_SUCCESS == RTC.set_time(&RTC.s3539x_times))
    { if(RTC.s3539x_times.aml_pmh) strcpy(ampm, "PM"); else strcpy(ampm, "AM");
      Serial.printf_P(PSTR("[s] Set time > %d-%02d-%02d(%s) %s %02d:%02d:%02d\r\n\r\n"),
          RTC.s3539x_times.year_four_digit,
          (int)RTC.BCD_to_int(RTC.s3539x_times.month),
          (int)RTC.BCD_to_int(RTC.s3539x_times.date),
          (char*)(&RTC.weeks[RTC.s3539x_times.week][0]),
          ampm,
          (int)RTC.BCD_to_int((uint8_t)(RTC.s3539x_times.hour & (uint8_t)(~AMPM_BIT))),
          (int)RTC.BCD_to_int(RTC.s3539x_times.minute),
          (int)RTC.BCD_to_int(RTC.s3539x_times.second) 
      );
    }
    else
    { Serial.printf_P(PSTR("[e] Set time Command, exec error!!\r\n\r\n"));
    }
  }
  else
  { Serial.printf_P(PSTR("[e] read err!! S-35390A\r\n"));
  }  
}

void loop() {
  int ret = RTC.get_time(&RTC.s3539x_times);
  if(S3539x_SUCCESS == ret)
  { //成功
    Serial.printf_P(PSTR("[i] S-35390A Time [%s/%s] %d/%02X/%02X(%s) %02X:%02X:%02X\r\n"),
    (RTC.s3539x_times.hour24)? "24h" : "12h", (RTC.s3539x_times.aml_pmh)? "PM" : "AM",
    RTC.s3539x_times.year_four_digit, RTC.s3539x_times.month, RTC.s3539x_times.date, (char*)(&RTC.weeks[RTC.s3539x_times.week][0]),
    (uint8_t)(RTC.s3539x_times.hour&0x3F), RTC.s3539x_times.minute, RTC.s3539x_times.second);        
  }
  else
  { //失敗
    Serial.printf_P(PSTR("[e] S-35390A Get Time error(%d)!\r\n"), ret);
  }
  delay(1000);
}
