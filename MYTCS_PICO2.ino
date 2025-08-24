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

#include <Fonts/FreeSans18pt7b.h> // フォントを読み込み
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include <XPT2046_Touchscreen.h> // Touch Screen

#include "MYTCS_PICO2.h"

// global params
Type_System     gbSystem;
Type_Scenario   *gbScenario;
Type_Train      gbTrain;
Type_BtLED      gbBtLed[NUM_LEDBUTTON];
Type_Point      gbPoint[NUM_POINT];
Type_Signal     gbSignal[NUM_SIGNAL];
Type_Crossing   gbCrossing[NUM_CROSSING];
Type_Sensor     gbSensor[NUM_SENSOR];
int gbIsHC595Update = 0;

unsigned int gbHC595Data[NUM_HC595];
unsigned int gbHC166Data[NUM_HC166];

// interrupt
struct repeating_timer st_timer;

// Flags

int flagDrawTFT=0;
int flagCheckTouchPanel=0;
int flagAlt800=0;
int flagAlt500=0;
int flagTESTAlt=0;

// interrupt handler
unsigned int gbIntCounter=0;

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
  gbSystem.scenario_number = 0;
  gbSystem.isEndless = 0;
  gbSystem.scenario_counter_max = 0;
  for(i=0;i<NUM_STREET_LIGHT;i++){
    gbSystem.streetLight[i]=true;
  }

  // scenario pointer
  gbScenario = gbScenario_0;

  // init train status
  gbTrain.direction = TRAIN_DIRECTION_CLOCKWISE;
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
    gbCrossing[i].led1 = false;
    gbCrossing[i].counter = 0;
  }
  // init sensor
  for(i=0;i<NUM_SENSOR;i++){
    gbSensor[i].status=0;
  } 
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
  pinMode(PIN_INTR_1ms, OUTPUT);

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

  // TEST LED on Board
  pinMode(LED_BUILTIN, OUTPUT);

  // init screen
  flagDrawTFT = true;
  canvas.setCursor(100,100); // avoid initial position bug
  canvas.setTextColor(ILI9341_WHITE);
  canvas.setFont(&FreeSans9pt7b);
  canvas.print("STARTING UP...");
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  // reset HC595
  updateHC595();

  /* タイマーの初期化(割込み間隔はusで指定) */
  add_repeating_timer_us(1000, My1msIntHandler, NULL, &st_timer);

  gbIsHC595Update = true;
  flagCheckTouchPanel = true;
  setTFTFromParams();
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
  for(int i=0; i>NUM_TFT_SIGNAL; i++){
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
  for(int i=0; i>NUM_TFT_POINT; i++){
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

  // 描画フラグなら
  if (flagDrawTFT == true)
  {
    // Reset flag
    flagDrawTFT = false;

    // TFT UIパラメータセット
    setTFTFromParams();

    // Draw track
    canvas.fillScreen(0x0000);
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
   
    // UI buttons
    for(int i=0; i<NUM_TFT_BUTTONS; i++){
      drawTFTButton( &tftButton[i], &canvas);
    }
    // the bitmap copy below takes long time 80ms
    digitalWrite(LED_BUILTIN, HIGH); // LED on board
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
    digitalWrite(LED_BUILTIN, LOW);
  }

}
