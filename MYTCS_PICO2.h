/*
  MYTCS_PICO2.h
  N-Gauge layout control system using Raspberry Pi Pico2 W
  Automated railway driving available
  (c)Toshi 2025
*/

#ifndef _MYTCS_PICO2_
#define _MYTCS_PICO2_

// MYTCS_PICO2.h

// 1ms counter
#define TIMER_COUNTER_MAX 100000

// PIN assign
#define PIN_MOTER_M1 2  // p4
#define PIN_MOTER_M2 3  // p5

#define PIN_166_CLK  4  // p6 SRD-CLK
#define PIN_166_DAT  5  // p7 SRD-SID
#define PIN_166_STB  6  // p9 SRD-CS

#define PIN_595_CLK  7  // p10 SWT-CLK
#define PIN_595_DAT  8  // p11 SWT-SID
#define PIN_595_STB  9  // p12 SWT-CS


#define PIN_ANALOG_VR 26 // p31   26-28 available

// Timing test (25 = LED_BUILTIN, onboard LED)
#define PIN_REALCLOCK_1ms    10 // p14 
#define PIN_TEST_1ms_INTRPT  11 // p15
#define PIN_TEST_TFT         12 // p16
#define PIN_TEST_HC595       13 // p17
#define PIN_TEST_HC166       14 // p19


// PWM
// #define PWM_FREQ 20000  // 20kHz
// #define PWM_RESO_BIT 8  // resolution　8bit
// #define PWM_TIMER_CH 0

// hardware timer
#define PRESCALER_DIVIDE_RATE 80
#define TIMER_UI_INTERVAL 1000  // 10ms
#define TIMER_UI_HANDLER_CH 1

// num of chip
#define NUM_HC166 2
#define NUM_HC595 4

#define HC595_BTLED 0
#define HC595_POINT 1
#define HC595_SIG1  2
#define HC595_SIG2  3

#define HC166_LEDBT  0
#define HC166_SENSOR 1

// number of items
#define NUM_LEDBUTTON 8
#define NUM_SPEED_LED 8
#define NUM_POINT 4
#define NUM_SIGNAL 3
#define NUM_CROSSING 2
#define NUM_SENSOR 8

// TRAIN status
#define TRAIN_SPEED_TOLERANCE 3
#define TRAIN_SPEED_MIN 5
#define TRAIN_DIRECTION_CLOCKWISE 0
#define TRAIN_DIRECTION_COUNTERCLOCKWISE 1

// SYSTEM
#define NUM_SCENARIO_ID 10
#define SYSTEM_MODE_MANUAL 0
#define SYSTEM_MODE_AUTO   1
#define NUM_STREET_LIGHT  5
typedef struct _Type_System
{
  int mode; // 0:manual, 1:auto
  int scenario_counter;
  int scenario_counter_max;
  int wait_timer;
  int scenario_number;
  int isEndless;
  int streetLight[NUM_STREET_LIGHT];
}Type_System;

/*
TRIGGER             PARAM_0      PARAM_1
---------------------------------------
SCN_TRIGGER_INVALID *ignore the scenario*
---------------------------------------
SCN_NOTRIGGER       0:point_1      dir
                    1:point_2      dir
                    2:train_dir    dir
                    3:train_spped  255
                    4:crossing     on/off
---------------------------------------
SCN_TRIGGER_SENSOR  sensor_no      N/A
---------------------------------------
SCN_TRIGGER_MSEC    wait_msec      N/A
---------------------------------------
*/

// auto drive scenario
//#define NUM_SCENARIO_COUNTER     200
#define SCN_TRIGGER_INVALID 0
#define SCN_TRIGGER_NO      1  // just do it
#define SCN_TRIGGER_SENSOR  2  // wait specified sensor
#define SCN_TRIGGER_MSEC    3  // wait specified msec

#define SCN_NOTG_P1 0
#define SCN_NOTG_P2 1
#define SCN_NOTG_TR_DIR 2
#define SCN_NOTG_SPEED 3
#define SCN_NOTG_CROSS 4
#define SCN_NOTG_ACCEL 5

// MY ATCS control funcions
#define BITMASK_1 0x00000001
#define BITMASK_2 0x00000002
#define BITMASK_3 0x00000004
#define BITMASK_4 0x00000008
#define BITMASK_5 0x00000010
#define BITMASK_6 0x00000020
#define BITMASK_7 0x00000040
#define BITMASK_8 0x00000080
#define BITMASK_FULL 0x000000ff

typedef struct _Type_Scenario
{
  int trigger;
  int param_0;
  int param_1;
}Type_Scenario;


typedef struct _Type_Train
{
  int direction; // 0 or 1
  int prevDirection; // 0 or 1
  int speed; // 0-255
  int prevSpeed; // revious state
  int tempSpeed; // for def comparison
  int maxSpeed; // 255
  int targetSpeed; // target speed specified when AUTO mode
  int accelTime; // interval in ms to in/decrease speed
} Type_Train;

// Button LED
typedef struct _Type_BtLED
{
  int led;   // 0 or 1
  int status; // 1=pressed
  int prevStatus; // revious state
  int prevBt; // 
} Type_BtLED;

// Speed Indicator LED
typedef struct _Type_SpeedLED
{
  int led; // 0 or 1
} Type_SpeedLED;

// POINT status
#define POINT_DIRECTION_LEFT   0
#define POINT_DIRECTION_RIGHT 1
#define POINT_DRIVE_DURATION 50 // test 50ms

typedef struct _Type_Point
{
  int direction; // 0 or 1
  int prevDirection;
  int driveM1;
  int driveM2;
  int isDriving;
  int onCounter;
} Type_Point;

// SIGNAL status
#define SIGNAL_COLOR_RED 0
#define SIGNAL_COLOR_YELLOW 1
#define SIGNAL_COLOR_GREEN 2

#define SIGNAL_STATUS_STOP 0
#define SIGNAL_STATUS_GO 1

#define SIGNAL_YELLOW_DURATION 3000 // 3sec

typedef struct _Type_Signal
{
  int status; // 0:stop, 1:go
  int prevStatus;
  int color; // 0:red, 
  int isChanging; //
  int counter;
} Type_Signal;

// Crossing status
#define CROSSING_STATUS_OFF 0
#define CROSSING_STATUS_ON 1
#define CROSSING_ON_DURATION 500

typedef struct _Type_Crossing
{
  int status; // 0 or 1
  int prevStatus;
  int led1;     
  int led2;     
  int counter;
} Type_Crossing;

// SENSOR status
typedef struct _Type_Sensor
{
  int status; // 0 or 1
} Type_Sensor;


// global params
extern Type_System     gbSystem;
extern Type_Scenario   *gbScenario;
extern Type_Train      gbTrain;
extern Type_BtLED  gbBtLed[];
extern Type_Point      gbPoint[];
extern Type_Signal     gbSignal[];
extern Type_Crossing   gbCrossing[];
extern Type_Sensor     gbSensor[];
extern int gbIsHC595Update;

extern unsigned int gbHC595Data[];
extern unsigned int gbHC166Data[];
extern unsigned int gbIntCounter;
extern unsigned int lastUITime;

// interrupt
extern struct repeating_timer st_timer;

/*
  #define TEST_LED_ON_BOARD 25
  pinMode(TEST_LED_ON_BOARD, OUTPUT);

  // Color definitions
  #define ILI9341_BLACK 0x0000       ///<   0,   0,   0
  #define ILI9341_NAVY 0x000F        ///<   0,   0, 123
  #define ILI9341_DARKGREEN 0x03E0   ///<   0, 125,   0
  #define ILI9341_DARKCYAN 0x03EF    ///<   0, 125, 123
  #define ILI9341_MAROON 0x7800      ///< 123,   0,   0
  #define ILI9341_PURPLE 0x780F      ///< 123,   0, 123
  #define ILI9341_OLIVE 0x7BE0       ///< 123, 125,   0
  #define ILI9341_LIGHTGREY 0xC618   ///< 198, 195, 198
  #define ILI9341_DARKGREY 0x7BEF    ///< 123, 125, 123
  #define ILI9341_BLUE 0x001F        ///<   0,   0, 255
  #define ILI9341_GREEN 0x07E0       ///<   0, 255,   0
  #define ILI9341_RED 0xF800         ///< 255,   0,   0
  #define ILI9341_CYAN 0x07FF        ///<   0, 255, 255
  #define ILI9341_MAGENTA 0xF81F     ///< 255,   0, 255
  #define ILI9341_YELLOW 0xFFE0      ///< 255, 255,   0
  #define ILI9341_WHITE 0xFFFF       ///< 255, 255, 255
  #define ILI9341_ORANGE 0xFD20      ///< 255, 165,   0
  #define ILI9341_GREENYELLOW 0xAFE5 ///< 173, 255,  41
  #define ILI9341_PINK 0xFC18        ///< 255, 130, 198

  FreeMono12pt7b.h
  FreeMono18pt7b.h
  FreeMono24pt7b.h
  FreeMono9pt7b.h
  FreeMonoBold12pt7b.h
  FreeMonoBold18pt7b.h
  FreeMonoBold24pt7b.h
  FreeMonoBold9pt7b.h
  FreeMonoBoldOblique12pt7b.h
  FreeMonoBoldOblique18pt7b.h
  FreeMonoBoldOblique24pt7b.h
  FreeMonoBoldOblique9pt7b.h
  FreeMonoOblique12pt7b.h
  FreeMonoOblique18pt7b.h
  FreeMonoOblique24pt7b.h
  FreeMonoOblique9pt7b.h
  FreeSans12pt7b.h
  FreeSans18pt7b.h
  FreeSans24pt7b.h
  FreeSans9pt7b.h
  FreeSansBold12pt7b.h
  FreeSansBold18pt7b.h
  FreeSansBold24pt7b.h
  FreeSansBold9pt7b.h
  FreeSansBoldOblique12pt7b.h
  FreeSansBoldOblique18pt7b.h
  FreeSansBoldOblique24pt7b.h
  FreeSansBoldOblique9pt7b.h
  FreeSansOblique12pt7b.h
  FreeSansOblique18pt7b.h
  FreeSansOblique24pt7b.h
  FreeSansOblique9pt7b.h
  FreeSerif12pt7b.h
  FreeSerif18pt7b.h
  FreeSerif24pt7b.h
  FreeSerif9pt7b.h
  FreeSerifBold12pt7b.h
  FreeSerifBold18pt7b.h
  FreeSerifBold24pt7b.h
  FreeSerifBold9pt7b.h
  FreeSerifBoldItalic12pt7b.h
  FreeSerifBoldItalic18pt7b.h
  FreeSerifBoldItalic24pt7b.h
  FreeSerifBoldItalic9pt7b.h
  FreeSerifItalic12pt7b.h
  FreeSerifItalic18pt7b.h
  FreeSerifItalic24pt7b.h
  FreeSerifItalic9pt7b.h
  Tiny3x3a2pt7b.h
*/

#define TFT_WIDTH   320 // 画面幅
#define TFT_HEIGHT  240 // 画面高さ
                             // pico <-> 9341
#define COMMON_MISO_RX  16   // p21  <-> p13
#define COMMON_SCK      18   // p24  <-> p7,p10
#define COMMON_MOSI_TX  19   // p25  <-> p6,p12

#define TFT_CS          17   // p22  <-> p3
#define TFT_DC          21   // p27  <-> p5
#define TFT_RST         22   // p29  <-> p4

#define TOUCH_CS        20   // p26  <-> p11

#define ILI9341_GREY127     0x7bef
#define ILI9341_DARKORANGE  0x7a80 // 50% orange
#define ILI9341_NIGHTORANGE 0x3940 // 25% orange
#define ILI9341_NIGHTGREY   0x3a07 // 25% gray

typedef struct _Type_TFTButton
{
    int pos_x;
    int pos_y;
    int width;
    int height;
    int color_frame;
    int color_fill;
    int color_text;
    int text_offset_x;
    int text_offset_y;
    char text[16];
    int isPressed;
    int isPressedPrevious;
}Type_TFTButton;

typedef struct _Type_TFTSensor
{
    int isActive;
    int pos_x;
    int pos_y;
}Type_TFTSensor;

typedef struct _Type_TFTCross{
    int isActive;
    int pos_x;
    int pos_y;
}Type_TFTCross;

typedef struct _Type_TFTSignal{
    int color; // 0=red 1=yellow 2=green
    int colorPrevious;
    int pos_x;
    int pos_y;
}Type_TFTSignal;

typedef struct _Type_TFTPoint{
    int direction; // 0= clockwise 1=counterclockwise
    int directionPrevious;
}Type_TFTPoint;

#define NUM_TFT_BUTTONS 7
#define UI_BUTTON_DETECTION_INTARVAL 50 // ms
extern Type_TFTButton tftButton[];

#define NUM_TFT_SENSOR 8
extern Type_TFTSensor tftSensor[];

#define NUM_TFT_CROSS 1
extern Type_TFTCross tftCross[];

#define NUM_TFT_SIGNAL 3
extern Type_TFTSignal tftSignal[];

#define NUM_TFT_POINT 2
extern Type_TFTPoint tftPoint[];

// pico_util.ino
extern void setParamFromTFTUI( void );
extern void setTFTFromParams(void);
extern void getPositionOnScreen( TS_Point *tsPoint, TS_Point *ScreenPos);
extern void drawTFTButton( Type_TFTButton *bt, GFXcanvas16 *canvas );
extern void isTFTButtonPressed( Type_TFTButton *bt, TS_Point *screenPos);
extern void drawTFTWholeTrack(GFXcanvas16 *canvas);
extern void drawTFTRail( GFXcanvas16 *canvas );
extern void drawTFTSensor( GFXcanvas16 *canvas );
extern void drawTFTDebugText( GFXcanvas16 *canvas, char *text );
extern void drawTFTSpeed( GFXcanvas16 *canvas, int speed );
extern void drawTFTDirection( GFXcanvas16 *canvas, int speed );
extern void drawTFTOthers( GFXcanvas16 *canvas );
extern void drawTFTPoint( GFXcanvas16 *canvas );
extern void drawTFTCross( GFXcanvas16 *canvas );
extern void drawTFTSignal( GFXcanvas16 *canvas );
extern void setSensorTFTFromHC166(void);

// global vals
extern uint slice_num;
extern Type_Scenario gbScenario_0[];
extern Type_Scenario gbScenario_1[];
extern Type_Scenario gbScenario_2[];

extern int flagDrawTFT;
extern int flagCheckTouchPanel;
extern int flagAlt800;
extern int flagTESTAlt;
extern int flagAlt500;


// global funcs
extern bool My1msIntHandler(struct repeating_timer *t);
extern void initScenarioParams(void);
extern int isSensorActive(int sensor_id);
extern void HandleAutoDriveScenario(void);
extern void updatePowerPack(void);
extern void MySerialOutput( void );
extern void MySerialInput( void );
extern void HandleCtrlButton(void);
extern void HandleLedButton(void);
extern void GetSpeedVr( void );
extern void HandleSignal(void);
extern void DrivePoint(void);
extern void DriveCrossing(void);
extern void updateHC595(void);
extern void updateButtonLedStatus(void);
extern void updateParamsFromCtrlButton(void);
extern void updateParamsFromLcdButton(void);



#endif  // MAIN_H
