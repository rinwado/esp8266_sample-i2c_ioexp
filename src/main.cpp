/**
 * タスクスケジューラーとティッカー ライブラリーを使ったＩＩＣデバイスアクセスのサンプルプログラムです。
 * Ｗｉ－Ｆｉ接続、リレー、ＲＴＣ、ＡＤＳ１１１５、外付けＡＱＭシリーズＬＣＤの制御の例があります。
 * for PlatformIO
 * 
 * 対応ボード：IOT Integrated Controller V1
 *            RRH-G101A REV-B
 * ボード設定：
 * ボードのＪＰ１を「ＬＥＤ」側にジャンパー、ＪＰ５を「ＯＮ」に設定
 * 半田ジャンパーＪＰ３、ＪＰ４はＯＦＦ（INA826 G=1）、ＪＰ６はＯＦＦ
 * 
 * 2026-04-20
 * Copyright (c) 2026 rinwado
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license text.
 */

//#include <new>
#include <Arduino.h>
#include <Ticker.h>
#include <TaskScheduler.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <Wire.h>
#include <TCA9534-GPIO.h>
#include "rd_iic_s3539x.h"
#include "net_env.h"
#include "ADS1X15.h"
#include "rd_small_lcds.h"


/// 定義
#define IIC_SCL_PIN             (5)           //IIC SCL Port
#define IIC_SDA_PIN             (4)           //IIC SCL Port
#define LED_LD4                 (16)          //IO port 16 に接続されている　ＬＥＤ
#define IO0_PIN                 (0)           //プログラムボタンと兼用[SW2/IN1]しているので、起動時の検出はできない
#define IO2_PIN                 (2)           //IN2
#define BUZZER_PIN              (13)          //ブザー制御出力　１でブザーＯＮ、０でブザーＯＦＦ
#define SW2_ISOIN1              (0x01)        //0000_0001
#define ISOIN2                  (0x02)        //0000_0010
#define TCA9534_SA              (0x20)        //TCA9534 のスレーブアドレス
#define NOF_EXGPIO              (8)           //TCA9534 のGPIOの数
#define EXIOP_OUT               (false)       //TCA9534 ピンモード出力
#define EXIOP_IN                (true)        //TCA9534 ピンモード入力
#define RL1_DRV                 (0)
#define RL2_DRV                 (1)
#define RL3_DRV                 (2)
#define RL4_DRV                 (3)
#define RL1LED_DRV              (4)
#define RL2LED_DRV              (5)
#define RL3LED_DRV              (6)
#define RL4LED_DRV              (7)
#define SET_ON                  (true)
#define SET_OFF                 (false)

#define NOF_TICK_CNT(ms)        (ms / 3.333)  //指定したｍｓ時間が Ticker 割込みでのカウントがいくつに相当するか
#define JST_OFFSET              (9 * 3600)    //9時間 × 3600秒
#define NTP_SERVER              "ntp.nict.jp"

#define WIFI_FIXED_STR_LEN_MAX  (48)          //NULLターミネータ含めた文字数

//ネットワーク系デフォルト値
#define DEF_WIFI_AP_ID			  "def_wi-fi"
#define DEF_WIFI_AP_TK			  "def_password"
#define DEF_FIX_IP            "192.168.0.200"
#define DEF_FIX_GW            "192.168.0.254"
#define DEF_FIX_SNM           "255.255.255.0"
#define DEF_FIX_DNS           "192.168.0.254"

enum wState
{	//Wi-Fi ステート
	WIFI_CSTART = 0,
	WIFI_CLIENT_PROC_ENT,
	WIFI_CONNECT_CHK,
	WIFI_TRY_CONNECT_TO_AP,
	WIFI_CONNECT_OKNG,

  WIFI_WAIT_PROCESS,
	WIFI_OHTER_PROCESS,
};

typedef struct wifi_setting_val
{
    bool f_dhcp;
    //bool f_secure;
    char ConnectSSID[WIFI_FIXED_STR_LEN_MAX];
    char ConnectSSID_Pass[WIFI_FIXED_STR_LEN_MAX];
    IPAddress wfixIP;
    IPAddress wfixGW;
    IPAddress wfixSNM;
    IPAddress wfixDNS;
    char WiFi_mac[8];
    char WiFi_mac_str[32];
} wifi_setting_val_t;


/// プロトタイプ宣言
void IRAM_ATTR onTickTimerISR(void);
void WiFiProc_Callback(void);
void MainWork_Callback(void);
void TskOneShot_ProcCallback(void);

/// オブジェクト生成
TCA9534 exp_gpio;                         //拡張GPIO
rd_iic_s3539x RTC(IIC_DEV_CODE_S35390);   //RTC[S35390A]
ADS1115 ADS(ADS1115_ADDRESS);
rd_iic_lcds AQM1602(IIC_ADDR_AQM1602, DEVICE_IS_5V0, LCD_TYPE_AQM1602);
//rd_iic_lcds AQM1602(IIC_ADDR_AQM0802, DEVICE_IS_3V3, LCD_TYPE_AQM0802);
Ticker tick_timer1;                       //ティッカー割り込み
Scheduler TskRunner;                      //スケジューラ
//Taskの宣言した順にタスク管理リストに登録され、管理リストの順に実行タイミングがチェックされる。
//Task(周期ms, 実行回数[-1は無限], 実行する関数, スケジューラへのポインタ)
Task tsk_wifi_proc(1, TASK_FOREVER, &WiFiProc_Callback, &TskRunner);                      //[wifi_proc]タスクの作成
Task tsk_main_work(1, TASK_FOREVER, &MainWork_Callback, &TskRunner);                      //[main_work]タスクの作成
Task tsk_OneShot_01(TASK_IMMEDIATE, TASK_ONCE, &TskOneShot_ProcCallback, &TskRunner);     //[Proc]OneShotタスクの作成

/// 変数
volatile uint16_t gn_cnt1 = 0;            //汎用カウンタ１
volatile uint16_t gn_cnt2 = 0;            //汎用カウンタ２
volatile uint8_t f_FE_SignalDetect, f_RE_SignalDetect;
volatile bool f_counter_trigger = false;  //割込みカウンタによるトリガフラグ

bool tca9534_PinMode[NOF_EXGPIO] = {EXIOP_OUT, EXIOP_OUT, EXIOP_OUT, EXIOP_OUT, EXIOP_OUT, EXIOP_OUT, EXIOP_OUT, EXIOP_OUT};  //配列[0]:P1..配列[7]:P8, TCA9534 全出力
bool tca9534_ioport[NOF_EXGPIO] = {SET_OFF, SET_OFF, SET_OFF, SET_OFF, SET_OFF, SET_OFF, SET_OFF, SET_OFF};                   //配列[0]:P1..配列[7]:P8, TCA9534 初期レベル HIGH, LOW
bool f_tca9534_available = false;
bool f_ads_available = false;
uint8_t act_mode = 0;                     //サンプルの動作モード

WiFiClient* WIFI_Client;
WiFiUDP* ntpUDP_Client;
NTPClient* NTP_client;
bool f_rtc_alarm[2] = {false, false};
bool f_rtc_access = false;
bool f_wifi_connected = false;
enum wState WiFi_state;
enum wState WiFi_next_state;
wifi_setting_val_t wifi_setting;

bool f_RLON_SequenceInProgress = false;
bool f_RLOF_SequenceInProgress = false;
bool f_NowPowerON = false;
uint16_t RL_DelayTimeCNT1 = 0;
uint16_t RL_DelayTimeCNT2 = 0;

int16_t adc_1st_data[4] = {0};
int16_t wait_time;
volatile int16_t WiFi_wait_counter;
volatile int16_t WiFi_ConnectTimeOut_Counter;
char wifi_macAddress[16];

struct tm* p_timeInfo;



/**
 * @brief Arduino setup
 * 
 */
void setup()
{
  //--- IOピン出力設定
  digitalWrite(BUZZER_PIN, LOW);  //BuzzerをOFF
  digitalWrite(LED_LD4, HIGH);    //LED-LD4をON
  //--- IOピン設定
  pinMode(BUZZER_PIN, OUTPUT);    //リセットからこのコードが実行されるまでの間ブザーが鳴ります。実行されればブザーＯＦＦ）
  pinMode(LED_LD4, OUTPUT);
  pinMode(IO0_PIN, INPUT);
  pinMode(IO2_PIN, INPUT);

  //--- シリアル
  Serial.begin(115200);
  Serial.printf_P(PSTR("\r\n[i] Flash Real Size: %u bytes\r\n"), ESP.getFlashChipRealSize()); //ESP8266に搭載されているフラッシュメモリ容量

  //--- IIC
  Wire.begin(IIC_SDA_PIN, IIC_SCL_PIN);
  Wire.setClock(100000);  //Speed 100k

  //--- TCA9534
  if(exp_gpio.begin(Wire, TCA9534_SA))
  { Serial.printf_P(PSTR("[i] Extended GPIO is available.\r\n"));
    //常時ＯＮリレー/ＬＥＤをＯＮにする
    tca9534_ioport[RL4_DRV] = SET_ON;                 //リレー４は常時ＯＮ
    tca9534_ioport[RL4LED_DRV] = SET_ON;      
    exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
    exp_gpio.pinMode(tca9534_PinMode);                //ポート入出力設定
    f_tca9534_available = true;
  }
  else
  { Serial.printf_P(PSTR("[i] Err: Extended GPIO cannot be used!\r\n"));
    f_tca9534_available = false;
  }

  //--- S35390A
  //初期化時に、過去にアラームが発生しセットされているアラームがあるか確認している
  //ここでアラームフラグを読込むので、RTCのアラームフラグは「クリアされる」
  //つまり、Wi-Fi接続されずに、電源が切れた場合や、NTPサーバーへの接続ができなかった場合は、NTPサーバー同期がされない。
  if(0 == RTC.initialization(&f_rtc_alarm[0], &f_rtc_alarm[1]))
  { Serial.printf_P(PSTR("[i] S-35390A initialization ALM1=%d, ALM2=%d\r\n"), (int)f_rtc_alarm[0], (int)f_rtc_alarm[1]);
    f_rtc_access = true;
  }
  else
  { Serial.printf_P(PSTR("[e] read err!! S-35390A data\r\n"));
    f_rtc_access = false;
  }

  //--- ADS1115
  if(ADS.begin())
  { Serial.printf_P(PSTR("ADS1x15 is available.\r\n"));
    f_ads_available = true;
  }
  else
  { Serial.printf_P(PSTR("ADS1x15 is not available.\r\n"));
    f_ads_available = false;
  }
  
  //--- LCD AQM1602(5V駆動)
  AQM1602.initialize();
  memcpy(AQM1602.lcd_data_buff[LCD_BUF_LINE1], ">>>> Rinwado ...", 16);
  memcpy(AQM1602.lcd_data_buff[LCD_BUF_LINE2], "RRH-G101A IoT-IC", 16);
  delay(300); //初期化後ＬＣＤがクリアされたのを確認する為の遅延
  AQM1602.display_update(AQM1602.lcd_data_buff[LCD_BUF_LINE1], 16, 0, AQM1602.lcd_data_buff[LCD_BUF_LINE2], 16, 0, LCD_NO_CLR, LINE_ALL);

  //---
  act_mode = 0;
  WiFi_state = WIFI_CSTART;
  WiFi_next_state = WIFI_CSTART;
  //---
  WIFI_Client = NULL;
  ntpUDP_Client = NULL;
  NTP_client = NULL;

  //Wi-Fi パラメータ初期化
  f_wifi_connected = false;
  wifi_setting.f_dhcp = true;
  memset(wifi_setting.ConnectSSID, 0, sizeof(wifi_setting.ConnectSSID));
  strcpy(wifi_setting.ConnectSSID, WIFI_SSID);
  memset(wifi_setting.ConnectSSID_Pass, 0, sizeof(wifi_setting.ConnectSSID_Pass));
  strcpy(wifi_setting.ConnectSSID_Pass, WIFI_VERI_WORD);
  wifi_setting.wfixIP.fromString(WIFI_FIX_IP);    //IPAddress のメンバ関数[fromString]でストリングからIPアドレスに変換
  wifi_setting.wfixGW.fromString(WIFI_FIX_GW);
  wifi_setting.wfixSNM.fromString(WIFI_FIX_SNM);
  wifi_setting.wfixDNS.fromString(WIFI_FIX_DNS);

  //--- ティッカーの開始 秒数で指定(0.003333 = 3.333ms, onTimerISR関数をセット)
  tick_timer1.attach(0.003333, onTickTimerISR);
  //--- タスクを有効化
  tsk_wifi_proc.enable();
  tsk_main_work.enable();
  //tsk_OneShot_01.enable();  //TASK_ONCE　なので、ここではenableにしない
}



/**
 * @brief Arduino loop
 * 
 */
void loop()
{
  //--- スケジューラを回す
  TskRunner.execute();
}



/**
 * @brief TaskScheduler コールバック関数
 *        １ｍｓ 間隔で実行される
 *        Wi-Fi 処理
 */
void WiFiProc_Callback(void)
{
  static uint32_t now_free_heap_size = 0;

  switch(WiFi_state)
  { //Wi-Fi 接続・切断・再接続の処理ステート
    case WIFI_CSTART:
      f_wifi_connected = false;
      now_free_heap_size = ESP.getFreeHeap();
      Serial.printf_P(PSTR("[i] Free Heap  size: %d\r\n"), (int)now_free_heap_size);
      Serial.printf_P(PSTR("[i] WiFiClient size: %d\r\n"), (int)sizeof(WiFiClient));
      Serial.printf_P(PSTR("[i] WiFiUDP    size: %d\r\n"), (int)sizeof(WiFiUDP));
      Serial.printf_P(PSTR("[i] NTPClient  size: %d\r\n"), (int)sizeof(NTPClient));

      if(8192 < now_free_heap_size)
      { //Wi-Fi 関係で 8192byte は、通信バッハーなどでこの先利用するだろうと予測して数値を設定している
        if(NULL == WIFI_Client)
        { WIFI_Client = new WiFiClient();
          if(NULL != WIFI_Client)
          { //OK
              WiFi.mode(WIFI_STA);
              Serial.printf_P(PSTR("[i] WiFi-Mode:STA\r\n"));
          }
        }

        if(NULL == ntpUDP_Client)
        { ntpUDP_Client = new WiFiUDP();
        }

        if((NULL == NTP_client) && (NULL != ntpUDP_Client))
        { NTP_client = new NTPClient(*ntpUDP_Client, NTP_SERVER, JST_OFFSET);
        }

        if((NULL == WIFI_Client) || (NULL == ntpUDP_Client) || (NULL == NTP_client))
        { //NG
          Serial.printf_P(PSTR("[e] Failed to create the instance. Will retry in 1 second.\r\n"));
          wait_time = NOF_TICK_CNT(1000); //1000ms
          WiFi_wait_counter = 0;
          WiFi_state = WIFI_WAIT_PROCESS;
          WiFi_next_state = WIFI_CSTART;
        }
        else
        { //OK
          now_free_heap_size = ESP.getFreeHeap();
          Serial.printf_P(PSTR("[i] Free Heap  size: %d\r\n"), (int)now_free_heap_size);            
          WiFi_state = WIFI_CLIENT_PROC_ENT;
          WiFi_next_state = WIFI_CLIENT_PROC_ENT;
        }
      }
      else
      { Serial.printf_P(PSTR("[?] There may not be enough heap memory. Will retry in 1 second.\r\n"));
        wait_time = NOF_TICK_CNT(1000); //1000ms
        WiFi_wait_counter = 0;
        WiFi_state = WIFI_WAIT_PROCESS;
        WiFi_next_state = WIFI_CSTART;
      }
    break;

    case WIFI_CLIENT_PROC_ENT:
      WiFi.disconnect(true);        //WiFi.disconnect(true, true); wifioff	trueを指定すると、ステーションモードを終了する。省略時はfalse。 eraseap	trueを指定すると、WiFiの設定情報を削除する。省略時はfalse。
      WiFi.setAutoConnect(false);		//電源再投入時に最後に接続されたAPに自動的に接続するか否か：自動接続しない
      WiFi.setAutoReconnect(false); //APの接続が切れた場合、自動的に再接続するか否か：自動接続しない（既に接続が切れている時に実行してもAPへの再接続はされない）

      if(WiFi.status() != WL_CONNECTED)
      {	//接続が切れているのを確認
        f_wifi_connected = false;
        WiFi_state = WIFI_TRY_CONNECT_TO_AP;
        WiFi_next_state = WIFI_TRY_CONNECT_TO_AP;
      }
      else
      { wait_time = NOF_TICK_CNT(50); //50ms
        WiFi_wait_counter = 0;
        WiFi_state = WIFI_WAIT_PROCESS;
        WiFi_next_state = WIFI_CLIENT_PROC_ENT;
      }
    break;     

    case WIFI_TRY_CONNECT_TO_AP:
        //ルーター（ＡＰ）への接続（WiFi.begin：デフォルトはDHCP）
        Serial.printf_P(PSTR("[i] WiFi Try connect.\r\n"));
        if(!wifi_setting.f_dhcp)
        { //DHCPでない場合
            WiFi.config(wifi_setting.wfixIP, wifi_setting.wfixGW, wifi_setting.wfixSNM, wifi_setting.wfixDNS);
        }
        WiFi.begin(wifi_setting.ConnectSSID, wifi_setting.ConnectSSID_Pass);

        WiFi_ConnectTimeOut_Counter = NOF_TICK_CNT(30000); //接続タイムアウト時間 30s セット
        WiFi_state = WIFI_CONNECT_OKNG;
        WiFi_next_state = WIFI_CONNECT_OKNG;

        now_free_heap_size = ESP.getFreeHeap();
        Serial.printf_P(PSTR("[i] Free Heap  size: %d\r\n"), (int)now_free_heap_size);   
    break;

    case WIFI_CONNECT_OKNG:
      if(WiFi.status() == WL_CONNECTED)
      {	//接続された
          WiFi.macAddress((uint8_t*)wifi_setting.WiFi_mac);
          memset(wifi_macAddress, 0, sizeof(wifi_macAddress));
          sprintf(wifi_macAddress, "%02X%02X%02X%02X%02X%02X",
                                              wifi_setting.WiFi_mac[5], wifi_setting.WiFi_mac[4], wifi_setting.WiFi_mac[3],
                                              wifi_setting.WiFi_mac[2], wifi_setting.WiFi_mac[1], wifi_setting.WiFi_mac[0]);
          sprintf(wifi_setting.WiFi_mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                                              wifi_setting.WiFi_mac[5], wifi_setting.WiFi_mac[4], wifi_setting.WiFi_mac[3],
                                              wifi_setting.WiFi_mac[2], wifi_setting.WiFi_mac[1], wifi_setting.WiFi_mac[0]);
          Serial.printf_P(PSTR("\r\n[i] WiFi connected.\r\n"));
          Serial.printf_P(PSTR("[i] MAC address    : %s\r\n"), wifi_setting.WiFi_mac_str);
          Serial.printf_P(PSTR("[i] IP address     : %s\r\n"), WiFi.localIP().toString().c_str());
          Serial.printf_P(PSTR("[i] Default Gateway: %s\r\n"), WiFi.gatewayIP().toString().c_str());
          Serial.printf_P(PSTR("[i] Subnetmask     : %s\r\n"), WiFi.subnetMask().toString().c_str());
          Serial.printf_P(PSTR("[i] DNS Server1    : %s\r\n"), WiFi.dnsIP(0).toString().c_str());
          Serial.printf_P(PSTR("[i] DNS Server2    : %s\r\n"), WiFi.dnsIP(1).toString().c_str());

          WiFi.setAutoConnect(false);		//電源再投入時に最後に接続されたAPに自動的に接続するか否か：自動接続しない
          WiFi.setAutoReconnect(false); //APの接続が切れた場合、自動的に再接続するか否か：自動接続しない（既に接続が切れている時に実行してもAPへの再接続はされない）

          NTP_client->begin();  //NTP Client 開始

          WiFi_state = WIFI_CONNECT_CHK;
          WiFi_next_state = WIFI_CONNECT_CHK;
          f_wifi_connected = true;
      }
      else
      {	//接続タイムアウトチェック
        if(0 >= WiFi_ConnectTimeOut_Counter)
        {   //タイムアウト
          Serial.printf_P(PSTR("[i] WiFi connect timeout(30s)!\r\n"));
          wait_time = NOF_TICK_CNT(200); //200ms
          WiFi_wait_counter = 0;

          WiFi_state = WIFI_WAIT_PROCESS;
          WiFi_next_state = WIFI_CLIENT_PROC_ENT;
          f_wifi_connected = false;
        }
      }        
    break;

    case WIFI_CONNECT_CHK:
      if(WiFi.status() != WL_CONNECTED)
      {	//接続が切れた
        Serial.printf_P(PSTR("[i] Now Wi-Fi Disconnected!\r\n"));
        NTP_client->end();              //NTP Client を終了
        WIFI_Client->stop();            //TCP通信停止

        wait_time = NOF_TICK_CNT(1000); //1000ms
        WiFi_wait_counter = 0;

        WiFi_state = WIFI_WAIT_PROCESS;
        WiFi_next_state = WIFI_CLIENT_PROC_ENT;
        f_wifi_connected = false;

        now_free_heap_size = ESP.getFreeHeap();
        Serial.printf_P(PSTR("[i] Free Heap  size: %d\r\n"), (int)now_free_heap_size);   
      }
    break;

    case WIFI_WAIT_PROCESS:
      if(wait_time <= WiFi_wait_counter)
          WiFi_state = WiFi_next_state;
    break;     

    default:
    break;
  } //switch(WiFi_state)
}



/**
 * @brief TaskScheduler コールバック関数
 *        １ｍｓ 間隔で実行される
 *        アプリメイン処理
 */
void MainWork_Callback(void)
{
  if(f_counter_trigger)
  { //１秒ごとの処理があれば、ここの記述
    f_counter_trigger = false;
  }

  //ＳＷ２ボタンフラグ確認（押下）
  if(0 != (f_FE_SignalDetect & SW2_ISOIN1))
  { //ＳＷ２が押下された
    Serial.printf_P(PSTR("[D] TASK: MainWork Proc (SW2[ISO IN1] ON...)\r\n"));
    digitalWrite(BUZZER_PIN, HIGH); //BuzzerをON

    f_FE_SignalDetect &= ~SW2_ISOIN1;   //SW2_ISOIN1 フラグクリア
  }
  //ＳＷ２ボタンフラグ確認（離上）：動作っモード設定
  if(0 != (f_RE_SignalDetect & SW2_ISOIN1))
  { //ＳＷ２が離された
    //Serial.printf_P(PSTR("TASK: MainWork Proc (SW2[ISO IN1] OFF..)\r\n"));
    digitalWrite(BUZZER_PIN, LOW);  //BuzzerをOFF

    act_mode++;
    if(3 < act_mode) act_mode = 0;
    Serial.printf_P(PSTR("[D] Action mode %d\r\n"), (int)act_mode);

    f_RE_SignalDetect &= ~SW2_ISOIN1;   //SW2_ISOIN1 フラグクリア
  }

  //絶縁入力２　ＯＮフラグ確認 ；リレーのシーケンスＯＮ・ＯＦＦのトリガ
  if(0 != (f_FE_SignalDetect & ISOIN2))
  {
    Serial.printf_P(PSTR("[D] TASK: MainWork Proc (ISO IN2 ON"));
    if(!f_RLON_SequenceInProgress && !f_RLOF_SequenceInProgress && !f_NowPowerON && (0 == act_mode))
    { Serial.printf_P(PSTR(", Start RL-ON Sequence)\r\n"));
      RL_DelayTimeCNT1 = 0;
      RL_DelayTimeCNT2 = 0;
      f_RLON_SequenceInProgress = true;
    } else
    if(!f_RLON_SequenceInProgress && !f_RLOF_SequenceInProgress && f_NowPowerON && (0 == act_mode))
    { Serial.printf_P(PSTR(", Start RL-OFF Sequence)\r\n"));
      RL_DelayTimeCNT1 = 0;
      RL_DelayTimeCNT2 = 0;
      f_RLOF_SequenceInProgress = true;
    }
    else
    { Serial.printf_P(PSTR(")\r\n"));
    }
  
    f_FE_SignalDetect &= ~ISOIN2;   //ISOIN2 フラグクリア
  }
  //絶縁入力２　ＯＦＦフラグ確認
  if(0 != (f_RE_SignalDetect & ISOIN2))
  {
    Serial.printf_P(PSTR("[D] TASK: MainWork Proc (ISO IN2 OFF)\r\n"));

    f_RE_SignalDetect &= ~ISOIN2;   //ISOIN2 フラグクリア
  }

  static uint8_t state = 0;
  static uint8_t ret_state = 0;
  switch(state)
  {
    static bool time_sync_start = false;
    static uint8_t prv_act_mode = 0;
    static uint16_t gl_cnt1 = 0;
    static uint16_t wait_count = 0;

    static bool f_request_adc = false;
    static int16_t data[4][1024] = {0};
    static int volt_mv[4] = {0};
    static uint8_t adc_ch = 0;
    static int data_cnt = 0;

    case 0:
      //アイドル
      if(prv_act_mode != act_mode)
      { //動作モードに変化があった
        if(3 == prv_act_mode)
        { //前回までのモードが、リレーリフレッシュ動作だった場合、リレー状態を初期状態に戻す
          for(int8_t r=0; r<8; r++) tca9534_ioport[r] = SET_OFF;
          //常時ＯＮリレー/ＬＥＤをＯＮにする
          tca9534_ioport[RL4_DRV] = SET_ON;
          tca9534_ioport[RL4LED_DRV] = SET_ON;  
          exp_gpio.digitalWrite(tca9534_ioport);

          f_RLON_SequenceInProgress = false;
          f_RLOF_SequenceInProgress = false;
          f_NowPowerON = false;
        }

        if(1 == prv_act_mode)
        { //前回までのモードが、ADS1115のデータ取得の場合、次の時刻同期設定の準備
          Serial.printf_P(PSTR("[D] TASK: MainWork Proc (Sync NTP Server...)\r\n")); 
          time_sync_start = true;
        }

        if(0 == prv_act_mode)
        { //前回までのモードが、通常モードの場合、次のADS1115によるデータ取得の準備
          Serial.printf_P(PSTR("[D] Starting 4-channel ADC at 128sps. Will take approximately 40sec.\r\n")); 
          data_cnt = 0;
          adc_ch = 0;
          memset((void*)data, 0, sizeof(data));
          f_request_adc = false;
        }
        
        //モード・ステート設定およびモード移行までの時間を設定
        prv_act_mode = act_mode;
        ret_state = act_mode;
        gl_cnt1 = 0;
        wait_count = 2000;
        state = 100;
      }
      else
      { //モードに変化がなかった
        state = ret_state;
      }
    break;

    case 1: //ADS1115によるデータ取得
      //state0 を回って戻ってくるので、約２ｍｓ間隔で処理
      if(f_ads_available)
      { //ADS1115 OK
        if(f_request_adc)
        { //ADC要求完了、データ取得と保存
          if(ADS.isReady())
          { data[(int)adc_ch][data_cnt] = ADS.getValue();
            if(3 <= adc_ch)
            { data_cnt++;
              adc_ch = 0;
            }
            else
            { adc_ch++;
            }
            f_request_adc = false;
          }
        }
        else
        { //ADC変換開始要求
          if(1024 > data_cnt)
          { //データサンプリング
            ADS.setGain(1);         //ADS1X15_PGA_4_096V
            ADS.setMode(1);         //ADS1X15_MODE_ONCE
            ADS.setDataRate(4);     //ADS1X15_DATARATE_4
            ADS.requestADC(adc_ch);
            f_request_adc = true;
          }
          else
          { //データ表示 (４ＣＨ を１０２４データ取得したらデータを表示)
            int r;
            float f;
            f = ADS.toVoltage(1) * 1000.0F;  //voltage factor x1000　でｍV
            for(r=0; r<1024; r++)
            { volt_mv[0] = (int)(((float)data[0][r] * f) + 0.5F);
              volt_mv[1] = (int)(((float)data[1][r] * f) + 0.5F);
              volt_mv[2] = (int)(((float)data[2][r] * f) + 0.5F);
              volt_mv[3] = (int)(((float)data[3][r] * f) + 0.5F);
              Serial.printf_P(PSTR("[G] D[%04d]-CH0..3: %06d, %06d, %06d, %06d\r\n"), r, volt_mv[0], volt_mv[1], volt_mv[2], volt_mv[3]);
            }
            data_cnt = 0;
            adc_ch = 0;
            memset((void*)data, 0, sizeof(data));
            f_request_adc = false;
            Serial.printf_P(PSTR("[D] Starting 4-channel ADC at 128sps. Will take approximately 40sec.\r\n")); 
          }
        }
      }

      ret_state = state;
      state = 0;
    break;

    case 2: //RTC 時刻同期 処理
      //state0 を回って戻ってくるので、約２ｍｓ間隔で処理
      if(f_wifi_connected && f_rtc_access)
      { //Wi-Fiに接続済みで、RTC初期化が成功している場合
        if(time_sync_start)
        { //時刻同期
          if(NTP_client->update())  //時刻をサーバーに問合せ。update()は、最大６０秒１回なので同期されるまで最大で６０秒待たされる。
          { //サーバーと同期がとれた
            char ampm[4];
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
              Serial.printf_P(PSTR("[S] Sync time > %d-%02d-%02d(%s) %s %02d:%02d:%02d\r\n\r\n"),
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
            time_sync_start = false;
            Serial.printf_P(PSTR("\r\n[D] TASK: MainWork Proc (Re-sync Success!) EpochTime=%u\r\n"), epoch);
          }
        }
      }

      gl_cnt1++;
      if(time_sync_start)
      { //同期待ち
        if(1500 <= gl_cnt1)  //state0 を回って戻ってくるので、カウンタは約２ｍｓでカウント
        { Serial.printf_P(PSTR("[D] TASK: MainWork Proc (Please wait until the time is synchronized with the NTP server.)\r\n"));
          gl_cnt1 = 0;
        }
      }
      else
      { //同期完了していれば、時刻を表示
        if(500 <= gl_cnt1)  //state0 を回って戻ってくるので、カウンタは約２ｍｓでカウント
        { int ret = RTC.get_time(&RTC.s3539x_times);
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
          gl_cnt1 = 0;
        }
      }

      state = 0;
      ret_state = act_mode;
    break;

    case 3: //リレー接点リフレッシュ
      gl_cnt1++;
      if(50 <= gl_cnt1)  //state0 を回って戻ってくるので、カウンタは約２ｍｓでカウント
      { //おおよそ１００ｍｓ間隔でリレーをＯＮ／ＯＦＦ
        if(f_tca9534_available)
        { //IOエキスパンダ初期化済みの場合
          for(int8_t r=0; r<8; r++)
            tca9534_ioport[r] = !tca9534_ioport[r];
          exp_gpio.digitalWrite(tca9534_ioport);
        }
        gl_cnt1 = 0;
      }

      state = 0;
      ret_state = act_mode;
    break;

    case 100: //ウエイト時間
      wait_count--;
      if(0 == wait_count)
      { state = ret_state;
      }
      else
      { if(prv_act_mode != act_mode)
          state = 0;
      }
    break;

    default:
      state = 0;
    break;
  } //switch(state)


  //----- リレーのＯＮ／ＯＦＦシーケンス
  if(f_RLON_SequenceInProgress && !f_RLOF_SequenceInProgress && !f_NowPowerON)
  { //ONシーケンス
    if(tca9534_ioport[RL1_DRV] && tca9534_ioport[RL2_DRV] && tca9534_ioport[RL3_DRV])
    {
      f_NowPowerON = true;
      f_RLON_SequenceInProgress = false;
    }
    else
    { if((NOF_TICK_CNT(0) <= RL_DelayTimeCNT1) && !tca9534_ioport[RL1_DRV])
      { //0秒
        tca9534_ioport[RL1_DRV] = SET_ON;
        tca9534_ioport[RL1LED_DRV] = SET_ON;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      if((NOF_TICK_CNT(10000) <= RL_DelayTimeCNT1) && !tca9534_ioport[RL2_DRV])
      { //10秒
        tca9534_ioport[RL2_DRV] = SET_ON;
        tca9534_ioport[RL2LED_DRV] = SET_ON;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      if((NOF_TICK_CNT(15000) <= RL_DelayTimeCNT1) && !tca9534_ioport[RL3_DRV])
      { //15秒
        tca9534_ioport[RL3_DRV] = SET_ON;
        tca9534_ioport[RL3LED_DRV] = SET_ON;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      //リレーがＯＮになるまではＬＥＤを点滅させる
      if(NOF_TICK_CNT(250) <= RL_DelayTimeCNT2)
      {
        if(!tca9534_ioport[RL2_DRV])
          tca9534_ioport[RL2LED_DRV] = !tca9534_ioport[RL2LED_DRV];

        if(!tca9534_ioport[RL3_DRV])
          tca9534_ioport[RL3LED_DRV] = !tca9534_ioport[RL3LED_DRV]; 
        
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }
    }
  } else

  if(!f_RLON_SequenceInProgress && f_RLOF_SequenceInProgress && f_NowPowerON)
  { //OFFシーケンス
    if(!tca9534_ioport[RL1_DRV] && !tca9534_ioport[RL2_DRV] && !tca9534_ioport[RL3_DRV])
    {
      f_NowPowerON = false;
      f_RLOF_SequenceInProgress = false;
    }
    else
    { if((NOF_TICK_CNT(0) <= RL_DelayTimeCNT1) && tca9534_ioport[RL3_DRV])
      { //0秒
        tca9534_ioport[RL3_DRV] = SET_OFF;
        tca9534_ioport[RL3LED_DRV] = SET_OFF;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      if((NOF_TICK_CNT(5000) <= RL_DelayTimeCNT1) && tca9534_ioport[RL2_DRV])
      { //5秒
        tca9534_ioport[RL2_DRV] = SET_OFF;
        tca9534_ioport[RL2LED_DRV] = SET_OFF;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      if((NOF_TICK_CNT(15000) <= RL_DelayTimeCNT1) && tca9534_ioport[RL1_DRV])
      { //15秒
        tca9534_ioport[RL1_DRV] = SET_OFF;
        tca9534_ioport[RL1LED_DRV] = SET_OFF;  
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }

      //リレーがＯＦＦになるまではＬＥＤを点滅させる
      if(NOF_TICK_CNT(250) <= RL_DelayTimeCNT2)
      {
        if(tca9534_ioport[RL2_DRV])
          tca9534_ioport[RL2LED_DRV] = !tca9534_ioport[RL2LED_DRV];

        if(tca9534_ioport[RL1_DRV])
          tca9534_ioport[RL1LED_DRV] = !tca9534_ioport[RL1LED_DRV]; 
        
        exp_gpio.digitalWrite(tca9534_ioport);            //ポート出力レベル設定
        RL_DelayTimeCNT2 = 0;
      }
    }
  }
}



/**
 * @brief ワンショット処理タスク
 *        ティックタイマー割込みの「リスタート」で処理が行われる (500ms間隔)
 *        処理が終わるとタスクは、disable（休止状態）
 */
void TskOneShot_ProcCallback(void)
{
  static uint8_t ld4_cnt = 0;
  
  if(0 == act_mode)
  { //動作モードが通常モードの場合
    static uint8_t gl_cnt100 = 0;

    if(!(gl_cnt100 & 0x03))
    { //約２秒間隔
      if(f_rtc_access)
      { //時刻表示
        int ret = RTC.get_time(&RTC.s3539x_times);
        if(S3539x_SUCCESS == ret)
        { //成功
          Serial.printf_P(PSTR("Time [%s/%s] %d/%02X/%02X(%s) %02X:%02X:%02X,  "),
          (RTC.s3539x_times.hour24)? "24h" : "12h", (RTC.s3539x_times.aml_pmh)? "PM" : "AM",
          RTC.s3539x_times.year_four_digit, RTC.s3539x_times.month, RTC.s3539x_times.date, (char*)(&RTC.weeks[RTC.s3539x_times.week][0]),
          (uint8_t)(RTC.s3539x_times.hour&0x3F), RTC.s3539x_times.minute, RTC.s3539x_times.second);        
        }
        else
        { //失敗
          Serial.printf_P(PSTR("[e] S-35390A Get Time error(%d)!\r\n"), ret);
        }
      }

      if(f_ads_available)
      { //ADC ADS1115取得データ表示 (ここでのＡＤＣはブロッキングされえる)
        static int volt_mv[4] = {0};

        ADS.setGain(1);     //ADS1X15_PGA_4_096V
        ADS.setMode(1);     //ADS1X15_MODE_ONCE
        ADS.setDataRate(4); //ADS1X15_DATARATE_4
        adc_1st_data[0] = ADS.readADC(0);
        adc_1st_data[1] = ADS.readADC(1);
        adc_1st_data[2] = ADS.readADC(2);
        adc_1st_data[3] = ADS.readADC(3);

        float f;
        f = ADS.toVoltage(1) * 1000.0F;  //voltage factor x1000　でｍV
        volt_mv[0] = (int)(((float)adc_1st_data[0] * f) + 0.5F);
        volt_mv[1] = (int)(((float)adc_1st_data[1] * f) + 0.5F);
        volt_mv[2] = (int)(((float)adc_1st_data[2] * f) + 0.5F);
        volt_mv[3] = (int)(((float)adc_1st_data[3] * f) + 0.5F);
        Serial.printf_P(PSTR("ADC-CH0..3[mV]: %06d, %06d, %06d, %06d\r\n"), volt_mv[0], volt_mv[1], volt_mv[2], volt_mv[3]);
      }
    } //if(!(gl_cnt100 & 0x03))
    gl_cnt100++;
  }

  digitalWrite(LED_LD4, (ld4_cnt & 0x01));  //LED(LD4)を点滅
  ld4_cnt++;
}


/**
 * @brief ティッカーによる割込み処理
 *        3.333mS ごとに処理
 *        ここには長い処理やdelay()などは書かない、１ｍｓ以下で処理が完了するような内容が望ましい
 */
void IRAM_ATTR onTickTimerISR(void)
{
  static uint8_t Signal_Filter[14] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  static uint8_t IO_SignalMonitorVal = 0x00;
  static uint8_t re, fe;
  static int8_t  r, ssv;
  
  //信号サンプリング
  IO_SignalMonitorVal = (0 != digitalRead(IO0_PIN))?  (IO_SignalMonitorVal | SW2_ISOIN1)  : (IO_SignalMonitorVal & ~SW2_ISOIN1);  //IO0 (sw2 button, ISO-IN1)
  IO_SignalMonitorVal = (0 != digitalRead(IO2_PIN))?  (IO_SignalMonitorVal | ISOIN2)      : (IO_SignalMonitorVal & ~ISOIN2);      //IO2 (ISO-IN2)
  //フィルターと信号安定チェック
  ssv = 0;
  for(r=11; r>0; r--)
  { Signal_Filter[r] = Signal_Filter[r-1];              //データシフト
    if(Signal_Filter[r] != IO_SignalMonitorVal) ssv++;  //最新データと異なっていたらカウントアップ
  }
  Signal_Filter[r] = IO_SignalMonitorVal;               //最新データ

  if(0 == ssv)
  {	//信号サンプリングデータが１２連続(3.333ms x 12 = 40ms)同じ安定
      //信号 ON/OFF エッジ抽出
      Signal_Filter[13] = ~Signal_Filter[12];
      Signal_Filter[12] = Signal_Filter[11];
      fe = ~(Signal_Filter[13] | Signal_Filter[12]);    //信号の立下がりエッジ検出
      re =   Signal_Filter[13] & Signal_Filter[12];     //信号の立上がりエッジ検出

      if((fe != 0x00) || (re != 0x00))
      {	//立下り、立ち上がりの変化あり
          //信号　変化(H -> L)
          if(0 != (fe & SW2_ISOIN1))     f_FE_SignalDetect |= SW2_ISOIN1;     //bit0:
          if(0 != (fe & ISOIN2))         f_FE_SignalDetect |= ISOIN2;         //bit1:

          //信号　変化(L -> H)
          if(0 != (re & SW2_ISOIN1))     f_RE_SignalDetect |= SW2_ISOIN1;     //bit0:
          if(0 != (re & ISOIN2))         f_RE_SignalDetect |= ISOIN2;         //bit1:
      }
  }

  //-----
  gn_cnt1++;
  if(NOF_TICK_CNT(1000) <= gn_cnt1)
  { //main_work 用のトリガフラグ (1000ms)
    gn_cnt1 = 0;
    f_counter_trigger = true;
  }
  //-----
  gn_cnt2++;
  if(NOF_TICK_CNT(500) <= gn_cnt2)
  { //ワンショット処理タスク (500ms)
    gn_cnt2 = 0;
    tsk_OneShot_01.restart();
  }

  //-----
  WiFi_wait_counter++;
  WiFi_ConnectTimeOut_Counter--;
  RL_DelayTimeCNT1++;
  RL_DelayTimeCNT2++;
}
