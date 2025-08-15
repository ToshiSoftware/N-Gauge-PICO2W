/*
  systemctrl.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  Train PWM power, serial communication, items on the layout, hardware UI on the board
  (c)Toshi 2025
*/

#include "MYTCS_PICO2.h"

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

void HandleCtrlButton(void)
{
  int i;
  int maskBit=0x01;
  for (i = 0; i < NUM_CTRLBUTTON; i++) {
    if((gbHC166Data[0] & maskBit) !=0)
    {
      gbCtrlButton[i].status = false;
    }
    else
    {
      gbCtrlButton[i].status = true;
    }
    maskBit = maskBit<<1;
    // is updated
    if(gbCtrlButton[i].prevStatus != gbCtrlButton[i].status ){
      gbIsUiChanged = true;
      gbCtrlButton[i].prevStatus = gbCtrlButton[i].status;
    }
  }
}

void HandleLedButton(void)
{
  int i;
  int maskBit=0x01;
  for (i = 0; i < NUM_LEDBUTTON; i++) {
    if((gbHC166Data[1] & maskBit) !=0)
    {
      gbLedButton[i].status = false;
    }
    else
    {
      gbLedButton[i].status = true;
    }
    maskBit = maskBit<<1;
    // is updated
    if(gbLedButton[i].prevStatus != gbLedButton[i].status ){
      gbIsUiChanged = true;
      gbLedButton[i].prevStatus = gbLedButton[i].status;
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
    gbIsUiChanged = true;
    gbIsHC595Update = true;
    gbTrain.speed = gbTrain.tempSpeed;
    gbTrain.prevSpeed = gbTrain.speed;
  }
}


// handle rotary encoder
int cw[] = {1, 3, 0, 2};
int ccw[] = {2, 0, 3, 1};

void HandleEncoder(void) {
  int i;
  int pinValue;

  // set encoder bit
  gbEncoder[0].B = 0;
  gbEncoder[0].A = 0;
  gbEncoder[1].B = 0;
  gbEncoder[1].A = 0;

  // get HC166 data
  if (gbHC166Data[0] & 0x10) gbEncoder[0].B = 1;
  if (gbHC166Data[0] & 0x20) gbEncoder[0].A = 1;
  if (gbHC166Data[0] & 0x40) gbEncoder[1].B = 1;
  if (gbHC166Data[0] & 0x80) gbEncoder[1].A = 1;

  for (i = 0; i < NUM_ENCODER; i++) {
    // if encoder A/B is changed..
    pinValue = gbEncoder[i].A + gbEncoder[i].B * 2;
    pinValue %= 4;
    if (gbEncoder[i].prev_pinValue != pinValue) {
      if (pinValue == cw[gbEncoder[i].prev_pinValue]) gbEncoder[i].value++;
      if (pinValue == ccw[gbEncoder[i].prev_pinValue]) gbEncoder[i].value--;
      gbEncoder[i].prev_pinValue = pinValue;
    }

    // value for app
    int dif;
    dif = gbEncoder[i].value - gbEncoder[i].prev_appValue;
    if(dif>ENCODER_TOLERANCE){
      gbIsUiChanged = true;
      gbEncoder[i].rotation_dir = ROTATION_PLUS;
      gbEncoder[i].prev_appValue = gbEncoder[i].value;
    }
    else if(dif<-ENCODER_TOLERANCE){
      gbIsUiChanged = true;
      gbEncoder[i].rotation_dir = ROTATION_MINUS;    
      gbEncoder[i].prev_appValue = gbEncoder[i].value;
    }
    else{
      gbEncoder[i].rotation_dir = ROTATION_STOP;    
    }
  }
}


void updateEncoderValue(void)
{
    // encoder in test
    if(gbEncoder[0].rotation_dir == ROTATION_PLUS){
      gbEncoder[0].rotation_dir = ROTATION_STOP;
      gbSystem.scenario_number++;
      if(gbSystem.scenario_number >= NUM_SCENARIO_ID) gbSystem.scenario_number=0;
    }
    else if(gbEncoder[0].rotation_dir == ROTATION_MINUS){
      gbEncoder[0].rotation_dir = ROTATION_STOP;
      gbSystem.scenario_number--;
      if(gbSystem.scenario_number < 0) gbSystem.scenario_number=NUM_SCENARIO_ID;
    }
    // encoder in test
    if(gbEncoder[1].rotation_dir == ROTATION_PLUS){
      gbEncoder[1].rotation_dir = ROTATION_STOP;
      gbSystem.streetLight[0] = true;
    }
    else if(gbEncoder[1].rotation_dir == ROTATION_MINUS){
      gbEncoder[1].rotation_dir = ROTATION_STOP;
      gbSystem.streetLight[0] = false;
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
      }
      if(gbSignal[i].counter==SIGNAL_YELLOW_DURATION)
      {
        if(gbSignal[i].status == SIGNAL_STATUS_STOP ){
          gbSignal[i].color =  SIGNAL_COLOR_RED;
          gbIsHC595Update = true;
        }
        else{
          gbSignal[i].color =  SIGNAL_COLOR_GREEN;         
          gbIsHC595Update = true;
        }
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
    1: Slider/VR indicator 1-8
    2: Point 1, 2, 3, 4
    3: Signal 1, 2, Crossing 1
    4: Signal 3, 4, Crossing 2

*/

void updateHC595(void){
  int i;
  unsigned int bitMask;

  // 0: Button LED 1-8
  gbHC595Data[0] = 0;
  bitMask=0x00000001;
  for(i=0; i<NUM_LEDBUTTON; i++){
    if(gbLedButton[i].led == true){
      gbHC595Data[0] = gbHC595Data[0] | bitMask;
    }
    bitMask = (bitMask << 1);
  }

  // 1:Slider/VR indicator 1-8
  {
    int ledStep;

    ledStep = (gbTrain.speed * 8 / gbTrain.maxSpeed);
    if(ledStep<0) ledStep=0;
    if(ledStep>7) ledStep=7;

    gbHC595Data[1] = 0;
    bitMask=0x00000001;
    //if( gbTrain.speed != 0)
    {
      for(i=0; i<ledStep; i++){
        bitMask = bitMask << 1;
      }
      gbHC595Data[1] = gbHC595Data[1] | bitMask;
    }
  }

  // 2:Point 1, 2, 3, 4
  {
    gbHC595Data[2] = 0;
    bitMask=0x00000001;

    for(i=0; i<NUM_POINT; i++){
      if(gbPoint[i].driveM1 == true){
        gbHC595Data[2] = gbHC595Data[2]|bitMask;
      }
      else{
        gbHC595Data[2] = gbHC595Data[2]&(~bitMask);
      }
      bitMask = bitMask << 1;
      if(gbPoint[i].driveM2 == true){
        gbHC595Data[2] = gbHC595Data[2]|bitMask;
      }
      else{
        gbHC595Data[2] = gbHC595Data[2]&(~bitMask);
      }
      bitMask = bitMask << 1;
    }
  }

  // 3:Signal 1
  gbHC595Data[3] = 0;
  if(gbSignal[0].color == SIGNAL_COLOR_RED){
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_1);
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_2;
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_3;
  }
  else if(gbSignal[0].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_1;
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_2);
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_3;
  }
  else{
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_1;
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_2;
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_3);
  }

  // 3:Signal 2
  if(gbSignal[1].color == SIGNAL_COLOR_RED){
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_4);
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_5;
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_6;
  }
  else if(gbSignal[1].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_4;
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_5);
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_6;
  }
  else{
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_4;
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_5;
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_6);
  }

  // 3:Crossing 1
  if(gbCrossing[0].led1 == true){
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_7;
  }
  else{
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_7);
  }
  if(gbCrossing[0].led2 == true){
    gbHC595Data[3] = gbHC595Data[3]|BITMASK_8;
  }
  else{
    gbHC595Data[3] = gbHC595Data[3]&(~BITMASK_8);
  }

  // 4:Signal 3
  gbHC595Data[4] = 0;
  if(gbSignal[2].color == SIGNAL_COLOR_RED){
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_1);
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_2;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_3;
  }
  else if(gbSignal[2].color == SIGNAL_COLOR_YELLOW){
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_1;
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_2);
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_3;
  }
  else{
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_1;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_2;
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_3);
  }

  // Street light
  if(gbSystem.streetLight[0]==false){
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_4);
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_5);
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_6);
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_7);
    gbHC595Data[4] = gbHC595Data[4]&(~BITMASK_8);
  }
  else{
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_4;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_5;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_6;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_7;
    gbHC595Data[4] = gbHC595Data[4]|BITMASK_8;
  }

  MySerialOutput();
}


/*
  LED BUTTON map
    0: Point 1 <-
    1: Point 1 ->
    2: Point 2 <-
    3: Point 2 ->
    4: Crossing Off
    5: Crossing On
    6: Direction <-
    7: Direction ->
*/

void updateButtonLedStatus(void){
  if(gbPoint[0].direction == POINT_DIRECTION_LEFT){
    gbLedButton[0].led = true;
    gbLedButton[1].led = false;    
  }
  else{
    gbLedButton[0].led = false;
    gbLedButton[1].led = true;    
  }
  if(gbPoint[1].direction == POINT_DIRECTION_LEFT){
    gbLedButton[2].led = true;
    gbLedButton[3].led = false;    
  }
  else{
    gbLedButton[2].led = false;
    gbLedButton[3].led = true;    
  }

  if(gbCrossing[0].status == CROSSING_STATUS_OFF){
    gbLedButton[4].led = true;
    gbLedButton[5].led = false;    
  }
  else{
    gbLedButton[4].led = false;
    gbLedButton[5].led = true;    
  }

  if(gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE){
    gbLedButton[6].led = true;
    gbLedButton[7].led = false;
  }
  else{
    gbLedButton[6].led = false;
    gbLedButton[7].led = true;
  }
}

void updateParamsFromCtrlButton(void){
  // change system mode
  if(gbCtrlButton[0].status==true && gbCtrlButton[1].status==false){
    gbSystem.mode = SYSTEM_MODE_MANUAL;
    gbSystem.scenario_counter = 0;
  }
  if(gbCtrlButton[0].status==false && gbCtrlButton[1].status==true){
    gbSystem.mode = SYSTEM_MODE_AUTO;
    initScenarioParams();
  }
  // panic
  if(gbCtrlButton[2].status==true){
    gbSystem.isEndless = false;
  }
  // panic
  if(gbCtrlButton[3].status==true){
    gbSystem.isEndless = true;
  }
}

void updateParamsFromLcdButton(void){
  // Point 1
  if(gbLedButton[0].status==true && gbLedButton[1].status==false){
    gbPoint[0].direction = POINT_DIRECTION_LEFT;
    // change signal
    gbSignal[0].status = SIGNAL_STATUS_STOP;
  }
  if(gbLedButton[0].status==false && gbLedButton[1].status==true){
    gbPoint[0].direction = POINT_DIRECTION_RIGHT;
    // change signal
    gbSignal[0].status = SIGNAL_STATUS_GO;
  }  
  // Point 2
  if(gbLedButton[2].status==true && gbLedButton[3].status==false){
    gbPoint[1].direction = POINT_DIRECTION_LEFT;
    // change signal
    gbSignal[1].status = SIGNAL_STATUS_GO;
  }
  if(gbLedButton[2].status==false && gbLedButton[3].status==true){
    gbPoint[1].direction = POINT_DIRECTION_RIGHT;
    // change signal
    gbSignal[1].status = SIGNAL_STATUS_STOP;
  }  
  // crossing
  if(gbLedButton[4].status==true && gbLedButton[5].status==false){
    gbCrossing[0].status = CROSSING_STATUS_OFF;
  }
  if(gbLedButton[4].status==false && gbLedButton[5].status==true){
    gbCrossing[0].status = CROSSING_STATUS_ON;
  }  
  // train direction
  if(gbLedButton[6].status==true && gbLedButton[7].status==false){
    gbTrain.direction = TRAIN_DIRECTION_CLOCKWISE;
    // change signal
    gbSignal[2].status = SIGNAL_STATUS_GO;
  }
  if(gbLedButton[6].status==false && gbLedButton[7].status==true){
    gbTrain.direction = TRAIN_DIRECTION_COUNTERCLOCKWISE;
    // change signal
    gbSignal[2].status = SIGNAL_STATUS_STOP;
  }
  //
  updateButtonLedStatus();
}
