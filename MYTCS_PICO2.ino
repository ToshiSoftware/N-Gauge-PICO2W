/*
  MYTCS_PICO2.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  Automated railway driving available
  (c)Toshi 2025
*/

#include <time.h>
#include <hardware/pwm.h>

#include <Adafruit_GFX.h>      // Adafruitのグラフィックスライブラリ
#include <Adafruit_ILI9341.h>  // 液晶表示器 ILI9341 制御用ライブラリ

#include <WiFi.h>
#include <WebServer.h>

#include <Fonts/FreeSans18pt7b.h> // フォントを読み込み
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include <XPT2046_Touchscreen.h> // Touch Screen

#include "MYTCS_PICO2.h"

// WIFI
// 実際の SSID/パスワードは wifi_secrets.h に記述（git 管理外: .gitignore 参照）
// ファイルが無い環境では下記の仮の値でコンパイルされる
#if __has_include("wifi_secrets.h")
  #include "wifi_secrets.h"
#else
  #define WIFI_SSID     "your-ssid"
  #define WIFI_PASSWORD "your-password"
#endif
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);  // HTTPサーバーをポート80で起動

// global params
Type_System     gbSystem;
Type_Scenario   *gbScenario;
Type_Train      gbTrain;
Type_BtLED      gbBtLed[NUM_LEDBUTTON];
Type_Point      gbPoint[NUM_POINT];
Type_Signal     gbSignal[NUM_SIGNAL];
Type_Crossing   gbCrossing[NUM_CROSSING];
Type_Sensor     gbSensor[NUM_SENSOR];
volatile int gbIsHC595Update = 0;

int flagEnableWifi=0;

unsigned int gbHC595Data[NUM_HC595];
unsigned int gbHC166Data[NUM_HC166];

// interrupt
struct repeating_timer st_timer;

// Flags

// 割込み(1ms)と loop() の両方から参照するため volatile
volatile int flagDrawTFT=0;
volatile int flagCheckTouchPanel=0;
volatile int flagAlt800=0;
volatile int flagAlt500=0;
volatile int flagTESTAlt=0;

// interrupt handler
volatile unsigned int gbIntCounter=0;

// ILI9341ディスプレイのインスタンスを作成
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

// TOUCH SCREEN
XPT2046_Touchscreen ts(TOUCH_CS);

// スプライト（メモリ描画領域から一括表示）をcanvasとして準備
// 画面表示をtftではなくcanvasで指定して一括描画することでチラツキなく表示できる
GFXcanvas16 canvas(TFT_WIDTH, TFT_HEIGHT);  // 16bitカラースプライト（オフスクリーンバッファ）

void initParams(void)
{
  int i;
  // device state
  for(i=0;i<NUM_HC595;i++){
    gbHC595Data[i] = 0;
  }
  for(i=0;i<NUM_HC166;i++){
    gbHC166Data[i] = 0;
  }  

  // init system mode
  gbSystem.mode = SYSTEM_MODE_MANUAL;
  gbSystem.scenario_counter = 0;
  gbSystem.wait_timer = 0;
  gbSystem.scenario_number = 1;
  gbSystem.isEndless = 0;
  gbSystem.scenario_counter_max = 0;
  for(i=0;i<NUM_STREET_LIGHT;i++){
    gbSystem.streetLight[i]=true;
  }

  // scenario pointer
  gbScenario = gbScenario_0; // Stop state speed = 20

  // init train status
  gbTrain.direction = TRAIN_DIRECTION_COUNTERCLOCKWISE;
  gbTrain.prevDirection = TRAIN_DIRECTION_CLOCKWISE;
  gbTrain.speed = 0;
  gbTrain.prevSpeed = 0;
  gbTrain.maxSpeed = 255;
  gbTrain.targetSpeed = 0;
  gbTrain.accelTime = 10;

  // init led button
  for(i=0;i<NUM_LEDBUTTON;i++){
    gbBtLed[i].led=0;
    gbBtLed[i].status=0;
    gbBtLed[i].prevStatus=0;
    gbBtLed[i].prevBt=0;
  }

  // init points
  for(i=0;i<NUM_POINT;i++) {
    gbPoint[i].direction=POINT_DIRECTION_LEFT;
    gbPoint[i].prevDirection=POINT_DIRECTION_RIGHT;
    gbPoint[i].onCounter=0;
    gbPoint[i].isDriving=false;
    gbPoint[i].driveM1 = false; // true=STOP_MODE false=Hi-Z
    gbPoint[i].driveM2 = false; // revised on 2025/07/26 when a point was broken
  } 
  // init train signals
  for(i=0;i<NUM_SIGNAL;i++){
    gbSignal[i].status=SIGNAL_STATUS_STOP;
    gbSignal[i].prevStatus=SIGNAL_STATUS_STOP;
    gbSignal[i].color=SIGNAL_COLOR_RED;
    gbSignal[i].isChanging=false;
    gbSignal[i].counter=0;
  }
  gbSignal[1].status = SIGNAL_STATUS_GO;
  gbSignal[1].prevStatus = SIGNAL_STATUS_GO;
  gbSignal[1].color = SIGNAL_COLOR_GREEN;
  gbSignal[2].status = SIGNAL_STATUS_GO;
  gbSignal[2].prevStatus = SIGNAL_STATUS_GO;
  gbSignal[2].color = SIGNAL_COLOR_GREEN;
  // init crossing sign
  for(i=0;i<NUM_CROSSING;i++) {
    gbCrossing[i].status=CROSSING_STATUS_OFF;
    gbCrossing[i].prevStatus=CROSSING_STATUS_OFF;
    gbCrossing[i].led1 = false;
    gbCrossing[i].led2 = false;
    gbCrossing[i].counter = 0;
  }
  // init sensor
  for(i=0;i<NUM_SENSOR;i++){
    gbSensor[i].status=0;
  } 
}

IPAddress myIPV4;
char myIPV4Str[32];
char htmlBody[65536]="";
char tempStr[128];

// WIFI
void handleRoot() {
  strcpy(htmlBody, "<h1>MYTCS_PICO2W</h1>\n");
  sprintf(tempStr, "<p>speed=%d<p>\n", gbTrain.speed);
  strcat(htmlBody,tempStr);
  server.send(200, "text/html", htmlBody);
}

int WIFI_setup(){
  int timeoutCount=0;

  //Serial.begin(115200);
  //delay(1000);

  // Wi-Fi接続
  WiFi.begin(ssid, password);

  //Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
    timeoutCount++;
    if(timeoutCount>8) return 0;
    canvas.print("#");
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  }
  //Serial.println("\nConnected! IP address: ");
  myIPV4  = WiFi.localIP();
  snprintf(myIPV4Str, sizeof(myIPV4Str), "IP=%u.%u.%u.%u", myIPV4[0], myIPV4[1], myIPV4[2], myIPV4[3]);
  //Serial.println(myIPV4);

  // ルートハンドラ設定
  server.on("/", handleRoot);

  // サーバー開始
  server.begin();
  //Serial.println("HTTP server started");

  return 1;
}

// PWM slice num
uint slice_num;

void setup() {
  // init globals
  initParams();
  initScenarioParams();

  // Serial pins
  pinMode(PIN_166_DAT, INPUT_PULLUP);
  pinMode(PIN_166_CLK, OUTPUT);
  pinMode(PIN_166_STB, OUTPUT);

  pinMode(PIN_595_DAT, OUTPUT);
  pinMode(PIN_595_CLK, OUTPUT);
  pinMode(PIN_595_STB, OUTPUT);

  // Timimg test pins
  pinMode(PIN_REALCLOCK_1ms, OUTPUT);
  pinMode(PIN_TEST_1ms_INTRPT, OUTPUT);
  pinMode(PIN_TEST_TFT, OUTPUT);
  pinMode(PIN_TEST_HC595, OUTPUT);
  pinMode(PIN_TEST_HC166, OUTPUT);

  // reset HC595
  updateHC595();

  // M1,M2のピン機能をPWMに設定
  gpio_set_function(PIN_MOTER_M1, GPIO_FUNC_PWM); // GPIO2
  gpio_set_function(PIN_MOTER_M2, GPIO_FUNC_PWM); // GPIO3
  // M1,M2のPWMスライスを取得
  // 偶数番(2n)のGPIOはAチャンネル、奇数番(2n+1)のGPIOはBチャンネル, スライス番号は n % 8
  // PWM周波数 f = sysclock / ((wrap+1) * clkdiv) より
  // clkdiv = sysclock / ((wrap+1) * f )
  slice_num = pwm_gpio_to_slice_num(2);
  // PWM周期を設定 (20kHz)
  // PICO2 CLOCK = 150MHZ
  // clkdiv = (150,000,000 / 20,000 ) / 256(8bit) --> 29.296875
  pwm_set_clkdiv(slice_num, 29.296875);
  pwm_set_wrap(slice_num, 255); // 8bit
  // チャンネルA(GP2)とチャンネルB(GP3)のPWMのHigh期間を設定
  pwm_set_chan_level(slice_num, PWM_CHAN_A, 256); // STOP = H (duty:100%)
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 256); //
  // PWM出力イネーブル
  pwm_set_enabled(slice_num, true);
  
  // reset HC595
  updateHC595();

  // TFT panel
  // SPI0初期設定
  SPI.setTX(COMMON_MOSI_TX); // SPI0のTX(MOSI)
  SPI.setRX(COMMON_MISO_RX);
  SPI.setSCK(COMMON_SCK); // SPI0のSCK

  // TFT初期設定
  tft.begin();           // TFTを初期化
  tft.setRotation(3);    // TFTの回転を設定（0-3）
  canvas.setTextSize(1); // テキストサイズを設定

  //タッチ入力開始
  ts.begin();
  ts.setRotation(3);

  // On-board LED : 常時消灯
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // init screen
  flagDrawTFT = true;
  canvas.setCursor(50,100); // avoid initial position bug
  canvas.setTextColor(ILI9341_WHITE);
  canvas.setFont(&FreeSans9pt7b);
  canvas.print("STARTING UP...");

  // reset HC595
  updateHC595();

  /* タイマーの初期化(割込み間隔はusで指定) */
  add_repeating_timer_us(1000, My1msIntHandler, NULL, &st_timer);

  gbIsHC595Update = true;
  flagCheckTouchPanel = true;
  setTFTFromParams();

  if(flagEnableWifi == true){
    // WIFI
    canvas.setCursor(50,140); // avoid initial position bug
    canvas.print("CONNECTING WIFI...");
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

    if(WIFI_setup()==0){
      sprintf(myIPV4Str, "no connection");
    }
  }
  // for debug
  strcpy(myIPV4Str, "Speed test 20");
}

int ct =0;

TS_Point tPoint;
boolean bTouch=false;
boolean bTouchPrev=false;
TS_Point screenPos;

int timerDisplayCurrent=0;
int timerDisplayPrevius=-1;
int speedPrevious=256;
unsigned int lastUITime=0;
char debugText[256] = "";

void loop() {
  int i;

  if(flagEnableWifi == true){
    // WIFI
    server.handleClient();  // クライアント処理
  }

  if(gbIntCounter%250 == 0){
    gbIsHC595Update = true;
  }

  // ---------------- pico -----------------
  // タッチパネル状態読み取り
  bTouch = false;
  tPoint.x=0;
  tPoint.y=0;

  if(flagCheckTouchPanel == true){
    flagCheckTouchPanel = false;
    // whether touched or not
    bTouch = ts.touched();    
    if (bTouch == true){
      tPoint = ts.getPoint();
      //flagDrawTFT = true;
    }
    else {
      //タッチがなければタッチ無表示
      tPoint.x=0;
      tPoint.y=0;
    }
    // ローカル座標を取得
    getPositionOnScreen( &tPoint, &screenPos);
  }

  // 信号変化判定
  for(int i=0; i<NUM_TFT_SIGNAL; i++){
    if(tftSignal[i].colorPrevious != tftSignal[i].color){
      flagDrawTFT = true;
    }
    tftSignal[i].colorPrevious = tftSignal[i].color;
  }

  // Speed 変化判定
  if(speedPrevious != gbTrain.speed){
    flagDrawTFT = true;
  }
  speedPrevious = gbTrain.speed;

  // ポイント変化判定
  for(int i=0; i<NUM_TFT_POINT; i++){
    if(tftPoint[i].directionPrevious != tftPoint[i].direction){
      flagDrawTFT = true;
    }
    tftPoint[i].directionPrevious = tftPoint[i].direction;
  }

  // TFT UI から内部変数をセットする
  for(int i=0; i<NUM_TFT_BUTTONS; i++){
    isTFTButtonPressed( &tftButton[i], &screenPos);
  }
  setParamFromTFTUI();

#if TFT_UI_DESIGN == 2
  // 走行中は路面電車のイラストを揺らす（電車の周囲だけ部分描画）
  uiTramAnimate();
#endif

  // 描画フラグなら
  if (flagDrawTFT == true)
  {
    // Reset flag
    flagDrawTFT = false;

    // TFT UIパラメータセット
    setTFTFromParams();

    // Draw track
    canvas.fillScreen(TFT_COLOR_BG);
    drawTFTRail(&canvas);
    drawTFTPoint(&canvas);
    drawTFTCross(&canvas);
    drawTFTSignal(&canvas);
    drawTFTSensor(&canvas);
    drawTFTSpeed(&canvas, gbTrain.speed);
    drawTFTDirection(&canvas, gbTrain.direction);
    drawTFTOthers(&canvas);

    // debug
    //drawTFTDebugText(&canvas, (char*)"Scenario:03 | Mode:AUTO");
    // int temp = analogRead( PIN_ANALOG_VR );
    // sprintf(debugText, "analogRead=%d", temp);
    // drawTFTDebugText(&canvas, debugText);
    // sprintf(debugText, "P[0]: isPressed:%d dir:%d", tftButton[0].isPressed, gbPoint[0].direction);
    // drawTFTDebugText(&canvas, debugText);

    // sprintf(debugText, "%d", gbIntCounter/1000);
    // drawTFTDebugText(&canvas, debugText);
   
    // uint16_t colorTrack = getILI9341Color(63, 64, 63);
    // sprintf(debugText, "%04x", colorTrack);
    // drawTFTDebugText(&canvas, debugText);

#if TFT_UI_DESIGN == 1
    drawTFTDebugText(&canvas, myIPV4Str);
#endif
    // 新デザインではデバッグ文字は表示しない（必要なら drawTFTDebugText() を呼ぶ）
   
    // UI buttons
    for(int i=0; i<NUM_TFT_BUTTONS; i++){
      drawTFTButton( &tftButton[i], &canvas);
    }
    // the bitmap copy below takes long time 80ms
    digitalWrite(PIN_TEST_TFT, HIGH); // timing test pin
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
    digitalWrite(PIN_TEST_TFT, LOW);
  }

}
