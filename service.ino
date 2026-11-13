/*
  service.ino
  N-Gauge layout control system using Raspberry Pi Pico2 W
  SERVICE mode: STRT/RET によるループ運転
  (c)Toshi 2025

  動作概要
    STRT 押下 : ランダムに選んだ番線(1/2)の電車が反時計回りに出発し、本線を周回する
    周回中    : 2つの途中駅にランダムで停車する
                  駅A: 踏切付近（5〜7間）  センサ8 通過時に判定
                  駅B: センサ4 付近（6〜4間） センサ7 通過時に判定
    RET 押下  : ボタンが「B2ST」に変わり、次に駅B へ着いた時点で停車し、
                時計回りに方向転換して元の番線へ戻る
    出発番線  : 1番線と2番線から交互に出発する（電源投入後の最初だけランダム）
    自動 RET  : 本線を SVC_AUTO_RET_LAPS 周したら自動で RET
    LOOP ON   : 帰着したら次の電車を自動で出発。SVC_LOOP_MAX_DEPARTS 回出発したら LOOP OFF
    MAN 押下  : その場で自動運転を中止（列車・ポイント等はそのまま）
  ボタン：旧 AUTO ボタンの位置で STRT -> RET -> B2ST と表示が変わる
    帰着後    : 停止状態（MANUAL モード、STRT 待ち）に戻る

  レイアウト（反時計回り = 出発方向）
    ホーム(1/2) -> 3 -> P1 -> 8 -> 5 -> [駅A] -> 踏切 -> 7 -> 6 -> [駅B] -> 4 -> P1(直進) -> 8 ...
  帰還（時計回り）
    [駅B] -> 6 -> 7 -> 踏切 -> [駅A 通過] -> 5 -> 8 -> P1(分岐) -> 3 -> P2 -> ホーム(1/2)

  実装
    既存のシナリオ実行エンジン(HandleAutoDriveScenario)で短い「セグメント」配列を実行し、
    セグメントが終わるたびに ServiceNextSegment() が次のセグメントを選ぶ。
    駅A（反時計回り）はセンサ8 で減速、センサ7 到達で直ちに停止。
    ホームの停止位置/加減速は gbScenario_1 で実機調整済みの値を使用。
    駅B（反時計回り）はセンサ6 で減速、センサ4 で停止（SVC_STOP_B_DELAY で調整）。
*/

#include "MYTCS_PICO2.h"

// 途中駅に停車する確率 [%]（駅A・駅B それぞれ、手前のセンサ通過時に判定）
#define SVC_STOP_PERCENT     35
// 途中駅での停車時間 [ms]
#define SVC_STATION_DWELL    5000
// 周回数がこの値に達したら自動で RET（駅B で方向転換して帰還）
#define SVC_AUTO_RET_LAPS    5
// LOOP ON のとき、この回数だけ出発したら LOOP を OFF に戻す
#define SVC_LOOP_MAX_DEPARTS 10
// 駅B（センサ4付近）：反時計回りでセンサ4 を検出してから停止を始めるまでの時間 [ms]
//   停止位置が手前すぎる場合は値を増やす（センサ4 からさらに進む）
#define SVC_STOP_B_DELAY     0

// セグメント ID
#define SVC_SEG_NONE      0
#define SVC_SEG_DEPART    1  // ホーム出発 〜 センサ7 通過（駅B 判定）
#define SVC_SEG_PASS_B    2  // 駅B を通過 〜 センサ8（駅A 判定）
#define SVC_SEG_STOP_B    3  // 駅B に停車（停車時間を含む）
#define SVC_SEG_LEAVE_B   4  // 駅B を発車 〜 センサ8（駅A 判定）
#define SVC_SEG_PASS_A    5  // 駅A を通過 〜 センサ7（駅B 判定）
#define SVC_SEG_STOP_A    6  // 駅A に停車（停車時間を含む）
#define SVC_SEG_LEAVE_A   7  // 駅A を発車 〜 センサ7（駅B 判定）
#define SVC_SEG_RETURN    8  // 駅B で方向転換 〜 センサ8
#define SVC_SEG_ARRIVE    9  // センサ3 〜 ホーム停車

// ---------------- セグメント定義 ----------------
//  駅A: 踏切付近（5 と 7 の間）  駅B: センサ4 付近（6 と 4 の間）

// ホーム出発（1番線: POINT_2 LEFT）
Type_Scenario svcDepart1[] = {
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    CROSS(CROSSING_STATUS_OFF)
    POINT_1(POINT_DIRECTION_RIGHT)
    POINT_2(POINT_DIRECTION_LEFT)
    WAIT_TIME(3000)
    ACCEL(100)
    SPEED(SPEED_MID)
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_ON)
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)
    POINT_1(POINT_DIRECTION_LEFT)   // 本線を周回できるように直進側へ
    ACCEL(200)
    SPEED(SPEED_HIGH)
};

// ホーム出発（2番線: POINT_2 RIGHT）
Type_Scenario svcDepart2[] = {
    SPEED(SPEED_STOP)
    DIRECTION(TRAIN_DIRECTION_COUNTERCLOCKWISE)
    CROSS(CROSSING_STATUS_OFF)
    POINT_1(POINT_DIRECTION_RIGHT)
    POINT_2(POINT_DIRECTION_RIGHT)
    WAIT_TIME(3000)
    ACCEL(100)
    SPEED(SPEED_MID)
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_ON)
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)
    POINT_1(POINT_DIRECTION_LEFT)
    ACCEL(200)
    SPEED(SPEED_HIGH)
};

// 駅B を通過：センサ4 で減速し、センサ8 まで（センサ8 で駅A の停車/通過を判定）
Type_Scenario svcPassB[] = {
    WAIT_SENSOR(4)
    ACCEL(100)
    SPEED(SPEED_MID)
    WAIT_SENSOR(8)
};

// 駅B に停車：センサ6 で減速、センサ4 で停止
Type_Scenario svcStopB[] = {
    WAIT_SENSOR(6)
    ACCEL(50)
    SPEED(SPEED_LOW)
    WAIT_SENSOR(4)
    WAIT_TIME(SVC_STOP_B_DELAY)
    ACCEL(5)
    SPEED(SPEED_STOP)
    WAIT_TIME(SVC_STATION_DWELL)
};

// 駅B を発車してセンサ8 まで（センサ8 で駅A の停車/通過を判定）
Type_Scenario svcLeaveB[] = {
    ACCEL(100)
    SPEED(SPEED_MID)
    WAIT_SENSOR(8)
};

// 駅A を通過
Type_Scenario svcPassA[] = {
    CROSS(CROSSING_STATUS_ON)
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)
    ACCEL(200)
    SPEED(SPEED_HIGH)
};

// 駅A に停車（反時計回り）：センサ8 で減速、センサ7 に到達したら直ちに停止
Type_Scenario svcStopA[] = {
    ACCEL(50)
    SPEED(SPEED_LOW)
    CROSS(CROSSING_STATUS_ON)
    WAIT_SENSOR(7)
    ACCEL(5)
    SPEED(SPEED_STOP)
    CROSS(CROSSING_STATUS_OFF)      // 踏切は通過済み
    WAIT_TIME(SVC_STATION_DWELL)
};

// 駅A を発車して周回を続ける
Type_Scenario svcLeaveA[] = {
    ACCEL(200)
    SPEED(SPEED_MID)
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_OFF)
    ACCEL(200)
    SPEED(SPEED_HIGH)
};

// 駅B で方向転換し、時計回りでホーム方向へ（駅A は通過。POINT_2 は出発時の番線のまま）
Type_Scenario svcReturn[] = {
    WAIT_TIME(1000)
    DIRECTION(TRAIN_DIRECTION_CLOCKWISE)
    WAIT_TIME(1000)
    ACCEL(200)
    SPEED(SPEED_MID)
    WAIT_SENSOR(6)
    POINT_1(POINT_DIRECTION_RIGHT)  // 列車が P1 から十分離れてから分岐（ホーム側）へ
    WAIT_SENSOR(7)
    CROSS(CROSSING_STATUS_ON)
    WAIT_SENSOR(8)
    CROSS(CROSSING_STATUS_OFF)
};

// 1番線へ到着（gbScenario_1 オレンジ車両の到着と同じ値）
Type_Scenario svcArrive1[] = {
    WAIT_SENSOR(3)
    ACCEL(50)
    SPEED(SPEED_LOW)
    WAIT_TIME(1000)
    ACCEL(20)
    SPEED(40)
    WAIT_SENSOR(1)
    ACCEL(5)
    SPEED(SPEED_STOP)
    WAIT_TIME(2000)
};

// 2番線へ到着（gbScenario_1 の2番線到着と同じ値）
Type_Scenario svcArrive2[] = {
    WAIT_SENSOR(3)
    ACCEL(100)
    SPEED(SPEED_LOW)
    WAIT_TIME(1000)
    ACCEL(20)
    SPEED(40)
    WAIT_SENSOR(2)
    ACCEL(5)
    SPEED(SPEED_STOP)
    WAIT_TIME(2000)
};

// ---------------- 制御 ----------------

Type_Service gbService = {SVC_STATE_IDLE, 1, SVC_SEG_NONE, 0, 0};
volatile int gbSvcStartRequest = 0;
volatile int gbSvcRetRequest = 0;

// 前回出発した番線（0 = 電源投入後まだ出発していない）
static int svcLastTrack = 0;

// 割込み内で使う軽量乱数（xorshift32）。種は STRT 押下時にハードウェア乱数から設定
static uint32_t svcRandState = 0x12345678;
static uint32_t svcRand(void){
  uint32_t x = svcRandState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  svcRandState = x;
  return x;
}

#define SVC_LOAD(seg_id, arr) svcLoadSegment(seg_id, arr, sizeof(arr)/sizeof(Type_Scenario))

static void svcLoadSegment(int seg_id, Type_Scenario *arr, int num){
  gbService.segment = seg_id;
  gbScenario = arr;
  gbSystem.scenario_counter_max = num;
  gbSystem.scenario_counter = 0;
  gbSystem.wait_timer = 0;
  flagDrawTFT = true;
}

// 電車を出発させる（番線は交互。電源投入後の最初だけランダム）
static void svcStartTrain(void){
  if(svcLastTrack == 0){
    svcLastTrack = (svcRand() & 0x100) ? 2 : 1;  // 「前回」をランダムに決める
  }
  svcLastTrack = (svcLastTrack == 1) ? 2 : 1;
  gbService.track = svcLastTrack;
  gbService.lapCount = 0;
  gbService.state = SVC_STATE_RUNNING;
  gbSvcRetRequest = 0;
  if(gbService.track == 1){ SVC_LOAD(SVC_SEG_DEPART, svcDepart1); }
  else                    { SVC_LOAD(SVC_SEG_DEPART, svcDepart2); }
  gbTrain.targetSpeed = gbTrain.speed; // 現在値から加減速
  gbSystem.mode = SYSTEM_MODE_SERVICE;
  gbIsHC595Update = true;

  // LOOP ON 中の出発回数をカウントし、上限に達したら LOOP を OFF に戻す
  // （この電車は通常どおり帰着し、次の電車は出発しない）
  if(gbSystem.isEndless == true){
    gbService.departCount++;
    if(gbService.departCount >= SVC_LOOP_MAX_DEPARTS){
      gbSystem.isEndless = false;
    }
  }
  flagDrawTFT = true;
}

// ホーム到着時：LOOP ON なら次の電車を出発、OFF なら停止状態へ
static void svcFinish(void){
  if(gbSystem.isEndless == true){
    svcStartTrain();
    return;
  }
  gbService.state = SVC_STATE_IDLE;
  gbService.segment = SVC_SEG_NONE;
  gbSystem.mode = SYSTEM_MODE_MANUAL;
  gbSystem.scenario_counter = 0;
  gbSystem.wait_timer = 0;
  gbTrain.targetSpeed = 0;
  flagDrawTFT = true;
}

// RET 受付（ボタン押下 / 規定周回数で自動）
static void svcRequestReturn(void){
  if(gbService.state == SVC_STATE_RUNNING){
    gbService.state = SVC_STATE_RETURNING;
    flagDrawTFT = true;
  }
}

// 1ms 割込みから毎回呼ぶ：loop() からの要求を処理
void ServiceHandleRequests(void){
  // STRT
  if(gbSvcStartRequest != 0){
    gbSvcStartRequest = 0;
    if(gbService.state == SVC_STATE_IDLE && gbSystem.mode == SYSTEM_MODE_MANUAL){
      svcStartTrain();
    }
  }
  // RET
  if(gbSvcRetRequest != 0){
    gbSvcRetRequest = 0;
    svcRequestReturn();
  }
  // MAN ボタンで SERVICE モードを抜けたら、その場で中止（列車・ポイント等はそのまま）
  if(gbSystem.mode != SYSTEM_MODE_SERVICE && gbService.state != SVC_STATE_IDLE){
    gbService.state = SVC_STATE_IDLE;
    gbService.segment = SVC_SEG_NONE;
    flagDrawTFT = true;
  }
}

// セグメント終了時に HandleAutoDriveScenario() から呼ばれる：次のセグメントを選ぶ
void ServiceNextSegment(void){
  switch(gbService.segment){
    // センサ7 通過：駅B に停車 or 通過（RET 受付済みなら必ず停車して帰還）
    case SVC_SEG_DEPART:
    case SVC_SEG_PASS_A:
    case SVC_SEG_LEAVE_A:
      // 周回数カウント（最初のセンサ7 通過 = 1）。規定周回を終えたら自動で RET
      gbService.lapCount++;
      if(gbService.lapCount > SVC_AUTO_RET_LAPS){
        svcRequestReturn();
      }
      if(gbService.state == SVC_STATE_RETURNING
         || (svcRand() % 100) < SVC_STOP_PERCENT){
        SVC_LOAD(SVC_SEG_STOP_B, svcStopB);
      }
      else{
        SVC_LOAD(SVC_SEG_PASS_B, svcPassB);
      }
      break;

    // 駅B 停車時間終了：RET 受付済みなら方向転換して帰還、それ以外は発車
    case SVC_SEG_STOP_B:
      if(gbService.state == SVC_STATE_RETURNING){
        SVC_LOAD(SVC_SEG_RETURN, svcReturn);
      }
      else{
        SVC_LOAD(SVC_SEG_LEAVE_B, svcLeaveB);
      }
      break;

    // センサ8 通過：駅A に停車 or 通過（帰還待ちの間は駅A には寄らない）
    case SVC_SEG_PASS_B:
    case SVC_SEG_LEAVE_B:
      if(gbService.state != SVC_STATE_RETURNING
         && (svcRand() % 100) < SVC_STOP_PERCENT){
        SVC_LOAD(SVC_SEG_STOP_A, svcStopA);
      }
      else{
        SVC_LOAD(SVC_SEG_PASS_A, svcPassA);
      }
      break;

    case SVC_SEG_STOP_A:
      SVC_LOAD(SVC_SEG_LEAVE_A, svcLeaveA);
      break;

    case SVC_SEG_RETURN:
      if(gbService.track == 1){ SVC_LOAD(SVC_SEG_ARRIVE, svcArrive1); }
      else                    { SVC_LOAD(SVC_SEG_ARRIVE, svcArrive2); }
      break;

    case SVC_SEG_ARRIVE: // ホーム到着
    default:
      svcFinish();
      break;
  }
}

// loop() から：STRT/RET ボタンが押された
void ServiceButtonPressed(void){
  if(gbService.state == SVC_STATE_IDLE){
    if(gbSystem.mode == SYSTEM_MODE_MANUAL){
      svcRandState = rp2040.hwrand32() | 1;  // 乱数の種（0 以外）
      gbSvcStartRequest = 1;
    }
  }
  else if(gbService.state == SVC_STATE_RUNNING){
    gbSvcRetRequest = 1;
  }
  // RETURNING 中は無視
}

// loop() から：LOOP ボタンで ON にされた
void ServiceLoopTurnedOn(void){
  gbService.departCount = 0;
}

// 描画前に呼ぶ：状態に応じてボタン表示を切り替える
void ServiceUpdateButton(void){
  Type_TFTButton *bt = &tftButton[TFT_BUTTON_SERVICE];
  if(gbService.state == SVC_STATE_IDLE){
    strcpy(bt->text, "STRT");
    bt->text_offset_x = 5;
    bt->color_frame = ILI9341_GREENYELLOW;
  }
  else{
    if(gbService.state == SVC_STATE_RETURNING){
      // RET 受付後（駅B で方向転換してホームへ帰還中）
      strcpy(bt->text, "B2ST");
      bt->text_offset_x = 5;
      bt->color_frame = ILI9341_GREY127;
    }
    else{
      strcpy(bt->text, "RET");
      bt->text_offset_x = 12;
      bt->color_frame = ILI9341_CYAN;
    }
  }
}
