#include <thread>
#include "DxLib.h"
#include "json.hpp"
#include "httplib.h"

static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;

static double roll = 0.0; // 左右の傾き
static double pitch = 0.0; // 前後の傾き
static double yaw = 0.0; // 方向
static double speed = 0.0; // 対気速度
static int altitude = 0; // 高度（cm）
static int rpm = 0; // 回転数（rpm/min）

void get_data() {
    // 通信関係のこととかを色々書く
    roll = GetRand(10) - 5;
    pitch = GetRand(10) - 5;
    yaw = GetRand(10) - 5;
    speed = GetRand(10);
    altitude = GetRand(1000);
    rpm = GetRand(300);
}

int android_main() {

    // 画面の解像度、カラービットの設定
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    // 背景色の指定
    SetBackgroundColor(64, 64, 64);
    // DXライブラリの初期化
    DxLib_Init();
    // 描画先を裏画面に設定
    SetDrawScreen(DX_SCREEN_BACK);

    // ここで画像のロード、初期設定を行う
    int font = CreateFontToHandle(NULL, 400, 50);
    int map = LoadGraph("biwako.png");
    int count = 0;
    int width = 50;

    // 色の定義
    const unsigned int green = GetColor(0, 255, 0);
    const unsigned int red = GetColor(255, 0, 0);
    const unsigned int blue = GetColor(0, 0, 255);

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
        if (pitch > 3.0) { color_top = red; }
        else if (pitch > 0.5) { color_top = green; }
        else { color_top = blue; }
        if (pitch < -3.0) { color_bottom = red; }
        else if (pitch < -0.5) { color_bottom = green; }
        else { color_bottom = blue; }
        if (roll > 3.0) { color_right = red; }
        else if (roll > 0.5) { color_right = green; }
        else { color_right = blue; }
        if (roll < -3.0) { color_left = red; }
        else if (roll < -0.5) { color_left = green; }
        else { color_left = blue; }

        DrawBox(0, 0, SCREEN_WIDTH, width, color_top, true);
        DrawBox(0, 0, width, SCREEN_HEIGHT, color_left, true);
        DrawBox(0, SCREEN_HEIGHT - width, SCREEN_WIDTH, SCREEN_HEIGHT, color_bottom, true);
        DrawBox(SCREEN_WIDTH - width, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color_right, true);
    }
    // https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/136.15,35.35,10.5,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}