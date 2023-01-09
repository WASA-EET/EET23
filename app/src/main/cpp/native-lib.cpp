#include <thread>
#include <vector>
#include "DxLib.h"
#include "json.hpp"
#include "httplib.h"

#define BIWAKO
// #define HUJIKAWA
// #define OOTONE

static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;
static const unsigned int GREEN = GetColor(0, 255, 0);
static const unsigned int RED = GetColor(255, 0, 0);
static const unsigned int BLUE = GetColor(0, 0, 255);

#if defined(BIWAKO)
static const char *IMAGE_PATH = "biwako.png";
static const double C_LAT = 35.35; // 中心の緯度
static const double C_LON = 136.15; // 中心の経度
static const double X_SCALE = 4100.0; // X座標の拡大率
static const double Y_SCALE = -5050.0; // Y座標の拡大率
#elif defined(HUJIKAWA)
static const char* IMAGE_PATH = "hujikawa.png";
static const double C_LAT = 0.0; // 中心の緯度
static const double C_LON = 0.0; // 中心の経度
static const double X_SCALE = 10000.0; // X座標の拡大率
static const double Y_SCALE = 10000.0; // Y座標の拡大率
#elif defined(OOTONE)
static const char* IMAGE_PATH = "ootone.png";
static const double C_LAT = 0.0; // 中心の緯度
static const double C_LON = 0.0; // 中心の経度
static const double X_SCALE = 10000.0; // X座標の拡大率
static const double Y_SCALE = 10000.0; // Y座標の拡大率
#endif

std::vector<int> trajectory_x; //可変長ベクトル x成分
std::vector<int> trajectory_y; //可変長ベクトル y成分
static double roll = 0.0; // 左右の傾き
static double pitch = 0.0; // 前後の傾き
static double yaw = 0.0; // 方向
static double speed = 0.0; // 対気速度
static int altitude = 0; // 高度（cm）
static int rpm = 0; // 回転数（rpm/min）
static double latitude = 35.199357; // 緯度
static double longitude = 136.050708; // 経度

void get_data() {
    // 通信関係のこととかを色々書く
    roll = GetRand(10) - 5;
    pitch = GetRand(10) - 5;
    yaw = GetRand(10) - 5;
    speed = GetRand(10);
    altitude = GetRand(1000);
    rpm = GetRand(300);

    // 35.199357, 136.050708
    //latitude = 35.199357;
    //longitude = 136.050708;
    latitude += 0.01;
    longitude += 0.01;
}

int android_main() {

    // 画面の解像度、カラービットの設定
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    // 背景色の指定
    SetBackgroundColor(0, 0, 0);
    // DXライブラリの初期化
    DxLib_Init();
    // 描画先を裏画面に設定
    SetDrawScreen(DX_SCREEN_BACK);

    // ここで画像のロード、初期設定を行う
    int font = CreateFontToHandle(nullptr, 400, 50);
    int map = LoadGraph(IMAGE_PATH);
    int count = 0;
    int bar_width = 50;

    // マニフェストに <uses-permission android:name="android.permission.INTERNET" /> の記載をお忘れなく
    //httplib::Client cli("http://192.168.4.1");
    //auto res = cli.Get("/GetMeasurementData"); // マルチスレッド必須

    // 1秒間に60回繰り返される
    while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0) {
        count++;

        // 1秒に1回実行される
        if (count % 60 == 0) {
            get_data();
        }

        // 地図の表示
        DrawExtendGraph(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, map, false);

        // 数値の表示
        int wide;
        wide = GetDrawFormatStringWidthToHandle(font, "%.1f", speed);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 100,
                                 GetColor(255, 255, 255), font, "%.1f", speed);
        wide = GetDrawFormatStringWidthToHandle(font, "%d", altitude);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 900,
                                 GetColor(255, 255, 255), font, "%d", altitude);
        wide = GetDrawFormatStringWidthToHandle(font, "%d", rpm);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 1700,
                                 GetColor(255, 255, 255), font, "%d", rpm);

        // ロールとピッチに応じて色を変える（0.5度以下→青、0.5~3.0度→緑、3.0度以上→赤）
        unsigned int color_top, color_bottom, color_left, color_right;
        if (pitch > 3.0) { color_top = RED; }
        else if (pitch > 0.5) { color_top = GREEN; }
        else { color_top = BLUE; }
        if (pitch < -3.0) { color_bottom = RED; }
        else if (pitch < -0.5) { color_bottom = GREEN; }
        else { color_bottom = BLUE; }
        if (roll > 3.0) { color_right = RED; }
        else if (roll > 0.5) { color_right = GREEN; }
        else { color_right = BLUE; }
        if (roll < -3.0) { color_left = RED; }
        else if (roll < -0.5) { color_left = GREEN; }
        else { color_left = BLUE; }

        // 描画
        DrawBox(0, 0, SCREEN_WIDTH, bar_width, color_top, true);
        DrawBox(0, 0, bar_width, SCREEN_HEIGHT, color_left, true);
        DrawBox(0, SCREEN_HEIGHT - bar_width, SCREEN_WIDTH, SCREEN_HEIGHT, color_bottom, true);
        DrawBox(SCREEN_WIDTH - bar_width, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color_right, true);

        // 緯度経度をピクセルの座標に変換する
        int x = (int) ((longitude - C_LON) * X_SCALE);
        int y = (int) ((latitude - C_LAT) * Y_SCALE);
        x += SCREEN_WIDTH / 2;
        y += SCREEN_HEIGHT / 2;

        // 現在地の表示
        DrawCircle(x, y, 5, GREEN);

        // 座標履歴（画面の座標）の追加
        if (x > 0 && x < SCREEN_WIDTH && y > 0 && y < SCREEN_HEIGHT) {
            if (trajectory_x.empty() || trajectory_y.empty() || trajectory_x.back() != x || trajectory_y.back() != y) {
                trajectory_x.push_back(x);
                trajectory_y.push_back(y);
            }
        }

        // 軌跡の描画
        for (int i = 1; i < trajectory_x.size(); i++) {
            DrawLine(trajectory_x[i - 1], trajectory_y[i - 1], trajectory_x[i], trajectory_y[i],RED, 5);
        }
    }

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}