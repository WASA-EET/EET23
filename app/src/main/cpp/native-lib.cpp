#include "DxLib.h"
#include "json.hpp"
static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;

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

    // 1秒間に60回繰り返される
    while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0) {

    }

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}