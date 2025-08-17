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
  // counter
  gbIntCounter++;
  //if(gbIntCounter>TIMER_COUNTER_MAX) gbIntCounter=0; // 100sec timer
  
  // test interrup interval
  tmpAlt = 1-tmpAlt;

  if(tmpAlt){digitalWrite(PIN_INTR_1ms, HIGH);}
  else{digitalWrite(PIN_INTR_1ms, LOW);}

  // original system task
  PriorityTask();

  if(gbIntCounter % 5){
    UsualTask();
  }

  // new system task for PICO2
  if(gbIntCounter % 500 == 0) {
    testLED = 1 - testLED;
  }

  // Touch Screen
  if(gbIntCounter % 20 == 0) {
    flagIsTouched = true;
  }

  // Cross blink
  if(gbIntCounter % 800 == 0)
  {
    flagAlt800 = 1-flagAlt800;
    flagDrawTFT = true;
  }

  // Point blink
  if(gbIntCounter % 500 == 0)
  {
    flagAlt500 = 1-flagAlt500;
    flagDrawTFT = true;
  }

  return true;
}

void UsualTask(void){
  if(gbIsUiChanged==true){
    // UI
    updateParamsFromLcdButton(); // read gbLedButton[i].status and change Point/Crossing/Signal/ButtonLED status

    // set power pack driver
    updatePowerPack();

    // RESET change flag
    gbIsUiChanged = false;
  }

  // Serial out
  if(gbIsHC595Update==true){
    updateHC595();
    // RESET change flag
    gbIsHC595Update = false;
    gbIsUiChanged=true;
  }
}

void PriorityTask(void){

  // serial in
  MySerialInput();  // read HC166 and then put into gbHC166Data
  setSensorTFTFromHC166();
  // serial in handler
  HandleLedButton(); // read gbHC166Data[1], and change gbLedButton[i].status: if changed, gbIsUiChanged = true;

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
        gbIsUiChanged = true;
        gbIsHC595Update = true;
      }
    }
    else if(gbTrain.speed < gbTrain.targetSpeed){
      if(gbIntCounter % (unsigned int)gbTrain.accelTime == 0){
        gbTrain.speed++;
        gbTrain.prevSpeed = gbTrain.speed;
        gbIsUiChanged = true;
        gbIsHC595Update = true;
      }
    }
    HandleAutoDriveScenario();
  }
}