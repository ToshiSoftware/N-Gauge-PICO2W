/*
  pico_util.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  TFT display utility functions
  (c)Toshi 2025
*/

#include "MYTCS_PICO2.h"

// 内部パラメータをTFT表示に反映する
void setTFTFromParams(void){
    // Crossing
    for(int i=0; i<NUM_TFT_CROSS; i++){
      tftCross[i].isActive = gbCrossing[i].status;
    }
    // Point
    for(int i=0; i<NUM_TFT_POINT; i++){
      tftPoint[i].direction = gbPoint[i].direction;
    }
    // Signal
    for(int i=0; i<NUM_TFT_SIGNAL; i++){
      tftSignal[i].color = gbSignal[i].color;
    }
    // Sensor
    for(int i=0; i<NUM_TFT_SENSOR; i++){
      tftSensor[i].isActive = gbSensor[i].status;
    }
}

// タッチパネルのボタンにより、内部パラメータをセットする
void setParamFromTFTUI( void ) {
  if( gbIntCounter > lastUITime + UI_BUTTON_DETECTION_INTARVAL){
    // Pnt 1
    if(tftButton[0].isPressed == true && tftButton[0].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbPoint[0].direction = 1 - gbPoint[0].direction;
      if(gbPoint[0].direction == POINT_DIRECTION_LEFT){
        gbSignal[0].status = SIGNAL_STATUS_STOP;
      }
      else{
        gbSignal[0].status = SIGNAL_STATUS_GO;
      }
      flagDrawTFT = true;
    }
    if(tftButton[0].isPressedPrevious!=tftButton[0].isPressed){
      flagDrawTFT = true;
    }
    tftButton[0].isPressedPrevious = tftButton[0].isPressed;

    // Pnt 2
    if(tftButton[1].isPressed == true && tftButton[1].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbPoint[1].direction = 1 - gbPoint[1].direction;
      if(gbPoint[1].direction == POINT_DIRECTION_RIGHT){
        gbSignal[1].status = SIGNAL_STATUS_STOP;
      }
      else{
        gbSignal[1].status = SIGNAL_STATUS_GO;
      }
      flagDrawTFT = true;
    }
    if(tftButton[1].isPressedPrevious!=tftButton[1].isPressed){
      flagDrawTFT = true;
    }
    tftButton[1].isPressedPrevious = tftButton[1].isPressed;

    // Cross
    if(tftButton[2].isPressed == true && tftButton[2].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbCrossing[0].status = 1 - gbCrossing[0].status;
    }
    if(tftButton[2].isPressedPrevious!=tftButton[2].isPressed){
      flagDrawTFT = true;
    }
    tftButton[2].isPressedPrevious = tftButton[2].isPressed;

    // Train Direction
    if(tftButton[3].isPressed == true && tftButton[3].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbTrain.direction = 1 - gbTrain.direction;
      if(gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE){
        gbSignal[2].status = SIGNAL_STATUS_GO;
      }
      else{
        gbSignal[2].status = SIGNAL_STATUS_STOP;
      }
      flagDrawTFT = true;
    }
    if(tftButton[3].isPressedPrevious!=tftButton[3].isPressed){
      flagDrawTFT = true;
    }
    tftButton[3].isPressedPrevious = tftButton[3].isPressed;
    
    // Mode:Manual
    if(tftButton[4].isPressed == true && tftButton[4].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbSystem.mode = SYSTEM_MODE_MANUAL;
      gbSystem.scenario_counter = 0;
      flagDrawTFT = true;
    }
    if(tftButton[4].isPressedPrevious!=tftButton[4].isPressed){
      flagDrawTFT = true;
    }
    tftButton[4].isPressedPrevious = tftButton[4].isPressed;

    // Mode:AUTO
    if(tftButton[5].isPressed == true && tftButton[5].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbSystem.mode = SYSTEM_MODE_AUTO;
      initScenarioParams();
      flagDrawTFT = true;
    }
    if(tftButton[5].isPressedPrevious!=tftButton[5].isPressed){
      flagDrawTFT = true;
    }
    tftButton[5].isPressedPrevious = tftButton[5].isPressed;

    // Mode:LoopRun
    if(tftButton[6].isPressed == true && tftButton[6].isPressedPrevious == false){
      gbIsUiChanged=true;
      gbSystem.isEndless = 1 - gbSystem.isEndless;
      flagDrawTFT = true;
    }
    if(tftButton[6].isPressedPrevious!=tftButton[6].isPressed){
      flagDrawTFT = true;
    }
    tftButton[6].isPressedPrevious = tftButton[6].isPressed;

    // store the last time UI pressed
    lastUITime = gbIntCounter;    
  }
}
 
// 左上（3700, 3785） 右下（160, 300）
#define LT_X 3700
#define LT_Y 3785
#define RB_X 185
#define RB_Y 300

void getPositionOnScreen( TS_Point *tsPoint, TS_Point *ScreenPos)
{
    int width_x = RB_X - LT_X;
    int width_y = RB_Y - LT_Y;

    int ts_x = tsPoint->x - LT_X;
    int ts_y = tsPoint->y - LT_Y;

    ScreenPos->x = ts_x * 320 / width_x;
    ScreenPos->y = ts_y * 240 / width_y;

    if(ScreenPos->x > 320){ ScreenPos->x = 320;}
    else if(ScreenPos->x < 0){ ScreenPos->x = 0;}
    if(ScreenPos->y > 240){ ScreenPos->y = 240;}
    else if(ScreenPos->y < 0){ ScreenPos->y = 0;}

}

//#define TEXT_OFFSET_X 5
#define TEXT_OFFSET_Y 21

void drawTFTButton( Type_TFTButton *bt, GFXcanvas16 *canvas ){

    //canvas->drawRoundRect(bt->pos_x, bt->pos_y, bt->width, bt->height, 4, bt->color_frame);
    canvas->fillRoundRect(bt->pos_x, bt->pos_y, bt->width, bt->height, bt->height/2-2, bt->color_frame);
    if(bt->isPressed == true )
    {
      //canvas->fillRoundRect(bt->pos_x+1, bt->pos_y+1, bt->width-2, bt->height-2, 4, bt->color_fill);
      canvas->fillRoundRect(bt->pos_x, bt->pos_y, bt->width, bt->height, bt->height/2-2, bt->color_fill);
    }
    canvas->setCursor(bt->pos_x+bt->text_offset_x, bt->pos_y+bt->text_offset_y);
    //canvas->setTextColor(bt->color_text);
    canvas->setTextColor(bt->color_text);
    canvas->setFont(&FreeSansBold9pt7b);
    canvas->print(bt->text);
}

void isTFTButtonPressed( Type_TFTButton *bt, TS_Point *screenPos){
    if( screenPos->x > bt->pos_x && screenPos->x < bt->pos_x+bt->width
        && screenPos->y > bt->pos_y && screenPos->y < bt->pos_y+bt->height)
    {
        bt->isPressed = true;
    }
    else{
        bt->isPressed = false;
    }
}

Type_TFTButton tftButton[NUM_TFT_BUTTONS]=
{
  {10, 205, 60, 30, ILI9341_ORANGE, ILI9341_NIGHTORANGE, ILI9341_BLACK, 5, 21, "PNT.1", 0, 0},
  {80, 205, 60, 30, ILI9341_ORANGE, ILI9341_NIGHTORANGE, ILI9341_BLACK, 5, 21, "PNT.2", 0, 0},
  {150, 205, 60, 30, ILI9341_ORANGE, ILI9341_NIGHTORANGE, ILI9341_BLACK, 5, 21, "CRSS", 0, 0},
  {220, 205, 60, 30, ILI9341_ORANGE, ILI9341_NIGHTORANGE, ILI9341_BLACK, 6, 21, "DIRC", 0, 0},

  {250, 20, 60, 30, ILI9341_WHITE, ILI9341_NIGHTGREY, ILI9341_BLACK, 9, 21, "MAN", 0, 0},
  {250, 60, 60, 30, ILI9341_WHITE, ILI9341_NIGHTGREY, ILI9341_BLACK, 5, 21, "AUTO", 0, 0},
  {250, 100, 60, 30, ILI9341_WHITE, ILI9341_NIGHTGREY, ILI9341_BLACK, 5, 21, "LOOP", 0, 0} //,
  //{250, 130, 60, 30, ILI9341_WHITE, ILI9341_RED, ILI9341_WHITE, "----", 0, 0}
};

uint16_t getILI9341Color(uint16_t red, uint16_t green, uint16_t blue)
{
    red = (red<<8) & 0xF800;
    green = (green<<3) & 0x07E0;
    blue = (blue>>3) & 0x001F;

    return( red|green|blue);
}

// void drawTFTWholeTrack(GFXcanvas16 *canvas){
//     canvas->fillScreen(0x0000);
//     drawTFTRail(canvas);
//     drawTFTSensor(canvas);
// }

void drawTFTDebugText( GFXcanvas16 *canvas, char *text ){
    canvas->setCursor(10, 185);
    canvas->setTextColor(ILI9341_WHITE);
    canvas->setFont(&FreeSans9pt7b);
    canvas->print(text);
}

void drawTFTRail( GFXcanvas16 *canvas ){

    // color
    uint16_t colorTrack = getILI9341Color(127, 127, 127);
    char text[128];
    // sprintf(text, "%04x,", colorTrack);
    // drawTFTDebugText(canvas, text);

    // track
    canvas->drawRoundRect(20, 20, 200, 141, 30, ILI9341_WHITE);
    // 構内侵入
    canvas->writeLine(170, 160, 110, 100, ILI9341_WHITE);
    // ホーム１
    canvas->writeLine(110, 100, 60, 50, ILI9341_WHITE);
    // ホーム２
    canvas->writeLine(110, 100, 70, 100, ILI9341_WHITE);
    canvas->writeLine(70, 100, 40, 70, ILI9341_WHITE);
}

Type_TFTSensor tftSensor[NUM_TFT_SENSOR]=
{
    {1, 50, 80},  // 1
    {0, 70, 60},  // 2
    {0, 150, 140},  // 3
    {0, 70, 160},  // 4
    {0, 220, 70},  // 5
    {0, 80, 20},  // 6
    {0, 160, 20},  // 7
    {1, 212, 152}  // 8
};

void drawTFTSensor( GFXcanvas16 *canvas )
{
    for(int i=0; i<NUM_TFT_SENSOR; i++){
        if( tftSensor[i].isActive == true){
            //canvas->fillCircle(tftSensor[i].pos_x, tftSensor[i].pos_y, 4, ILI9341_GREEN);
            canvas->fillRect(tftSensor[i].pos_x-6, tftSensor[i].pos_y-6, 
                             12, 12,
                             ILI9341_RED
                            );
        }
        else{
            //canvas->fillCircle(tftSensor[i].pos_x, tftSensor[i].pos_y, 4, ILI9341_RED);
            canvas->drawRect(tftSensor[i].pos_x-6, tftSensor[i].pos_y-6, 
                             12, 12,
                             ILI9341_WHITE
                            );
            canvas->fillRect(tftSensor[i].pos_x-5, tftSensor[i].pos_y-5, 
                             10, 10,
                             ILI9341_BLACK
                            );
        }
    }
}

Type_TFTCross tftCross[NUM_TFT_CROSS] = {
    {false, 200, 30}
};


void drawTFTCross( GFXcanvas16 *canvas ){

    for(int i=0; i<NUM_TFT_CROSS; i++){
        if(tftCross[i].isActive == true){
            // frame
            canvas->drawRoundRect(tftCross[i].pos_x, tftCross[i].pos_y, 30, 20, 4, ILI9341_WHITE);
            canvas->fillRoundRect(tftCross[i].pos_x+1, tftCross[i].pos_y+1, 28, 18, 4, ILI9341_BLACK);
            if(flagAlt800 == 0){
                canvas->drawCircle(tftCross[i].pos_x+10, tftCross[i].pos_y+10, 4, ILI9341_DARKGREY);
                canvas->fillCircle(tftCross[i].pos_x+20, tftCross[i].pos_y+10, 4, ILI9341_RED);
            }
            else{
                canvas->fillCircle(tftCross[i].pos_x+10, tftCross[i].pos_y+10, 4, ILI9341_RED);
                canvas->drawCircle(tftCross[i].pos_x+20, tftCross[i].pos_y+10, 4, ILI9341_DARKGREY);
            }
        }
        else{
            // frame
            canvas->drawRoundRect(tftCross[i].pos_x, tftCross[i].pos_y, 30, 20, 4, ILI9341_WHITE);
            canvas->fillRoundRect(tftCross[i].pos_x+1, tftCross[i].pos_y+1, 28, 18, 4, ILI9341_BLACK);
                canvas->drawCircle(tftCross[i].pos_x+10, tftCross[i].pos_y+10, 4, ILI9341_DARKGREY);
                canvas->drawCircle(tftCross[i].pos_x+20, tftCross[i].pos_y+10, 4, ILI9341_DARKGREY);
        }
    }
    // Cross 1 Button lamp
    if(tftCross[0].isActive == true){
        canvas->fillRect(170, 196, 20, 6, ILI9341_RED);        
    }
    else{
        canvas->drawRect(170, 196, 20, 6, ILI9341_LIGHTGREY);
    }

}

Type_TFTSignal tftSignal[NUM_TFT_SIGNAL]=
{
    // {0, 99, 140, 80},
    // {1, 99, 90, 110},
    // {2, 99, 40, 110}
    {0, 99, 130, 90},
    {1, 99, 75, 95},
    {2, 99, 30, 130}};

void drawTFTSignal( GFXcanvas16 *canvas ){
    for(int i=0; i<NUM_TFT_SIGNAL; i++){
        // frame
        canvas->drawRoundRect(tftSignal[i].pos_x,   tftSignal[i].pos_y,   20, 40, 8, ILI9341_WHITE);
        canvas->fillRoundRect(tftSignal[i].pos_x+1, tftSignal[i].pos_y+1, 18, 38, 7, ILI9341_BLACK);
        if(tftSignal[i].color == 0){
            canvas->fillCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+30, 4, ILI9341_RED);
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+20, 4, ILI9341_DARKGREY);
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+10, 4, ILI9341_DARKGREY);
        }
        else if(tftSignal[i].color == 1){
            // frame
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+30, 4, ILI9341_DARKGREY);
            canvas->fillCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+20, 4, ILI9341_YELLOW);
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+10, 4, ILI9341_DARKGREY);
        }
        else{ // if(tftSignal[i].color == 2){
            // frame
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+30, 4, ILI9341_DARKGREY);
            canvas->drawCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+20, 4, ILI9341_DARKGREY);
            canvas->fillCircle(tftSignal[i].pos_x+10, tftSignal[i].pos_y+10, 4, ILI9341_GREEN);
        }
    }
}

Type_TFTPoint tftPoint[NUM_TFT_POINT] = {
    {0, 99},
    {0, 99}
};

void drawTFTPoint( GFXcanvas16 *canvas ){
    uint16_t enabledRailColor = ILI9341_RED;
    uint16_t disabledRailColor = ILI9341_NIGHTGREY;
    int blink = false;

    // point 1
    if(tftPoint[0].direction == POINT_DIRECTION_RIGHT){
        { // if(flagAlt500){
            // 170,160 - 130,120
            canvas->writeLine(170, 162, 120, 112, enabledRailColor);
            canvas->writeLine(170, 161, 120, 111, enabledRailColor);
            canvas->writeLine(170, 159, 120, 109, enabledRailColor);
            canvas->writeLine(170, 158, 120, 108, enabledRailColor);
        }
        // 170,160 - 110,159
        canvas->writeLine(170, 162, 110, 162, disabledRailColor);
        canvas->writeLine(170, 161, 110, 161, disabledRailColor);
        canvas->writeLine(170, 159, 110, 159, disabledRailColor);
        canvas->writeLine(170, 158, 110, 158, disabledRailColor);    
        // Sw lamp
        canvas->drawRect(15, 196, 20, 6, ILI9341_LIGHTGREY);
        canvas->fillRect(45, 196, 20, 6, ILI9341_GREEN);
    }
    else{
        // 170,160 - 130,120
        canvas->writeLine(170, 162, 120, 112, disabledRailColor);
        canvas->writeLine(170, 161, 120, 111, disabledRailColor);
        canvas->writeLine(170, 159, 120, 109, disabledRailColor);
        canvas->writeLine(170, 158, 120, 108, disabledRailColor);
        { // if(flagAlt500){
            // 170,160 - 110,159
            canvas->writeLine(170, 162, 110, 162, enabledRailColor);
            canvas->writeLine(170, 161, 110, 161, enabledRailColor);
            canvas->writeLine(170, 159, 110, 159, enabledRailColor);
            canvas->writeLine(170, 158, 110, 158, enabledRailColor);
        }
        // Sw lamp
        canvas->fillRect(15, 196, 20, 6, ILI9341_GREEN);
        canvas->drawRect(45, 196, 20, 6, ILI9341_LIGHTGREY);
    }
    // point 2
    if(tftPoint[1].direction == POINT_DIRECTION_RIGHT){
        { // if(flagAlt500){
            // 110,100 - 60,50
            canvas->writeLine(110, 102, 60, 52, enabledRailColor);
            canvas->writeLine(110, 101, 60, 51, enabledRailColor);
            canvas->writeLine(110,  99, 60, 49, enabledRailColor);
            canvas->writeLine(110,  98, 60, 48, enabledRailColor);
        }
        // 110,100 - 70,100
        canvas->writeLine(110, 102, 70, 102, disabledRailColor);
        canvas->writeLine(110, 101, 70, 101, disabledRailColor);
        canvas->writeLine(110,  99, 70, 99, disabledRailColor); 
        canvas->writeLine(110,  98, 70, 98, disabledRailColor); 
        // 70,100 - 40,70     
        canvas->writeLine(70, 102, 40, 72, disabledRailColor);
        canvas->writeLine(70, 101, 40, 71, disabledRailColor);
        canvas->writeLine(70,  99, 40, 69, disabledRailColor); 
        canvas->writeLine(70,  98, 40, 68, disabledRailColor);     
        // Sw lamp
        canvas->drawRect(85, 196, 20, 6, ILI9341_LIGHTGREY);
        canvas->fillRect(115, 196, 20, 6, ILI9341_GREEN);    
   }
    else{
        // 110,100 - 50,40
        canvas->writeLine(110, 102, 60, 52, disabledRailColor);
        canvas->writeLine(110, 101, 60, 51, disabledRailColor);
        canvas->writeLine(110,  99, 60, 49, disabledRailColor);
        canvas->writeLine(110,  98, 60, 48, disabledRailColor);
        { // if(flagAlt500){
            // 110,100 - 70,100
            canvas->writeLine(110, 102, 70, 102, enabledRailColor);
            canvas->writeLine(110, 101, 70, 101, enabledRailColor);
            canvas->writeLine(110,  99, 70, 99, enabledRailColor); 
            canvas->writeLine(110,  98, 70, 98, enabledRailColor); 
            // 70,100 - 40,70     
            canvas->writeLine(70, 102, 40, 72, enabledRailColor);
            canvas->writeLine(70, 101, 40, 71, enabledRailColor);
            canvas->writeLine(70,  99, 40, 69, enabledRailColor); 
            canvas->writeLine(70,  98, 40, 68, enabledRailColor);     
        }
        // Sw lamp
        canvas->fillRect(85, 196, 20, 6, ILI9341_GREEN);
        canvas->drawRect(115, 196, 20, 6, ILI9341_LIGHTGREY);    
    }
}

void drawTFTSpeed( GFXcanvas16 *canvas, int speed ){
    char text[128];
    canvas->setCursor(110, 65);
    canvas->setTextColor(ILI9341_WHITE);
    canvas->setFont(&FreeSans18pt7b);
    sprintf( text, "%03d", speed);
    canvas->print(text);  
    canvas->setFont(&FreeSans9pt7b);
    //canvas->print("km/h");  
 
}

void drawTFTDirection( GFXcanvas16 *canvas, int direction ){
    canvas->drawCircleHelper(180, 120, 30, 4, ILI9341_YELLOW); // 1:左上、2:右上、4:右下、8:左下
    canvas->drawCircleHelper(60, 60, 30, 1, ILI9341_YELLOW); // 1:左上、2:右上、4:右下、8:左下
    if(direction == 0){
        // 180,150 ◀
        canvas->fillTriangle(173, 150, 180, 146, 180, 154, ILI9341_YELLOW);
        canvas->fillTriangle( 67,  30, 60, 26, 60, 34, ILI9341_YELLOW);
        // Sw lamp
        canvas->fillRect(225, 196, 20, 6, ILI9341_YELLOW);
        canvas->drawRect(255, 196, 20, 6, ILI9341_LIGHTGREY);
    }
    else{
        // 210,120 ▲
        canvas->fillTriangle(210, 113, 206, 120, 214, 120, ILI9341_YELLOW);
        canvas->fillTriangle(30, 67, 26, 60, 34, 60, ILI9341_YELLOW);
        // Sw lamp
        canvas->drawRect(225, 196, 20, 6, ILI9341_LIGHTGREY);
        canvas->fillRect(255, 196, 20, 6, ILI9341_YELLOW);
    }

}

void drawTFTOthers( GFXcanvas16 *canvas ){

    // Manual button lamp
    if(gbSystem.mode == SYSTEM_MODE_MANUAL){
        canvas->fillRect(241, 25, 6, 20, ILI9341_RED);
    }
    else{
        canvas->drawRect(241, 25, 6, 20, ILI9341_LIGHTGREY);
    }
    // Scenario No.
    char text[128];
    canvas->setCursor(250, 160);
    canvas->setTextColor(ILI9341_WHITE);
    canvas->setFont(&FreeSans9pt7b);
    sprintf(text, "( %03d )", gbSystem.scenario_counter);
    canvas->print(text);  
    // Auto button lamp
    if(gbSystem.mode == SYSTEM_MODE_AUTO){
        canvas->fillRect(241, 65, 6, 20, ILI9341_RED);
    }
    else{
        canvas->drawRect(241, 65, 6, 20, ILI9341_LIGHTGREY);
    }
    // LOOP button lamp
    if(gbSystem.isEndless == true){
        canvas->fillRect(241, 105, 6, 20, ILI9341_RED);
    }
    else{
        canvas->drawRect(241, 105, 6, 20, ILI9341_LIGHTGREY);
    }
}

unsigned int sensorPrevious=0;

void setSensorTFTFromHC166(void){
    for(int i=1; i<=8; i++){
        switch(i)
        {
            case 1: if((gbHC166Data[HC166_SENSOR] & BITMASK_1) == 0) { tftSensor[0].isActive=true; } 
                    else{tftSensor[0].isActive=false;}
                break;
            case 2: if((gbHC166Data[HC166_SENSOR] & BITMASK_2) == 0) { tftSensor[1].isActive=true; } 
                    else{tftSensor[1].isActive=false;}
                break;
            case 3: if((gbHC166Data[HC166_SENSOR] & BITMASK_3) == 0) { tftSensor[2].isActive=true; } 
                    else{tftSensor[2].isActive=false;}
                break;
            case 4: if((gbHC166Data[HC166_SENSOR] & BITMASK_4) == 0) { tftSensor[3].isActive=true; } 
                    else{tftSensor[3].isActive=false;}
                break;
            case 5: if((gbHC166Data[HC166_SENSOR] & BITMASK_5) == 0) { tftSensor[4].isActive=true; } 
                    else{tftSensor[4].isActive=false;}
                break;
            case 6: if((gbHC166Data[HC166_SENSOR] & BITMASK_6) == 0) { tftSensor[5].isActive=true; } 
                    else{tftSensor[5].isActive=false;}
                break;
            case 7: if((gbHC166Data[HC166_SENSOR] & BITMASK_7) == 0) { tftSensor[6].isActive=true; } 
                    else{tftSensor[6].isActive=false;}
                break;
            case 8: if((gbHC166Data[HC166_SENSOR] & BITMASK_8) == 0) { tftSensor[7].isActive=true; }  
                    else{tftSensor[7].isActive=false;}
                break;
            default: break;
        }
    }
    // redraw
    if(sensorPrevious != (gbHC166Data[HC166_SENSOR]&BITMASK_FULL)){
        flagDrawTFT = true;
    }
    sensorPrevious = (gbHC166Data[HC166_SENSOR]&BITMASK_FULL);
}