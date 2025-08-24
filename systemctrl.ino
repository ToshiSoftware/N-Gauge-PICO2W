/*
  systemctrl.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  Train PWM power, serial communication, items on the layout, hardware UI on the board
  (c)Toshi 2025
*/

#include "MYTCS_PICO2.h"

#define USE_8_BUTTONS 1

/*
  LED BUTTON map
  1) #ifdef USE_8_BUTTONS
    0: Point 1, left
    1: Point 1, right
    2: Point 2, left
    3: Point 2, right
    4: Crossing, Off
    5: Crossing, On
    6: Direction, CLOCKWISE
    7: Direction, COUNTERCLOCKWISE
  2) #ifndef USE_8_BUTTONS
    0: Point 1, left/right alternatively
    1: Point 2, left/right alternatively
    2: Crossing, ON/OFF alternatively
    3: Direction, CLOCKWISE/COUNTERCLOCKWISE alternatively
*/


// Power pack control using PWM
void updatePowerPack(void){
    if(gbTrain.direction==TRAIN_DIRECTION_CLOCKWISE){
      pwm_set_chan_level(slice_num, PWM_CHAN_A, 256-gbTrain.speed); // STOP = H (duty:100%)
      pwm_set_chan_level(slice_num, PWM_CHAN_B, 255);

    }
    else{
      pwm_set_chan_level(slice_num, PWM_CHAN_A, 255); // STOP = H (duty:100%)
      pwm_set_chan_level(slice_num, PWM_CHAN_B, 255-gbTrain.speed);
    }
}


// Serial out to HC595
void MySerialOutput( void )
{
  int chip;
  int i;
  unsigned int shifted_data=0;

  // initial
  digitalWrite(PIN_595_DAT, HIGH);
  digitalWrite(PIN_595_CLK, HIGH);
  digitalWrite(PIN_595_STB, HIGH); 
  // prepare to latch ( change CS to LOW )
  digitalWrite(PIN_595_STB, LOW);
  digitalWrite(PIN_595_STB, LOW);
  digitalWrite(PIN_595_STB, LOW);

  // Send data
  for( chip=0; chip<NUM_HC595; chip++)
  {
    shifted_data = gbHC595Data[NUM_HC595-chip-1];
    for(i=0; i<8; i++)
    {
      // data set
      if(shifted_data & 0x80) digitalWrite(PIN_595_DAT, HIGH);
      else                    digitalWrite(PIN_595_DAT, LOW);
      shifted_data = (shifted_data << 1);
      // shift
      digitalWrite(PIN_595_CLK, LOW);
      digitalWrite(PIN_595_CLK, HIGH);
    }
    digitalWrite(PIN_595_CLK, HIGH);
    digitalWrite(PIN_595_CLK, HIGH);
    digitalWrite(PIN_595_CLK, HIGH);
  }
  // latch ( change CS to HIGH )
  digitalWrite(PIN_595_STB, LOW);
  digitalWrite(PIN_595_STB, LOW);
  digitalWrite(PIN_595_STB, LOW);
  digitalWrite(PIN_595_STB, HIGH);
  // termination
  digitalWrite(PIN_595_DAT, HIGH);
}


void MySerialInput( void )
{
  int i, chip;

  for(i=0; i<NUM_HC166; i++) gbHC166Data[i]=0;

  // loada data into HC166
  digitalWrite(PIN_166_STB, HIGH);
  digitalWrite(PIN_166_CLK, LOW);

  digitalWrite(PIN_166_STB, LOW);  // latch mode
  digitalWrite(PIN_166_CLK, HIGH); // latch data
  digitalWrite(PIN_166_STB, HIGH); // shift mode

  for(chip=0; chip<NUM_HC166; chip++){
    // read form G to A
    for (i = 0; i < 8; i++) {
      // bit shift
      gbHC166Data[chip] = gbHC166Data[chip] << 1;
      gbHC166Data[chip] = gbHC166Data[chip] & 0xfe;  // mask LSB with 0
      // enable CLK
      digitalWrite(PIN_166_CLK, LOW); // HC165 shift and output data
      // read sdi
      if (digitalRead(PIN_166_DAT) == HIGH) { // read data
        gbHC166Data[chip] = gbHC166Data[chip] | 0x01;
      }
      // disable CLK
      digitalWrite(PIN_166_CLK, HIGH); 
    }
  }
}

void HandleLedButton(void) // called every 1ms
{
  int i;
  int maskBit=0x01;
  for (i = 0; i < NUM_LEDBUTTON; i++) {
    if((gbHC166Data[HC166_LEDBT] & maskBit) !=0)
    {
      gbBtLed[i].status = false;
    }
    else
    {
      gbBtLed[i].status = true;
    }
    maskBit = maskBit<<1;
    // is updated
    if(gbBtLed[i].prevStatus != gbBtLed[i].status ){
      gbIsHC595Update = true;
      gbBtLed[i].prevStatus = gbBtLed[i].status;
    }
  }
}

#define VR_TORR
void GetSpeedVr( void )
{
  int potValue;
  int def;
  // Analog in
  potValue = analogRead( PIN_ANALOG_VR ); // potValue 0-1023
  gbTrain.tempSpeed = potValue/4;
  gbTrain.tempSpeed = gbTrain.tempSpeed; // * gbTrain.maxSpeed / 255;
  if(gbTrain.tempSpeed<TRAIN_SPEED_MIN)   gbTrain.tempSpeed=0;
  if(gbTrain.tempSpeed>255-TRAIN_SPEED_MIN) gbTrain.tempSpeed=255;

  def = abs(gbTrain.tempSpeed - gbTrain.prevSpeed);
  if(def>=TRAIN_SPEED_TOLERANCE){
    gbIsHC595Update = true;
    gbTrain.speed = gbTrain.tempSpeed;
    gbTrain.prevSpeed = gbTrain.speed;
  }
}

// Signal handling
void HandleSignal(void)
{
  int i;
  for (i = 0; i < NUM_SIGNAL; i++) {
    // flags
    if( gbSignal[i].prevStatus != gbSignal[i].status){
      gbSignal[i].prevStatus = gbSignal[i].status;
      gbSignal[i].counter=0;
      gbSignal[i].isChanging = true;
    }
    
    if(gbSignal[i].isChanging == true)
    {
      // 
      if(gbSignal[i].counter==0) 
      {
        gbSignal[i].color =  SIGNAL_COLOR_YELLOW;
        gbIsHC595Update = true;
        flagDrawTFT = true;
      }
      if(gbSignal[i].counter==SIGNAL_YELLOW_DURATION)
      {
        if(gbSignal[i].status == SIGNAL_STATUS_STOP ){
          gbSignal[i].color =  SIGNAL_COLOR_RED;
          flagDrawTFT = true;
        }
        else{
          gbSignal[i].color =  SIGNAL_COLOR_GREEN;         
          flagDrawTFT = true;
        }
        gbIsHC595Update = true;
        gbSignal[i].isChanging = false;
      }
      // internal counter ++
      gbSignal[i].counter++;
    }
  }

}

void DrivePoint(void){
  int i;
  for (i = 0; i < NUM_POINT; i++) {
    // flags
    if( gbPoint[i].prevDirection != gbPoint[i].direction){
      gbPoint[i].prevDirection = gbPoint[i].direction;
      gbPoint[i].onCounter=0;
      gbPoint[i].isDriving = true;
    }
    //
    if(gbPoint[i].isDriving == true){
      if(gbPoint[i].onCounter==0)
      {
        if(gbPoint[i].direction==POINT_DIRECTION_LEFT){
          gbPoint[i].driveM1 = false;
          gbPoint[i].driveM2 = true;
        }
        else{
          gbPoint[i].driveM1 = true;
          gbPoint[i].driveM2 = false;          
        }
        // serial out
        gbIsHC595Update = true;
      }
      if(gbPoint[i].onCounter==POINT_DRIVE_DURATION){
        gbPoint[i].isDriving = false;
        // do not drive coils
        gbPoint[i].driveM1 = false; // true=STOP_MODE false=Hi-Z
        gbPoint[i].driveM2 = false; 
        // serial out
        gbIsHC595Update = true;
      }
      gbPoint[i].onCounter++;
    }
  }
}

void DriveCrossing(void){
  int i;
  for (i = 0; i < NUM_CROSSING; i++) {
    // flags
    if( gbCrossing[i].status == CROSSING_STATUS_ON){
      if(gbCrossing[i].counter==0)
      {
        gbCrossing[i].led1 = true;
        gbCrossing[i].led2 = false;
        gbIsHC595Update = true;
      }
      else if(gbCrossing[i].counter==CROSSING_ON_DURATION)
      {
        gbCrossing[i].led1 = false;
        gbCrossing[i].led2 = true;
        gbIsHC595Update = true;
      }

      gbCrossing[i].counter++;
      if(gbCrossing[i].counter > CROSSING_ON_DURATION*2){
        gbCrossing[i].counter = 0;
      }
    }
    // OFF
    if( gbCrossing[i].prevStatus != gbCrossing[i].status){
      if(gbCrossing[i].status == CROSSING_STATUS_OFF){
          gbCrossing[i].led1 = false;
          gbCrossing[i].led2 = false;
          gbIsHC595Update = true;
      }
      gbCrossing[i].prevStatus = gbCrossing[i].status;
    }
  }
}



/*
  HC595 map
    0: Button LED 1-8
    1: Point 1, 2, 3, 4
    2: Signal 1, 2, Crossing 1
    3: Signal 3, 4, Crossing 2
*/

void updateHC595(void){
  int i;
  unsigned int bitMask;

  // 0: Button LED 1-8
  gbHC595Data[HC595_BTLED] = 0;
  bitMask=0x00000001;
  for(i=0; i<NUM_LEDBUTTON; i++){
    if(gbBtLed[i].led == true){
      gbHC595Data[HC595_BTLED] = gbHC595Data[HC595_BTLED] | bitMask;
    }
    bitMask = (bitMask << 1);
  }

  // 1:Point 1, 2, 3, 4
  {
    gbHC595Data[HC595_POINT] = 0;
    bitMask=0x00000001;

    for(i=0; i<NUM_POINT; i++){
      if(gbPoint[i].driveM1 == true){
        gbHC595Data[HC595_POINT] = gbHC595Data[HC595_POINT]|bitMask;
      }
      else{
        gbHC595Data[HC595_POINT] = gbHC595Data[HC595_POINT]&(~bitMask);
      }
      bitMask = bitMask << 1;
      if(gbPoint[i].driveM2 == true){
        gbHC595Data[HC595_POINT] = gbHC595Data[HC595_POINT]|bitMask;
      }
      else{
        gbHC595Data[HC595_POINT] = gbHC595Data[HC595_POINT]&(~bitMask);
      }
      bitMask = bitMask << 1;
    }
  }

  // 2:Signal 1
  gbHC595Data[HC595_SIG1] = 0;
  if(gbSignal[0].color == SIGNAL_COLOR_RED){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_1);
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_2;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_3;
  }
  else if(gbSignal[0].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_1;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_2);
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_3;
  }
  else{
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_1;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_2;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_3);
  }

  // 3:Signal 2
  if(gbSignal[1].color == SIGNAL_COLOR_RED){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_4);
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_5;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_6;
  }
  else if(gbSignal[1].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_4;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_5);
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_6;
  }
  else{
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_4;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_5;
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_6);
  }

  // 3:Crossing 1
  if(gbCrossing[0].led1 == true){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_7;
  }
  else{
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_7);
  }
  if(gbCrossing[0].led2 == true){
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]|BITMASK_8;
  }
  else{
    gbHC595Data[HC595_SIG1] = gbHC595Data[HC595_SIG1]&(~BITMASK_8);
  }

  // 4:Signal 3
  gbHC595Data[HC595_SIG2] = 0;
  if(gbSignal[2].color == SIGNAL_COLOR_RED){
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_1);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_2;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_3;
  }
  else if(gbSignal[2].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_1;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_2);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_3;
  }
  else{
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_1;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_2;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_3);
  }

  // Street light
  if(gbSystem.streetLight[0]==false){
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_4);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_5);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_6);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_7);
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]&(~BITMASK_8);
  }
  else{
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_4;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_5;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_6;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_7;
    gbHC595Data[HC595_SIG2] = gbHC595Data[HC595_SIG2]|BITMASK_8;
  }

  MySerialOutput();
}

void updateButtonLedStatus(void){
  #ifdef USE_8_BUTTONS
  if(gbPoint[0].direction == POINT_DIRECTION_LEFT){
    gbBtLed[0].led = true;
    gbBtLed[1].led = false;    
  }
  else{
    gbBtLed[0].led = false;
    gbBtLed[1].led = true;    
  }
  if(gbPoint[1].direction == POINT_DIRECTION_LEFT){
    gbBtLed[2].led = true;
    gbBtLed[3].led = false;    
  }
  else{
    gbBtLed[2].led = false;
    gbBtLed[3].led = true;    
  }
  #else
  if(gbPoint[0].direction == POINT_DIRECTION_LEFT){
    gbBtLed[0].led = true;
  }
  else{
    gbBtLed[0].led = false;
  }
  if(gbPoint[1].direction == POINT_DIRECTION_LEFT){
    gbBtLed[1].led = true;
  }
  else{
    gbBtLed[1].led = false;
  }
  #endif

  #ifdef USE_8_BUTTONS
  if(gbCrossing[0].status == CROSSING_STATUS_OFF){
    gbBtLed[4].led = true;
    gbBtLed[5].led = false;    
  }
  else{
    gbBtLed[4].led = false;
    gbBtLed[5].led = true;    
  }

  if(gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE){
    gbBtLed[6].led = true;
    gbBtLed[7].led = false;
  }
  else{
    gbBtLed[6].led = false;
    gbBtLed[7].led = true;
  }
  #else
  if(gbCrossing[0].status == CROSSING_STATUS_ON){
    gbBtLed[2].led = true;
  }
  else{
    gbBtLed[2].led = false;
  }

  if(gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE){
    gbBtLed[3].led = true;
  }
  else{
    gbBtLed[3].led = false;
  }
  #endif
}


void updateParamsFromLcdButton(void){
  #ifdef USE_8_BUTTONS
  // Point 1
  if(gbBtLed[0].status==true && gbBtLed[1].status==false){
    gbPoint[0].direction = POINT_DIRECTION_LEFT;
    // change signal
    gbSignal[0].status = SIGNAL_STATUS_STOP;
  }
  if(gbBtLed[0].status==false && gbBtLed[1].status==true){
    gbPoint[0].direction = POINT_DIRECTION_RIGHT;
    // change signal
    gbSignal[0].status = SIGNAL_STATUS_GO;
  }
  // if button status changed, redraw TFT
  if(gbBtLed[0].status==true && gbBtLed[0].prevBt==false){
    flagDrawTFT = true;
  }
  gbBtLed[0].prevBt = gbBtLed[0].status;
  if(gbBtLed[1].status==true && gbBtLed[1].prevBt==false){
    flagDrawTFT = true;
  }  
  gbBtLed[1].prevBt = gbBtLed[1].status;

  // Point 2
  if(gbBtLed[2].status==true && gbBtLed[3].status==false){
    gbPoint[1].direction = POINT_DIRECTION_LEFT;
    // change signal
    gbSignal[1].status = SIGNAL_STATUS_GO;
  }
  if(gbBtLed[2].status==false && gbBtLed[3].status==true){
    gbPoint[1].direction = POINT_DIRECTION_RIGHT;
    // change signal
    gbSignal[1].status = SIGNAL_STATUS_STOP;
  }
  // if button status changed, redraw TFT
  if(gbBtLed[2].status==true && gbBtLed[2].prevBt==false){
    flagDrawTFT = true;
  }
  gbBtLed[2].prevBt = gbBtLed[2].status;
  if(gbBtLed[3].status==true && gbBtLed[3].prevBt==false){
    flagDrawTFT = true;
  }  
  gbBtLed[3].prevBt = gbBtLed[3].status;

  #else
  // PT1
  if(gbBtLed[0].status==true && gbBtLed[0].prevBt==false){
    if(gbPoint[0].direction == POINT_DIRECTION_LEFT){
      gbPoint[0].direction = POINT_DIRECTION_RIGHT;
      gbSignal[0].status = SIGNAL_STATUS_GO;
    }
    else{
      gbPoint[0].direction = POINT_DIRECTION_LEFT;
      gbSignal[0].status = SIGNAL_STATUS_STOP;      
    }
    flagDrawTFT = true;
  }
  gbBtLed[0].prevBt = gbBtLed[0].status;

  if(gbBtLed[1].status==true && gbBtLed[1].prevBt==false){
    if(gbPoint[1].direction == POINT_DIRECTION_LEFT){
      gbPoint[1].direction = POINT_DIRECTION_RIGHT;
      gbSignal[1].status = SIGNAL_STATUS_STOP;
    }
    else{
      gbPoint[1].direction = POINT_DIRECTION_LEFT;
      gbSignal[1].status = SIGNAL_STATUS_GO;      
    }
    flagDrawTFT = true;
  }
  gbBtLed[1].prevBt = gbBtLed[1].status;
  #endif

  #ifdef USE_8_BUTTONS
  // crossing
  if(gbBtLed[4].status==true && gbBtLed[5].status==false){
    gbCrossing[0].status = CROSSING_STATUS_OFF;
  }
  if(gbBtLed[4].status==false && gbBtLed[5].status==true){
    gbCrossing[0].status = CROSSING_STATUS_ON;
  }  
  // if button status changed, redraw TFT
  if(gbBtLed[4].status==true && gbBtLed[4].prevBt==false){
    flagDrawTFT = true;
  }
  gbBtLed[4].prevBt = gbBtLed[4].status;
  if(gbBtLed[5].status==true && gbBtLed[5].prevBt==false){
    flagDrawTFT = true;
  }  
  gbBtLed[5].prevBt = gbBtLed[5].status;

  // train direction
  if(gbBtLed[6].status==true && gbBtLed[7].status==false){
    gbTrain.direction = TRAIN_DIRECTION_CLOCKWISE;
    // change signal
    gbSignal[2].status = SIGNAL_STATUS_GO;
  }
  if(gbBtLed[6].status==false && gbBtLed[7].status==true){
    gbTrain.direction = TRAIN_DIRECTION_COUNTERCLOCKWISE;
    // change signal
    gbSignal[2].status = SIGNAL_STATUS_STOP;
  }
  // if button status changed, redraw TFT
  if(gbBtLed[6].status==true && gbBtLed[6].prevBt==false){
    flagDrawTFT = true;
  }
  gbBtLed[6].prevBt = gbBtLed[6].status;
  if(gbBtLed[7].status==true && gbBtLed[7].prevBt==false){
    flagDrawTFT = true;
  }  
  gbBtLed[7].prevBt = gbBtLed[7].status;

  #else
  if(gbBtLed[2].status==true && gbBtLed[2].prevBt==false){
    if(gbCrossing[0].status == CROSSING_STATUS_OFF){
      gbCrossing[0].status = CROSSING_STATUS_ON;
    }
    else{
      gbCrossing[0].status = CROSSING_STATUS_OFF;
    }
    flagDrawTFT = true;
  }
  gbBtLed[2].prevBt = gbBtLed[2].status;

  if(gbBtLed[3].status==true && gbBtLed[3].prevBt==false){
    if(gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE){
      gbTrain.direction = TRAIN_DIRECTION_COUNTERCLOCKWISE;
      gbSignal[2].status = SIGNAL_STATUS_STOP;
    }
    else{
      gbTrain.direction = TRAIN_DIRECTION_CLOCKWISE;
      gbSignal[2].status = SIGNAL_STATUS_GO;
    }
    flagDrawTFT = true;
  }
   gbBtLed[3].prevBt = gbBtLed[3].status;
  #endif
  //
  updateButtonLedStatus();
}
