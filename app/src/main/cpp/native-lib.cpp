#include "DxLib.h"
#include "json.hpp"
#include "httplib.h"

static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;

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
    int small_font = CreateFontToHandle(nullptr, 40, 50);
    int map = LoadGraph("biwako.png");
    int count = 0;

    // 1秒間に60回繰り返される
    while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0) {
        DrawExtendGraph(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, map, false);

        int str_width = GetDrawFormatStringWidthToHandle(font, "%d", count / 60);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - str_width / 2, 0, GetColor(255, 255, 255), font, "%d", count / 60);
        count++;

        DrawLine(0, 0, 0, 0, 0xffffffff, 1);
    }

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}