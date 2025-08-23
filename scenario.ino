/*
  scenario.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  Automated driving scenario functions
  (c)Toshi 2025
*/

// #define BITMASK_1 0x01
// #define BITMASK_2 0x02
// #define BITMASK_3 0x04
// #define BITMASK_4 0x08
// #define BITMASK_5 0x10
// #define BITMASK_6 0x20
// #define BITMASK_7 0x40
// #define BITMASK_8 0x80

/*
 ----6-------7-------5---
|                        |
|      1  2          CROSS
|       \  \             |
|        \-P2            |
|            \           |
|             3          8
|              \         |
 ----4----------P1-------

Direction <- TRAIN_DIRECTION_CLOCKWISE
          -> TRAIN_DIRECTION_COUNTERCLOCKWISE
POINT     <- POINT_DIRECTION_LEFT
          -> POINT_DIRECTION_RIGHT
CROSS     off CROSSING_STATUS_OFF
          on  CROSSING_STATUS_ON
*/

#include "MYTCS_PICO2.h"

//#define _DEBUG 1

#define SPEED_STOP 0
#define SPEED_LOW 50
#define SPEED_MID 70
#define SPEED_HIGH 90

// Scenario macros
#define INVALID_DATA()    {SCN_TRIGGER_INVALID,0,0},
#define WAIT_TIME(time)    {SCN_TRIGGER_MSEC, time, 0},
//#ifdef _DEBUG
//  #define WAIT_SENSOR(no)    {SCN_TRIGGER_MSEC, 2000, 0},
//#else
  #define WAIT_SENSOR(no)    {SCN_TRIGGER_SENSOR, no, 0},
//#endif
#define POINT_1(dir)       {SCN_TRIGGER_NO, SCN_NOTG_P1, dir},
#define POINT_2(dir)       {SCN_TRIGGER_NO, SCN_NOTG_P2, dir},
#define CROSS(onoff)       {SCN_TRIGGER_NO, SCN_NOTG_CROSS, onoff},
#define SPEED(spd)         {SCN_TRIGGER_NO, SCN_NOTG_SPEED, spd},
#define DIRECTION(drctn)   {SCN_TRIGGER_NO, SCN_NOTG_TR_DIR, drctn},
#define ACCEL(ms)          {SCN_TRIGGER_NO, SCN_NOTG_ACCEL, ms},

//#define NUM_SCENARIO_COUNTER (sizeof(gbScenario) / sizeof(Type_Scenario))

// scenario data

Type_Scenario gbScenario_0[] ={
    // 初期化
    WAIT_TIME(1000)
    //WAIT_SENSOR(2)
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    WAIT_TIME(1000)

    // ポイント位置初期化
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_1(POINT_DIRECTION_RIGHT)

    POINT_2(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_2(POINT_DIRECTION_RIGHT)

    // 発車する
    WAIT_TIME(3000)
    ACCEL(100)
    SPEED(SPEED_MID)

    // ウォーミングアップで1周する
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_ON)    
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)   
    POINT_1(POINT_DIRECTION_LEFT)
    ACCEL(200)
    SPEED(SPEED_HIGH)
    WAIT_SENSOR(4)
    ACCEL(100)
    SPEED(SPEED_MID)

    // 8を通過したら踏切ON
    WAIT_SENSOR(8)
    ACCEL(50)
    SPEED(SPEED_LOW)
    CROSS(CROSSING_STATUS_ON)

    // 5を通過
    WAIT_SENSOR(5)
    WAIT_TIME(1400)
    ACCEL(20)
    SPEED(SPEED_STOP)

    // 5秒停車　ポイント１を周回へ
    WAIT_TIME(3000)
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)

    // 方向を変えて変えて奥の駅を出発
    DIRECTION(TRAIN_DIRECTION_CLOCKWISE)
    ACCEL(200)  
    SPEED(SPEED_MID)

    // 8を通過したら踏切OFF
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_OFF)

    // 4を通過したら手前の駅で停車
    WAIT_SENSOR(4)
    WAIT_TIME(100)
    ACCEL(20)  
    SPEED(SPEED_STOP)

    // 手前の駅を出発
    WAIT_TIME(5000)
    ACCEL(200)  
    SPEED(SPEED_MID)

    // 6を通過したら停車　踏切ON
    WAIT_SENSOR(6)
    //WAIT_TIME(1000)
    ACCEL(50)
    SPEED(SPEED_LOW)
    WAIT_SENSOR(7)   
    WAIT_TIME(600)
    ACCEL(20)
    SPEED(SPEED_STOP)
    CROSS(CROSSING_STATUS_ON)
    WAIT_TIME(5000)
    
    // ポイント１を切り替えて駅へ入る
    POINT_1(POINT_DIRECTION_RIGHT)
    ACCEL(200)      
    SPEED(SPEED_MID)
    // 8を通過
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_OFF)
    // 3を通過
    WAIT_SENSOR(3)
    //WAIT_TIME(1000)
    ACCEL(100)      
    SPEED(SPEED_LOW)
    WAIT_TIME(1000)
    ACCEL(20)      
    SPEED(40)
    // 2を通過　２番線へ到着
    WAIT_SENSOR(2)
    //WAIT_TIME(500)
    ACCEL(5)      
    SPEED(SPEED_STOP)
  
    // ５秒停車
    WAIT_TIME(5000)


    // オレンジ車両へ変更　----------------------
    // 初期化
    WAIT_TIME(1000)
    //WAIT_SENSOR(1)
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    WAIT_TIME(1500)
    POINT_1(POINT_DIRECTION_RIGHT)
    POINT_2(POINT_DIRECTION_LEFT)

     // 発車する
    WAIT_TIME(3000)
    ACCEL(100)
    SPEED(SPEED_MID)

    // ウォーミングアップで2周する
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_ON)    
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)   
    POINT_1(POINT_DIRECTION_LEFT)
    ACCEL(200)
    SPEED(SPEED_HIGH)
    //
    WAIT_SENSOR(4)
    ACCEL(100)
    SPEED(SPEED_MID)

    // 8を通過したら踏切ON
    WAIT_SENSOR(8)
    ACCEL(50)
    SPEED(SPEED_LOW)
    CROSS(CROSSING_STATUS_ON)

    // 5を通過
    WAIT_SENSOR(5)
    WAIT_TIME(1400)
    ACCEL(20)
    SPEED(SPEED_STOP)

    // 5秒停車　ポイント１を周回へ
    WAIT_TIME(3000)
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)

    // 方向を変えて変えて奥の駅を出発
    DIRECTION(TRAIN_DIRECTION_CLOCKWISE)
    ACCEL(200)  
    SPEED(SPEED_MID)

    // 8を通過したら踏切OFF
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_OFF)

    // 4を通過したら手前の駅で停車
    WAIT_SENSOR(4)
    WAIT_TIME(200)
    ACCEL(20)  
    SPEED(SPEED_STOP)

    // 手前の駅を出発
    WAIT_TIME(5000)
    ACCEL(200)  
    SPEED(SPEED_MID)

    // 6を通過したら停車　踏切ON
    WAIT_SENSOR(6)
    //WAIT_TIME(1000)
    ACCEL(50)
    SPEED(SPEED_LOW)
    WAIT_SENSOR(7)   
    WAIT_TIME(600)
    ACCEL(20)
    SPEED(SPEED_STOP)
    CROSS(CROSSING_STATUS_ON)
    WAIT_TIME(5000)

    // ポイント１を切り替えて駅へ入る
    POINT_1(POINT_DIRECTION_RIGHT)
    ACCEL(200)      
    SPEED(SPEED_MID)
    // 8を通過
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_OFF)
    // 3を通過
    WAIT_SENSOR(3)
    //WAIT_TIME(1000)
    ACCEL(50)      
    SPEED(SPEED_LOW)
    WAIT_TIME(1000)
    ACCEL(20) 
    SPEED(40)
    // 2を通過　２番線へ到着
    WAIT_SENSOR(1)
    //WAIT_TIME(500)
    ACCEL(5)      
    SPEED(SPEED_STOP)
  
    // 停車
    WAIT_TIME(5000)

    // end of scenario
    INVALID_DATA()
};


Type_Scenario gbScenario_1[] ={
    // 初期化
    WAIT_TIME(1000)
    //WAIT_SENSOR(2)
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    WAIT_TIME(1000)

    // ポイント位置初期化
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_1(POINT_DIRECTION_RIGHT)

    POINT_2(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_2(POINT_DIRECTION_RIGHT)

    // end of scenario
    INVALID_DATA()
};

Type_Scenario gbScenario_2[] ={
    // 初期化
    WAIT_TIME(1000)
    //WAIT_SENSOR(2)
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    WAIT_TIME(1000)

    // ポイント位置初期化
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_1(POINT_DIRECTION_RIGHT)

    POINT_2(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_2(POINT_DIRECTION_RIGHT)

    // end of scenario
    INVALID_DATA()
};

Type_Scenario gbScenario_3[] ={
    // 初期化
    WAIT_TIME(1000)
    //WAIT_SENSOR(2)
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    WAIT_TIME(1000)

    // ポイント位置初期化
    POINT_1(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_1(POINT_DIRECTION_RIGHT)

    POINT_2(POINT_DIRECTION_LEFT)
    WAIT_TIME(2000)
    POINT_2(POINT_DIRECTION_RIGHT)

    // end of scenario
    INVALID_DATA()
};


void initScenarioParams(void){
  int i;

  // select scenario data array
  if(gbSystem.scenario_number == 0)
  {
    gbScenario = gbScenario_0;
    gbSystem.scenario_counter_max = (sizeof(gbScenario_0) / sizeof(Type_Scenario));
    gbSystem.scenario_counter = 0;
  }
  else if(gbSystem.scenario_number == 1)
  {
    gbScenario = gbScenario_1;
    gbSystem.scenario_counter_max = (sizeof(gbScenario_1) / sizeof(Type_Scenario));
    gbSystem.scenario_counter = 0;
  }
  else if(gbSystem.scenario_number == 2)
  {
    gbScenario = gbScenario_2;
    gbSystem.scenario_counter_max = (sizeof(gbScenario_2) / sizeof(Type_Scenario));
    gbSystem.scenario_counter = 0;
  }
  else // if(gbSystem.scenario_number == 3)
  {
    gbScenario = gbScenario_3;
    gbSystem.scenario_counter_max = (sizeof(gbScenario_3) / sizeof(Type_Scenario));
    gbSystem.scenario_counter = 0;
  }
}

// get sensor no
int isSensorActive(int sensor_id)
{
  //int active=false;
  
  switch(sensor_id)
  {
    case 1: if((gbHC166Data[HC166_SENSOR] & BITMASK_1) == 0) { return true; } break;
    case 2: if((gbHC166Data[HC166_SENSOR] & BITMASK_2) == 0) { return true; } break;
    case 3: if((gbHC166Data[HC166_SENSOR] & BITMASK_3) == 0) { return true; } break;
    case 4: if((gbHC166Data[HC166_SENSOR] & BITMASK_4) == 0) { return true; } break;
    case 5: if((gbHC166Data[HC166_SENSOR] & BITMASK_5) == 0) { return true; } break;
    case 6: if((gbHC166Data[HC166_SENSOR] & BITMASK_6) == 0) { return true; } break;
    case 7: if((gbHC166Data[HC166_SENSOR] & BITMASK_7) == 0) { return true; } break;
    case 8: if((gbHC166Data[HC166_SENSOR] & BITMASK_8) == 0) { return true; } break;
    default: break;
  }

  //uintTempValue =  active; //gbHC166Data[HC166_SENSOR] & BITMASK_2;

  return false;
}

void HandleAutoDriveScenario(void){
  // auto drive
  //int numScenarioCounter = (sizeof(gbScenario) / sizeof(Type_Scenario));
  if(gbSystem.scenario_counter>=gbSystem.scenario_counter_max){
    if(gbSystem.isEndless==true){
      gbSystem.scenario_counter=0;
    }
    else{
      // goto Manual Mode
      gbSystem.mode = SYSTEM_MODE_MANUAL;
      gbSystem.scenario_counter=0;
      flagDrawTFT = true;
    }
    return;
  }
  // invalid
  if(gbScenario[gbSystem.scenario_counter].trigger == SCN_TRIGGER_INVALID){
      gbSystem.scenario_counter++;
      return;    
  }
  // wait for sensor active
  if(gbScenario[gbSystem.scenario_counter].trigger == SCN_TRIGGER_SENSOR)
  {
    //uintTempValue =isSensorActive(gbScenario[gbSystem.scenario_counter].param_0);
    if(isSensorActive(gbScenario[gbSystem.scenario_counter].param_0)==true){
      gbSystem.scenario_counter++;
      flagDrawTFT = true;
      return;
    }
  }
  // wait for timer
  if(gbScenario[gbSystem.scenario_counter].trigger == SCN_TRIGGER_MSEC){
    gbSystem.wait_timer++;
    if(gbSystem.wait_timer > gbScenario[gbSystem.scenario_counter].param_0){
      gbSystem.scenario_counter++;
      gbSystem.wait_timer = 0;
      flagDrawTFT = true;
      return;
    }
  }
  // no trigger
  if(gbScenario[gbSystem.scenario_counter].trigger == SCN_TRIGGER_NO){
    // Point1
    if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_P1){
      if(gbScenario[gbSystem.scenario_counter].param_1 == POINT_DIRECTION_LEFT){
        gbPoint[0].direction = POINT_DIRECTION_LEFT;
        gbSignal[0].status = SIGNAL_STATUS_STOP;
      }
      else if(gbScenario[gbSystem.scenario_counter].param_1 == POINT_DIRECTION_RIGHT){
        gbPoint[0].direction = POINT_DIRECTION_RIGHT;
        gbSignal[0].status = SIGNAL_STATUS_GO;
      }
      gbIsUiChanged = true;
      gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      updateButtonLedStatus();
      return;
    }
    // Point2
    else if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_P2){
      if(gbScenario[gbSystem.scenario_counter].param_1 == POINT_DIRECTION_LEFT){
        gbPoint[1].direction = POINT_DIRECTION_LEFT;
        gbSignal[1].status = SIGNAL_STATUS_GO;
      }
      else if(gbScenario[gbSystem.scenario_counter].param_1 == POINT_DIRECTION_RIGHT){
        gbPoint[1].direction = POINT_DIRECTION_RIGHT;
        gbSignal[1].status = SIGNAL_STATUS_STOP;
      }
      gbIsUiChanged = true;
      gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      updateButtonLedStatus();
      return;
    }
    // train direction
    else if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_TR_DIR){
      if(gbScenario[gbSystem.scenario_counter].param_1 == TRAIN_DIRECTION_CLOCKWISE){
        gbTrain.direction = TRAIN_DIRECTION_CLOCKWISE;
        gbSignal[2].status = SIGNAL_STATUS_GO;
      }
      else if(gbScenario[gbSystem.scenario_counter].param_1 == TRAIN_DIRECTION_COUNTERCLOCKWISE){
        gbTrain.direction = TRAIN_DIRECTION_COUNTERCLOCKWISE;
        gbSignal[2].status = SIGNAL_STATUS_STOP;
      }
      gbIsUiChanged = true;
      gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      updateButtonLedStatus();
      return;
     }
    // train speed
    else if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_SPEED){
      gbTrain.targetSpeed = gbScenario[gbSystem.scenario_counter].param_1;
      gbIsUiChanged = true;
      gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      updateButtonLedStatus();
      return;
     }
    // speed acceleration
    else if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_ACCEL){
      gbTrain.accelTime = gbScenario[gbSystem.scenario_counter].param_1;
      //gbIsUiChanged = true;
      //gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      //updateButtonLedStatus();
      return;
     }    // crossing
    else if(gbScenario[gbSystem.scenario_counter].param_0 == SCN_NOTG_CROSS){
      gbCrossing[0].status = gbScenario[gbSystem.scenario_counter].param_1;
      gbIsUiChanged = true;
      gbIsHC595Update = true;
      gbSystem.scenario_counter++;
      updateButtonLedStatus();
      return;
    }
  }
}

