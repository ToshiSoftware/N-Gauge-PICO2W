/*
  inthandler.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  1ms interval interrupt handler functions
  (c)Toshi 2025
*/

#include "MYTCS_PICO2.h"

// called every 1 msec
int tmpAlt = 0;
bool My1msIntHandler(struct repeating_timer *t) {
  // Timing test
  digitalWrite(PIN_TEST_1ms_INTRPT, HIGH);
  // counter
  gbIntCounter++;
  //if(gbIntCounter>TIMER_COUNTER_MAX) gbIntCounter=0; // 100sec timer
  
  // 1ms real clock
  tmpAlt = 1-tmpAlt;
  if(tmpAlt){digitalWrite(PIN_REALCLOCK_1ms, HIGH);}
  else{digitalWrite(PIN_REALCLOCK_1ms, LOW);}

  // called every
  PriorityTask();

  // Touch Screen
  if(gbIntCounter % 20 == 0) {
    flagCheckTouchPanel = true;
  }

  // Cross blink
  if(gbIntCounter % 800 == 0)
  {
    flagAlt800 = 1-flagAlt800;
    // only when cross lamp is active, redraw TFT
    if(tftCross[0].isActive == true){
      flagDrawTFT = true;
    }
  }

  // // Point blink
  // if(gbIntCounter % 500 == 0)
  // {
  //   flagAlt500 = 1-flagAlt500;
  //   flagDrawTFT = true;
  // }

  // Timing test
  digitalWrite(PIN_TEST_1ms_INTRPT, LOW);
  return true;
}

void PriorityTask(void){

  // serial in
  MySerialInput();  // read HC166 and then put into gbHC166Data
  setSensorTFTFromHC166();
  // serial in handler
  HandleLedButton(); // read gbHC166Data[1], and change gbBtLed[i].status: if changed, gbIsHC595Update = true;

  // drive devices
  HandleSignal(); // read gbSignal[i].status, if changed goto signal sequence
  DrivePoint(); // read gbPoint[i].direction, if changed drive Point
  DriveCrossing(); // read gbCrossing[i].status, if changed drive Crossing
  
  // VR
  if(gbSystem.mode == SYSTEM_MODE_MANUAL){
    GetSpeedVr();  // read Analog pin, and then set gbTrain.speed
  }
  else{
    if(gbTrain.accelTime>1000 || gbTrain.accelTime<=0){
      gbTrain.accelTime = 10;
    }
    if(gbTrain.speed > gbTrain.targetSpeed){
      if(gbIntCounter % (unsigned int)gbTrain.accelTime == 0){
        gbTrain.speed--;
        gbTrain.prevSpeed = gbTrain.speed;
        gbIsHC595Update = true;
      }
    }
    else if(gbTrain.speed < gbTrain.targetSpeed){
      if(gbIntCounter % (unsigned int)gbTrain.accelTime == 0){
        gbTrain.speed++;
        gbTrain.prevSpeed = gbTrain.speed;
        gbIsHC595Update = true;
      }
    }
    HandleAutoDriveScenario();
  }

  // Serial out
  if(gbIsHC595Update==true){
    // UI
    updateParamsFromLcdButton(); // read gbBtLed[i].status and change Point/Crossing/Signal/ButtonLED status
    // set power pack driver
    updatePowerPack();
    // output serial to HC595
    updateHC595();
    // RESET change flag
    gbIsHC595Update = false;
  }

}