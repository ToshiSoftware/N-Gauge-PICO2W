/*
  tft_ui_v2.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  TFT display : new design (TFT_UI_DESIGN == 2)
  (c)Toshi 2025

  画面構成 (320x240)
    +-------------------------------------------------------------+
    | [MODE] info                                        STEP 000 |  ステータスバー
    +-----------------------------------------+-----------------+-+
    |                                         | SPD        FDR >|B|
    |        線路図（センサ/信号/踏切/ポイント）   |  128            |A|  <- 右側の実機フェーダー
    |                                         | [1 | 2 | <>]    |R|     に隣接する速度バー
    |                                         | [ MAN  ]        | |  出発番線 1のみ/2のみ/交互
    |                                         | [ STRT ]        | |
    |                                         | [ LOOP ]        | |
    +-----------+-----------+-----------+-----+-----------+-----+-+
    | POINT 1   | POINT 2   | CROSSING  | DIRECTION              |  下段ボタン群
    | [SW1|SW2] | [SW3|SW4] | [SW5|SW6] | [SW7|SW8]              |  <- 真下に実機 SW1〜SW8
    +-------------------------------------------------------------+

  下段の各グループは左右 2 分割で、真下のハードウェアボタン（＋LED）と 1 対 1 に対応する。
  点灯している半分 = 実機で LED が点灯しているボタン。タッチでもその半分を押すと同じ動作になる。
*/

#include "MYTCS_PICO2.h"

#if TFT_UI_DESIGN == 2

// ---------------- layout ----------------
#define UI_STATUS_H     13

#define UI_PANEL_X      240   // 右パネル左端
#define UI_PANEL_R      302   // ボタン右端
#define UI_BAR_X        309   // 速度バー
#define UI_BAR_Y        17
#define UI_BAR_W        9
#define UI_BAR_H        156

#define UI_GROUP_Y      184
#define UI_GROUP_H      54

// ---------------- helpers ----------------
static uint16_t uiDim(uint16_t c){ return (c >> 1) & 0x7BEF; } // 50% 輝度

static void uiText(GFXcanvas16 *c, const char *s, int x, int y, const GFXfont *f, uint16_t col){
  c->setFont(f);
  c->setTextSize(1);
  c->setTextWrap(false);
  c->setTextColor(col);
  c->setCursor(x, y);
  c->print(s);
}

static void uiTextBounds(GFXcanvas16 *c, const char *s, const GFXfont *f, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h){
  c->setFont(f);
  c->setTextSize(1);
  c->setTextWrap(false);
  c->getTextBounds(s, 0, 0, x1, y1, w, h);
}

// (cx, cy) を中心に描画
static void uiTextCenter(GFXcanvas16 *c, const char *s, int cx, int cy, const GFXfont *f, uint16_t col){
  int16_t x1, y1; uint16_t w, h;
  uiTextBounds(c, s, f, &x1, &y1, &w, &h);
  uiText(c, s, cx - (int)w/2 - x1, cy - (int)h/2 - y1, f, col);
}

// 右端 rx に揃える。f==NULL のとき y は文字の上端、フォント指定時は y はベースライン
static void uiTextRight(GFXcanvas16 *c, const char *s, int rx, int y, const GFXfont *f, uint16_t col){
  int16_t x1, y1; uint16_t w, h;
  uiTextBounds(c, s, f, &x1, &y1, &w, &h);
  uiText(c, s, rx - (int)w - x1, y, f, col);
}

// 3px 幅の線（水平・斜め線用）
static void uiThickLine(GFXcanvas16 *c, int x0, int y0, int x1, int y1, uint16_t col){
  for(int d=-1; d<=1; d++){
    c->writeLine(x0, y0+d, x1, y1+d, col);
  }
}

// 2px 幅の線（アイコン用）
static void uiLine2(GFXcanvas16 *c, int x0, int y0, int x1, int y1, uint16_t col){
  c->writeLine(x0, y0, x1, y1, col);
  c->writeLine(x0, y0+1, x1, y1+1, col);
}

// ---------------- buttons ----------------
//  0-3 : 下段グループ（SW1/2, SW3/4, SW5/6, SW7/8 の上）
//  4-6 : 右パネルのモードボタン
#define UI_GROUP_X(k)   (UI_SW_ORIGIN_X + UI_SW_PITCH*2*(k) + 2)
#define UI_GROUP_W      (UI_SW_PITCH*2 - 4)

Type_TFTButton tftButton[NUM_TFT_BUTTONS]=
{
  {UI_GROUP_X(0), UI_GROUP_Y, UI_GROUP_W, UI_GROUP_H, UI_COL_ROUTE, UI_COL_PANEL, UI_COL_BG, 0, 0, "POINT 1",   0, 0, 0, 0},
  {UI_GROUP_X(1), UI_GROUP_Y, UI_GROUP_W, UI_GROUP_H, UI_COL_ROUTE, UI_COL_PANEL, UI_COL_BG, 0, 0, "POINT 2",   0, 0, 0, 0},
  {UI_GROUP_X(2), UI_GROUP_Y, UI_GROUP_W, UI_GROUP_H, UI_COL_CROSS, UI_COL_PANEL, UI_COL_BG, 0, 0, "CROSSING",  0, 0, 0, 0},
  {UI_GROUP_X(3), UI_GROUP_Y, UI_GROUP_W, UI_GROUP_H, UI_COL_DIR,   UI_COL_PANEL, UI_COL_BG, 0, 0, "DIRECTION", 0, 0, 0, 0},

  {UI_PANEL_X+2,  88, 60, 26, UI_COL_MAN,   UI_COL_PANEL, UI_COL_BG, 0, 0, "MAN",  0, 0, 0, 0},
  {UI_PANEL_X+2, 118, 60, 26, UI_COL_START, UI_COL_PANEL, UI_COL_BG, 0, 0, "STRT", 0, 0, 0, 0}, // TFT_BUTTON_SERVICE (label set by ServiceUpdateButton)
  {UI_PANEL_X+2, 148, 60, 26, UI_COL_LOOP,  UI_COL_PANEL, UI_COL_BG, 0, 0, "LOOP", 0, 0, 0, 0},

  {UI_PANEL_X+2,  58, 60, 24, UI_COL_START, UI_COL_PANEL, UI_COL_BG, 0, 0, "TRACK", 0, 0, 0, 0}  // TFT_BUTTON_TRACK
};

// 出発番線セレクタで押された区画（0=1番線のみ, 1=2番線のみ, 2=交互）
static int uiTouchedThird(Type_TFTButton *bt){
  int sec = (bt->touchX - bt->pos_x) * NUM_SVC_TRACK_MODE / bt->width;
  if(sec < 0) sec = 0;
  if(sec >= NUM_SVC_TRACK_MODE) sec = NUM_SVC_TRACK_MODE - 1;
  return sec;
}

// グループ内の左右どちらが押されたか（0=左=奇数番SW, 1=右=偶数番SW）
static int uiTouchedHalf(Type_TFTButton *bt){
  return (bt->touchX >= bt->pos_x + bt->width/2) ? 1 : 0;
}

// グループで現在選択されている半分（実機 LED の点灯位置と同じ）
static int uiSelectedHalf(int idx){
  switch(idx){
    case 0: return (tftPoint[0].direction == POINT_DIRECTION_LEFT) ? 0 : 1;
    case 1: return (tftPoint[1].direction == POINT_DIRECTION_LEFT) ? 0 : 1;
    case 2: return (tftCross[0].isActive == true) ? 1 : 0;
    case 3: return (gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE) ? 0 : 1;
  }
  return 0;
}

// ボタンが押されたときの処理（実機ボタンと同じ割り当て: systemctrl.ino の LED BUTTON map 参照）
static void uiButtonAction(int idx, int half){
  switch(idx){
    case 0: // Point 1 : SW1=LEFT, SW2=RIGHT
      gbIsHC595Update = true;
      gbPoint[0].direction = half ? POINT_DIRECTION_RIGHT : POINT_DIRECTION_LEFT;
      gbSignal[0].status = (gbPoint[0].direction == POINT_DIRECTION_LEFT) ? SIGNAL_STATUS_STOP : SIGNAL_STATUS_GO;
      break;
    case 1: // Point 2 : SW3=LEFT, SW4=RIGHT
      gbIsHC595Update = true;
      gbPoint[1].direction = half ? POINT_DIRECTION_RIGHT : POINT_DIRECTION_LEFT;
      gbSignal[1].status = (gbPoint[1].direction == POINT_DIRECTION_RIGHT) ? SIGNAL_STATUS_STOP : SIGNAL_STATUS_GO;
      break;
    case 2: // Crossing : SW5=OFF, SW6=ON
      gbIsHC595Update = true;
      gbCrossing[0].status = half ? CROSSING_STATUS_ON : CROSSING_STATUS_OFF;
      break;
    case 3: // Direction : SW7=CLOCKWISE, SW8=COUNTERCLOCKWISE
      gbIsHC595Update = true;
      gbTrain.direction = half ? TRAIN_DIRECTION_COUNTERCLOCKWISE : TRAIN_DIRECTION_CLOCKWISE;
      gbSignal[2].status = (gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE) ? SIGNAL_STATUS_GO : SIGNAL_STATUS_STOP;
      break;
    case 4: // Mode : Manual
      gbIsHC595Update = true;
      gbSystem.mode = SYSTEM_MODE_MANUAL;
      gbSystem.scenario_counter = 0;
      break;
    case TFT_BUTTON_SERVICE: // STRT / RET
      ServiceButtonPressed();
      break;
    case TFT_BUTTON_TRACK: // 出発番線（次の出発から有効）
      gbSvcTrackMode = half;  // 0..2 = SVC_TRACK_MODE_1 / _2 / _ALT
      break;
    case 6: // LOOP
      gbIsHC595Update = true;
      gbSystem.isEndless = 1 - gbSystem.isEndless;
      if(gbSystem.isEndless == true){
        ServiceLoopTurnedOn(); // 出発回数カウンタをリセット
      }
      break;
  }
}

// タッチパネルのボタンにより、内部パラメータをセットする
void setParamFromTFTUI( void ) {
  if( gbIntCounter > lastUITime + UI_BUTTON_DETECTION_INTARVAL){
    for(int i=0; i<NUM_TFT_BUTTONS; i++){
      Type_TFTButton *bt = &tftButton[i];
      if(bt->isPressed == true && bt->isPressedPrevious == false){
        uiButtonAction(i, (i == TFT_BUTTON_TRACK) ? uiTouchedThird(bt) : uiTouchedHalf(bt));
        flagDrawTFT = true;
      }
      if(bt->isPressedPrevious != bt->isPressed){
        flagDrawTFT = true;
      }
      bt->isPressedPrevious = bt->isPressed;
    }
    // store the last time UI pressed
    lastUITime = gbIntCounter;
  }
}

// ---- 下段グループのアイコン ----
// Point 1 : 根元は右（本線 x>170 側）。LEFT=直進（水平）、RIGHT=分岐（左上へ）
static void uiIconPoint1(GFXcanvas16 *c, int cx, int cy, int half, uint16_t fg, uint16_t sub){
  int px = cx + 4, py = cy + 5;
  uiLine2(c, cx + 12, py, px, py, fg);                 // 根元
  if(half == 0){
    uiLine2(c, px, py, px - 10, py - 10, sub);         // 分岐（非開通）
    c->writeLine(px, py, px - 10, py - 10, sub);
    uiLine2(c, px, py, cx - 13, py, fg);               // 直進（開通）
    c->writeLine(px, py - 1, cx - 13, py - 1, fg);
  } else {
    c->writeLine(px, py, cx - 13, py, sub);            // 直進（非開通）
    uiLine2(c, px, py, px - 11, py - 11, fg);          // 分岐（開通）
    c->writeLine(px + 1, py, px - 10, py - 11, fg);
  }
}

// Point 2 : 根元は右下（構内入口側）。RIGHT=左上へ直線、LEFT=水平→左上
static void uiIconPoint2(GFXcanvas16 *c, int cx, int cy, int half, uint16_t fg, uint16_t sub){
  int px = cx + 4, py = cy + 3;
  uiLine2(c, cx + 11, cy + 10, px, py, fg);            // 根元
  c->writeLine(cx + 12, cy + 10, px + 1, py, fg);
  if(half == 0){
    c->writeLine(px, py, px - 12, py - 12, sub);       // RIGHT 側（非開通）
    uiLine2(c, px, py, px - 8, py, fg);                // LEFT 側（開通）
    uiLine2(c, px - 8, py, px - 16, py - 8, fg);
    c->writeLine(px - 7, py, px - 15, py - 8, fg);
  } else {
    c->writeLine(px, py, px - 8, py, sub);             // LEFT 側（非開通）
    c->writeLine(px - 8, py, px - 16, py - 8, sub);
    uiLine2(c, px, py, px - 12, py - 12, fg);          // RIGHT 側（開通）
    c->writeLine(px + 1, py, px - 11, py - 12, fg);
  }
}

// Crossing : 2灯 + OFF/ON
static void uiIconCross(GFXcanvas16 *c, int cx, int cy, int half, int on, uint16_t fg){
  int ly = cy - 5;
  if(half == 0){
    c->drawCircle(cx - 6, ly, 4, fg);
    c->drawCircle(cx + 6, ly, 4, fg);
    uiTextCenter(c, "OFF", cx, cy + 10, NULL, fg);
  } else {
    if(on){ // 作動中は交互に点滅
      if(flagAlt800 == 0){ c->fillCircle(cx - 6, ly, 4, fg); c->drawCircle(cx + 6, ly, 4, fg); }
      else               { c->drawCircle(cx - 6, ly, 4, fg); c->fillCircle(cx + 6, ly, 4, fg); }
    } else {
      c->fillCircle(cx - 6, ly, 4, fg);
      c->fillCircle(cx + 6, ly, 4, fg);
    }
    uiTextCenter(c, "ON", cx, cy + 10, NULL, fg);
  }
}

// Direction : 回転矢印 + CW/CCW
static void uiIconDir(GFXcanvas16 *c, int cx, int cy, int half, uint16_t fg){
  int ax = cx, ay = cy - 8, r = 7;
  if(half == 0){ // CLOCKWISE : 下側を左向きに進む
    c->drawCircleHelper(ax, ay, r,   1|2|4, fg);
    c->drawCircleHelper(ax, ay, r-1, 1|2|4, fg);
    c->fillTriangle(ax - 5, ay + r, ax + 1, ay + r - 4, ax + 1, ay + r + 4, fg);
    uiTextCenter(c, "CW", cx, cy + 11, NULL, fg);
  } else {       // COUNTERCLOCKWISE : 下側を右向きに進む
    c->drawCircleHelper(ax, ay, r,   1|2|8, fg);
    c->drawCircleHelper(ax, ay, r-1, 1|2|8, fg);
    c->fillTriangle(ax + 5, ay + r, ax - 1, ay + r - 4, ax - 1, ay + r + 4, fg);
    uiTextCenter(c, "CCW", cx, cy + 11, NULL, fg);
  }
}

// 下段グループ（左右 2 分割のセグメントボタン）
static void drawTFTSwitchGroup(int idx, Type_TFTButton *bt, GFXcanvas16 *c){
  int x = bt->pos_x, y = bt->pos_y, w = bt->width, h = bt->height;
  uint16_t accent = bt->color_frame;
  int sel = uiSelectedHalf(idx);

  c->fillRoundRect(x, y, w, h, 6, UI_COL_PANEL);
  c->drawRoundRect(x, y, w, h, 6, bt->isPressed ? UI_COL_TEXT : UI_COL_BORDER);
  uiTextCenter(c, bt->text, x + w/2, y + 7, NULL, UI_COL_MUTED);

  int hw = (w - 8) / 2;
  int hh = h - 17;
  int hy = y + 14;
  for(int k=0; k<2; k++){
    int hx = x + 3 + k*(hw + 2);
    int on = (k == sel);
    uint16_t fill = UI_COL_PANEL_HI;
    if(on){
      fill = accent;
      if(idx == 2 && k == 0) fill = UI_COL_MUTED; // 踏切 OFF は落ち着いた色
    }
    c->fillRoundRect(hx, hy, hw, hh, 4, fill);
    uint16_t fg  = on ? UI_COL_BG : UI_COL_MUTED;
    uint16_t sub = on ? uiDim(fill) : UI_COL_DIM;
    int cx = hx + hw/2, cy = hy + hh/2;
    switch(idx){
      case 0: uiIconPoint1(c, cx, cy, k, fg, sub); break;
      case 1: uiIconPoint2(c, cx, cy, k, fg, sub); break;
      case 2: uiIconCross(c, cx, cy, k, on, fg);   break;
      case 3: uiIconDir(c, cx, cy, k, fg);         break;
    }
  }
}

// 右パネルのモードボタン
static void drawTFTModeButton(Type_TFTButton *bt, GFXcanvas16 *c){
  int x = bt->pos_x, y = bt->pos_y, w = bt->width, h = bt->height;
  uint16_t accent = bt->color_frame;
  uint16_t fill, border, text;

  if(bt->isPressed == true){
    fill = UI_COL_PANEL_HI; border = accent; text = accent;
  }
  else if(bt->isActive == true){
    fill = accent; border = accent; text = UI_COL_BG;
  }
  else{
    fill = UI_COL_PANEL; border = UI_COL_BORDER; text = accent;
  }
  c->fillRoundRect(x, y, w, h, 8, fill);
  c->drawRoundRect(x, y, w, h, 8, border);
  uiTextCenter(c, bt->text, x + w/2, y + h/2, &FreeSansBold9pt7b, text);

  // SERVICE 運転中は右上のドットが 800ms 周期で点滅
  if(bt == &tftButton[TFT_BUTTON_SERVICE] && bt->isActive == true && flagAlt800 == 0){
    c->fillCircle(x + w - 7, y + 7, 2, UI_COL_BG);
  }
}

// 出発番線セレクタ（1 / 2 / 交互 の 3 区画）
static void drawTFTTrackSelector(Type_TFTButton *bt, GFXcanvas16 *c){
  int x = bt->pos_x, y = bt->pos_y, w = bt->width, h = bt->height;
  uint16_t accent = bt->color_frame;
  int sel = gbSvcTrackMode;

  c->fillRoundRect(x, y, w, h, 6, UI_COL_PANEL);
  c->drawRoundRect(x, y, w, h, 6, bt->isPressed ? UI_COL_TEXT : UI_COL_BORDER);
  int sw = (w - 4) / NUM_SVC_TRACK_MODE;  // 区画の幅
  for(int k=0; k<NUM_SVC_TRACK_MODE; k++){
    int sx = x + 2 + k*sw;
    int on = (k == sel);
    if(on){
      c->fillRoundRect(sx, y + 2, sw, h - 4, 4, accent);
    }
    uint16_t fg = on ? UI_COL_BG : UI_COL_MUTED;
    int cx = sx + sw/2, cy = y + h/2;
    if(k == SVC_TRACK_MODE_1){
      uiTextCenter(c, "1", cx, cy, &FreeSansBold9pt7b, fg);
    }
    else if(k == SVC_TRACK_MODE_2){
      uiTextCenter(c, "2", cx, cy, &FreeSansBold9pt7b, fg);
    }
    else{ // 交互：左右の矢印
      c->drawFastHLine(cx - 5, cy - 3, 9, fg);
      c->fillTriangle(cx + 5, cy - 3, cx + 2, cy - 6, cx + 2, cy, fg);
      c->drawFastHLine(cx - 4, cy + 3, 9, fg);
      c->fillTriangle(cx - 5, cy + 3, cx - 2, cy, cx - 2, cy + 6, fg);
    }
  }
}

void drawTFTButton( Type_TFTButton *bt, GFXcanvas16 *canvas ){
  int idx = bt - tftButton;
  if(idx >= 0 && idx < 4){
    drawTFTSwitchGroup(idx, bt, canvas);
  }
  else if(idx == TFT_BUTTON_TRACK){
    drawTFTTrackSelector(bt, canvas);
  }
  else{
    drawTFTModeButton(bt, canvas);
  }
}

// ---------------- track diagram ----------------
// 線路図全体の縦方向オフセット（ステータスバーと下段ボタンの間で上下中央に配置）
//   座標表（tftSensor / tftCross / tftSignal）は元デザインと共通のまま、描画時にずらす
#define UI_TRACK_DY  7
#define TY(y)        ((y) + UI_TRACK_DY)

void drawTFTDebugText( GFXcanvas16 *canvas, char *text ){
  uiText(canvas, text, 4, 174, NULL, UI_COL_DIM);
}

// ---------------- tram illustration ----------------
// 線路内の空きスペースに置く路面電車のイラスト（クリーム＋オレンジの旧型車をモチーフ）
// (x0, y0) = 車体屋根の左上。全体の大きさは約 76x50（架線 y0-15 〜 レール y0+35）
#define UI_TRAM_X  122
#define UI_TRAM_Y  TY(46)

#define TRAM_COL_CREAM   RGB565(226, 210, 160)
#define TRAM_COL_ORANGE  RGB565(228,  96,  42)
#define TRAM_COL_YELLOW  RGB565(244, 200,  40)
#define TRAM_COL_GLASS   RGB565( 40,  58,  80)
#define TRAM_COL_GLARE   RGB565( 92, 120, 150)
#define TRAM_COL_ROOF    RGB565(176, 178, 176)
#define TRAM_COL_ROOF_HI RGB565(214, 216, 214)
#define TRAM_COL_TRUCK   RGB565( 52,  54,  60)
#define TRAM_COL_LIGHT   RGB565(255, 244, 200)

// ---- 走行中アニメーション ----
//  車体（屋根・パンタ根元・車体）が上下に 1px 揺れ、車輪のスポークが回る。
//  架線・レール・台車は固定。パンタグラフは架線に接したまま伸び縮みする。
//  描画は電車の周囲だけを部分転送するので軽い（全画面転送 約80ms に対し数 ms）。
#define TRAM_ANIM_MIN_SPEED   TRAIN_SPEED_MIN  // これ未満は停車中とみなし揺らさない
#define TRAM_ANIM_SLOW_MS     300              // 低速時のコマ間隔 [ms]
#define TRAM_ANIM_FAST_MS     90               // 最高速時のコマ間隔 [ms]
// 部分転送する範囲（電車イラスト全体を含み、他の表示物と重ならないこと）
#define TRAM_AREA_X  (UI_TRAM_X - 5)
#define TRAM_AREA_Y  (UI_TRAM_Y - 16)
#define TRAM_AREA_W  78
#define TRAM_AREA_H  54

static int tramPhase = 0;                  // アニメーションのコマ番号
static const int8_t tramBob[8] = {0, 1, 1, 0, 1, 0, 0, 1}; // 車体の上下（不規則に揺らす）

static void drawTFTTram( GFXcanvas16 *c, int x0, int y0 ){
  int yb = y0 + tramBob[tramPhase & 7];   // 車体の基準 y（揺れ込み）

  // 架線とレール（固定）
  c->drawFastHLine(x0 - 4, y0 - 15, 76, UI_COL_DIM);
  c->drawFastHLine(x0 - 4, y0 + 35, 76, UI_COL_DIM);
  c->drawFastHLine(x0 - 4, y0 + 36, 76, UI_COL_TRACK_OFF);

  // 台車（固定）
  c->fillRoundRect(x0 + 6,  y0 + 29, 16, 4, 1, TRAM_COL_TRUCK);
  c->fillRoundRect(x0 + 46, y0 + 29, 16, 4, 1, TRAM_COL_TRUCK);

  // パンタグラフ（Z 形）: 根元は車体と一緒に動き、集電舟は架線に接したまま
  c->writeLine(x0 + 30, yb - 4,  x0 + 37, y0 - 9,  UI_COL_MUTED);
  c->writeLine(x0 + 37, y0 - 9,  x0 + 32, y0 - 14, UI_COL_MUTED);
  c->drawFastHLine(x0 + 27, y0 - 14, 11, UI_COL_TEXT);

  // 屋根と屋上機器（冷房装置）
  c->fillRect(x0 + 24, yb - 4, 20, 5, TRAM_COL_ROOF_HI);
  c->drawFastHLine(x0 + 24, yb - 2, 20, TRAM_COL_ROOF);
  c->fillRect(x0 + 13, yb - 3, 8, 4, TRAM_COL_ROOF_HI);
  c->fillRoundRect(x0 + 1, yb, 66, 6, 3, TRAM_COL_ROOF);

  // 車体（クリーム）
  c->fillRoundRect(x0, yb + 3, 68, 24, 3, TRAM_COL_CREAM);
  // 窓まわりの帯（オレンジ）
  c->fillRect(x0, yb + 8, 68, 10, TRAM_COL_ORANGE);
  // 黄色の帯
  c->fillRect(x0, yb + 21, 68, 2, TRAM_COL_YELLOW);
  // 裾（オレンジ）
  c->fillRoundRect(x0, yb + 24, 68, 5, 2, TRAM_COL_ORANGE);

  // 側窓
  const int winX[] = {3, 10, 17, 35, 42, 49};
  for(int i=0; i<6; i++){
    c->fillRect(x0 + winX[i], yb + 9, 6, 7, TRAM_COL_GLASS);
    c->drawFastHLine(x0 + winX[i], yb + 9, 6, TRAM_COL_GLARE);
  }
  // 乗降扉
  c->fillRect(x0 + 25, yb + 8, 8, 21, TRAM_COL_ORANGE);
  c->fillRect(x0 + 26, yb + 9, 6, 9, TRAM_COL_GLASS);
  c->drawFastVLine(x0 + 29, yb + 9, 19, TRAM_COL_CREAM);
  // 運転台の前面窓（右側が進行方向）
  c->fillRect(x0 + 57, yb + 9, 9, 8, TRAM_COL_GLASS);
  c->drawFastHLine(x0 + 57, yb + 9, 9, TRAM_COL_GLARE);
  // 行先表示と前照灯
  c->fillRect(x0 + 56, yb + 4, 9, 3, TRAM_COL_GLASS);
  c->drawFastHLine(x0 + 57, yb + 5, 7, TRAM_COL_LIGHT);
  c->fillCircle(x0 + 61, yb + 1, 2, TRAM_COL_LIGHT);

  // 車輪（固定）: スポーク位置が回転する。向きは進行方向に合わせる
  static const int8_t spokeX[4] = {0, 1, 0, -1};
  static const int8_t spokeY[4] = {-1, 0, 1, 0};
  int sp = (gbTrain.direction == TRAIN_DIRECTION_CLOCKWISE) ? (tramPhase & 3) : ((4 - (tramPhase & 3)) & 3);
  const int wheelX[] = {9, 19, 49, 59};
  for(int i=0; i<4; i++){
    c->fillCircle(x0 + wheelX[i], y0 + 32, 2, TRAM_COL_TRUCK);
    c->drawPixel(x0 + wheelX[i] + spokeX[sp], y0 + 32 + spokeY[sp], UI_COL_MUTED);
  }
}

// loop() から毎回呼ぶ：走行中は速度に応じた間隔で電車を揺らす
void uiTramAnimate(void){
  static unsigned int lastTick = 0;
  static uint16_t areaBuf[TRAM_AREA_W * TRAM_AREA_H];
  int speed = gbTrain.speed;

  if(speed < TRAM_ANIM_MIN_SPEED){
    if(tramPhase != 0){       // 停車したら揺れのない姿勢に戻す
      tramPhase = 0;
      flagDrawTFT = true;
    }
    return;
  }
  unsigned int interval = TRAM_ANIM_SLOW_MS - (TRAM_ANIM_SLOW_MS - TRAM_ANIM_FAST_MS) * speed / 255;
  if(gbIntCounter - lastTick < interval) return;
  lastTick = gbIntCounter;
  tramPhase = (tramPhase + 1) & 7;

  // 全画面の再描画が予定されていれば、そちらで描かれる
  if(flagDrawTFT == true) return;

  // 電車の周囲だけを描き直して部分転送
  canvas.fillRect(TRAM_AREA_X, TRAM_AREA_Y, TRAM_AREA_W, TRAM_AREA_H, UI_COL_BG);
  drawTFTTram(&canvas, UI_TRAM_X, UI_TRAM_Y);
  uint16_t *src = canvas.getBuffer();
  for(int y=0; y<TRAM_AREA_H; y++){
    memcpy(&areaBuf[y * TRAM_AREA_W], &src[(TRAM_AREA_Y + y) * TFT_WIDTH + TRAM_AREA_X], TRAM_AREA_W * sizeof(uint16_t));
  }
  tft.drawRGBBitmap(TRAM_AREA_X, TRAM_AREA_Y, areaBuf, TRAM_AREA_W, TRAM_AREA_H);
}

void drawTFTRail( GFXcanvas16 *canvas ){
  // 路面電車のイラスト（線路内の空きスペース）
  drawTFTTram(canvas, UI_TRAM_X, UI_TRAM_Y);

  // 本線（周回）
  for(int d=-1; d<=1; d++){
    canvas->drawRoundRect(20+d, TY(20+d), 200-2*d, 141-2*d, 30-d, UI_COL_TRACK);
  }
  // 構内侵入
  uiThickLine(canvas, 170, TY(160), 110, TY(100), UI_COL_TRACK);
  // ホーム１
  uiThickLine(canvas, 110, TY(100), 60, TY(50), UI_COL_TRACK);
  // ホーム２
  uiThickLine(canvas, 110, TY(100), 70, TY(100), UI_COL_TRACK);
  uiThickLine(canvas, 70, TY(100), 40, TY(70), UI_COL_TRACK);
  // 車止め
  canvas->writeLine(56, TY(54), 64, TY(46), UI_COL_TRACK);
  canvas->writeLine(55, TY(53), 63, TY(45), UI_COL_TRACK);
  canvas->writeLine(36, TY(74), 44, TY(66), UI_COL_TRACK);
  canvas->writeLine(35, TY(73), 43, TY(65), UI_COL_TRACK);
}

void drawTFTSensor( GFXcanvas16 *canvas )
{
  for(int i=0; i<NUM_TFT_SENSOR; i++){
    int x = tftSensor[i].pos_x, y = TY(tftSensor[i].pos_y);
    if( tftSensor[i].isActive == true){
      canvas->drawRoundRect(x-7, y-7, 15, 15, 3, uiDim(UI_COL_OCCUPIED));
      canvas->fillRoundRect(x-5, y-5, 11, 11, 2, UI_COL_OCCUPIED);
    }
    else{
      canvas->fillRoundRect(x-5, y-5, 11, 11, 2, UI_COL_BG);
      canvas->drawRoundRect(x-5, y-5, 11, 11, 2, UI_COL_TRACK);
    }
  }
}

void drawTFTCross( GFXcanvas16 *canvas ){
  for(int i=0; i<NUM_TFT_CROSS; i++){
    int x = tftCross[i].pos_x, y = TY(tftCross[i].pos_y);
    int active = (tftCross[i].isActive == true);
    canvas->fillRoundRect(x, y, 30, 20, 6, UI_COL_PANEL);
    canvas->drawRoundRect(x, y, 30, 20, 6, active ? UI_COL_CROSS : UI_COL_BORDER);
    uint16_t lamp1 = UI_COL_LAMP_OFF, lamp2 = UI_COL_LAMP_OFF;
    if(active){
      if(flagAlt800 == 0) lamp2 = UI_COL_CROSS;
      else                lamp1 = UI_COL_CROSS;
    }
    canvas->fillCircle(x+9,  y+10, 4, lamp1);
    canvas->fillCircle(x+20, y+10, 4, lamp2);
  }
}

void drawTFTSignal( GFXcanvas16 *canvas ){
  for(int i=0; i<NUM_TFT_SIGNAL; i++){
    int x = tftSignal[i].pos_x, y = TY(tftSignal[i].pos_y);
    canvas->fillRoundRect(x+2, y, 16, 40, 8, UI_COL_PANEL);
    canvas->drawRoundRect(x+2, y, 16, 40, 8, UI_COL_BORDER);
    // 上から 緑 / 黄 / 赤
    uint16_t g = UI_COL_LAMP_OFF, yl = UI_COL_LAMP_OFF, r = UI_COL_LAMP_OFF;
    if(tftSignal[i].color == SIGNAL_COLOR_RED)         r  = UI_COL_SIG_RED;
    else if(tftSignal[i].color == SIGNAL_COLOR_YELLOW) yl = UI_COL_SIG_YEL;
    else                                               g  = UI_COL_SIG_GRN;
    canvas->fillCircle(x+10, y+10, 4, g);
    canvas->fillCircle(x+10, y+20, 4, yl);
    canvas->fillCircle(x+10, y+30, 4, r);
  }
}

void drawTFTPoint( GFXcanvas16 *canvas ){
  uint16_t on  = UI_COL_ROUTE;
  uint16_t off = UI_COL_TRACK_OFF;

  // point 1 : 170,160 分岐側(→120,110) / 直進側(→110,160)
  int p1r = (tftPoint[0].direction == POINT_DIRECTION_RIGHT);
  uiThickLine(canvas, 170, TY(160), 120, TY(110), p1r ? on : off);
  uiThickLine(canvas, 170, TY(160), 110, TY(160), p1r ? off : on);
  canvas->fillCircle(170, TY(160), 3, on);

  // point 2 : 110,100 → 60,50（RIGHT） / → 70,100 → 40,70（LEFT）
  int p2r = (tftPoint[1].direction == POINT_DIRECTION_RIGHT);
  uiThickLine(canvas, 110, TY(100), 60, TY(50), p2r ? on : off);
  uiThickLine(canvas, 110, TY(100), 70, TY(100), p2r ? off : on);
  uiThickLine(canvas, 70, TY(100), 40, TY(70), p2r ? off : on);
  canvas->fillCircle(110, TY(100), 3, on);
}

void drawTFTDirection( GFXcanvas16 *canvas, int direction ){
  // 1:左上、2:右上、4:右下、8:左下
  canvas->drawCircleHelper(180, TY(120), 30, 4, UI_COL_DIR);
  canvas->drawCircleHelper(180, TY(120), 29, 4, UI_COL_DIR);
  canvas->drawCircleHelper(60, TY(60), 30, 1, UI_COL_DIR);
  canvas->drawCircleHelper(60, TY(60), 29, 1, UI_COL_DIR);
  if(direction == TRAIN_DIRECTION_CLOCKWISE){
    canvas->fillTriangle(172, TY(150), 181, TY(145), 181, TY(155), UI_COL_DIR); // ◀
    canvas->fillTriangle( 68, TY(30),  59, TY(25),  59, TY(35), UI_COL_DIR); // ▶
  }
  else{
    canvas->fillTriangle(210, TY(112), 205, TY(121), 215, TY(121), UI_COL_DIR); // ▲
    canvas->fillTriangle( 30, TY(68),  25, TY(59),  35, TY(59), UI_COL_DIR); // ▼
  }
}

// ---------------- speed panel (right, next to the fader) ----------------
void drawTFTSpeed( GFXcanvas16 *canvas, int speed ){
  char text[16];
  int isManual = (gbSystem.mode == SYSTEM_MODE_MANUAL);

  uiText(canvas, "SPD", UI_PANEL_X + 2, UI_BAR_Y, NULL, UI_COL_MUTED);
  if(isManual){
    uiTextRight(canvas, "FDR >", UI_PANEL_R, UI_BAR_Y, NULL, UI_COL_DIM);
  }
  else{
    sprintf(text, "TGT %d", gbTrain.targetSpeed);
    uiTextRight(canvas, text, UI_PANEL_R, UI_BAR_Y, NULL, UI_COL_MUTED);
  }
  sprintf(text, "%d", speed);
  uiTextRight(canvas, text, UI_PANEL_R, 52, &FreeSans18pt7b, UI_COL_TEXT);

  // 速度バー（実機フェーダーの隣）
  int innerTop = UI_BAR_Y + 2;
  int innerH   = UI_BAR_H - 4;
  int innerBot = innerTop + innerH;
  canvas->drawRoundRect(UI_BAR_X, UI_BAR_Y, UI_BAR_W, UI_BAR_H, 3, UI_COL_BORDER);
  int fillH = speed * innerH / 255;
  if(fillH > 0){
    canvas->fillRect(UI_BAR_X + 2, innerBot - fillH, UI_BAR_W - 4, fillH, isManual ? UI_COL_SPEED : UI_COL_START);
  }
  // 目盛り 0/25/50/75/100%
  for(int t=0; t<=4; t++){
    int ty = innerBot - t * innerH / 4;
    canvas->drawFastHLine(UI_BAR_X - 3, ty, 2, UI_COL_DIM);
  }
  // 自動運転中は目標速度マーカー
  if(!isManual){
    int ty = innerBot - gbTrain.targetSpeed * innerH / 255;
    canvas->fillTriangle(UI_BAR_X - 5, ty - 3, UI_BAR_X - 5, ty + 3, UI_BAR_X - 1, ty, UI_COL_TEXT);
  }
}

// ---------------- status bar ----------------
void drawTFTOthers( GFXcanvas16 *canvas ){
  char text[48];

  canvas->fillRect(0, 0, TFT_WIDTH, UI_STATUS_H, UI_COL_PANEL);
  canvas->drawFastHLine(0, UI_STATUS_H, TFT_WIDTH, UI_COL_BORDER);

  // モード表示
  const char *mode;
  uint16_t modeCol;
  if(gbSystem.mode == SYSTEM_MODE_MANUAL){ mode = "MANUAL";  modeCol = UI_COL_MAN; }
  else if(gbSystem.mode == SYSTEM_MODE_SERVICE){ mode = "SERVICE"; modeCol = tftButton[TFT_BUTTON_SERVICE].color_frame; }
  else { mode = "AUTO"; modeCol = UI_COL_START; }
  int pw = strlen(mode) * 6 + 7;
  canvas->fillRoundRect(2, 1, pw, UI_STATUS_H - 2, 3, modeCol);
  uiText(canvas, mode, 6, 3, NULL, UI_COL_BG);

  // SERVICE 運転の状況
  int ix = 2 + pw + 6;
  if(gbSystem.mode == SYSTEM_MODE_SERVICE){
    if(gbService.state == SVC_STATE_RETURNING){
      sprintf(text, "RETURN TO TRK%d", gbService.track);
    }
    else{
      sprintf(text, "TRK%d  LAP %d", gbService.track, gbService.lapCount);
    }
    uiText(canvas, text, ix, 3, NULL, UI_COL_TEXT);
    ix += strlen(text) * 6 + 12;
  }
  else{
    // 停止中：次に出発する番線のモード
    const char *dep = (gbSvcTrackMode == SVC_TRACK_MODE_1) ? "DEPART: TRK1 ONLY"
                    : (gbSvcTrackMode == SVC_TRACK_MODE_2) ? "DEPART: TRK2 ONLY"
                    : "DEPART: TRK1/2 ALT";
    uiText(canvas, dep, ix, 3, NULL, UI_COL_MUTED);
    ix += strlen(dep) * 6 + 12;
  }
  if(gbSystem.isEndless == true){
    sprintf(text, "LOOP #%d", gbService.departCount);
    uiText(canvas, text, ix, 3, NULL, UI_COL_LOOP);
  }

  // シナリオステップ
  sprintf(text, "STEP %03d", gbSystem.scenario_counter);
  uiTextRight(canvas, text, TFT_WIDTH - 4, 3, NULL, UI_COL_MUTED);
}

#endif // TFT_UI_DESIGN == 2
