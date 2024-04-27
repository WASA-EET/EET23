#include <thread>
#include <string>
#include <fstream>
#include "DxLib.h"
#include "json.hpp"
#include "httplib.h"
#include "hmac_sha256.hpp"
#include "base64.hpp"

/*
 *  リポジトリ：https://github.com/WASA-EET/EET23
 */

// マイコンネットワークに接続しない場合のテスト用
// #define TEST_CASE

static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;
static const unsigned int COLOR_BLACK = GetColor(0x00, 0x00, 0x00);
static const unsigned int COLOR_RED = GetColor(0xff, 0x4b, 0x00);
static const unsigned int COLOR_YELLOW_RED = GetColor(0xf6, 0xaa, 0x00);
static const unsigned int COLOR_YELLOW = GetColor(0xf2, 0xe7, 0x00);
static const unsigned int COLOR_GREEN = GetColor(0x00, 0xb0, 0x6b);
static const char *IMAGE_PLANE_PATH = "plane.png";
static const double DEFAULT_PITCH = 0.0;

// MapBoxにおける倍率は指数なので、以下の式から倍率を導出する。（パラメータは試行錯誤で出した）
// X座標の倍率=2.8312×(2^MapBoxの倍率)
// Y座標の倍率=-3.5217×(2^MapBoxの倍率)
enum {
    PLACE_BIWAKO,
    PLACE_HUJIKAWA,
    PLACE_OOTONE,
    PLACE_MAX,
};
// 各場所のURL（琵琶湖、富士川、大利根）※ MAPBOXからダウンロード可能
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/136.15,35.35,10.5,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/138.6315,35.121,16.25,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/140.2412,35.8594,15.75,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
static const char *IMAGE_MAP_PATH[PLACE_MAX] = {"biwako.png", "hujikawa.png", "ootone.png"};
static const double C_LAT[PLACE_MAX] = {35.35, 35.121, 35.8594}; // 中心の緯度
static const double C_LON[PLACE_MAX] = {136.15, 138.6315, 140.2412}; // 中心の経度
static const double X_SCALE[PLACE_MAX] = {4100.0, 220650.0, 156500.0}; // X座標の拡大率
static const double Y_SCALE[PLACE_MAX] = {-5050.0, -274500.0, -194000.0}; // Y座標の拡大率

static int current_place = 0;

std::string JsonString_Sensor;
std::string JsonString_Server;
nlohmann::json JsonInput_Sensor;
nlohmann::json JsonInput_Server;
// 参考：https://qiita.com/yohm/items/0f389ba5c5de4e2df9cf

// マイコンから収集したデータ
std::vector<int> trajectory_x; // 可変長ベクトル x成分
std::vector<int> trajectory_y; // 可変長ベクトル y成分
static double roll = 0.0; // 左右の傾き
static double pitch = 0.0; // 前後の傾き
static double yaw = 0.0; // 方向
static double speed = 0.0; // 対気速度
static double altitude = 0; // 高度（m）
static int rpm = 0; // ペラ回転数（rpm）
static int power = 0; // 出力（watt）
static double latitude = 0.0; // 緯度
static double longitude = 0.0; // 経度

// サーバーから収集したデータ
struct WIND {
    // TODO: 各要素宣言
};
/*
 * 風向・風速のJSON構造は以下の通りとする
 * {
 *   "data_num" : "データ数"
 *   "records" [
 *     {
 *       "AID" : "風速計のID"
 *       "Latitude" : "緯度"
 *       "Longitude" : "経度"
 *       "WindSpeed" : "風速"
 *       "WindDirection" : "風向"
 *     },
 *     {
 *     ...以降要素数だけ同じ構造のデータが並ぶ
 *   ]
 * }
 * */
// ref: https://github.com/nlohmann/json/issues/716
std::vector<WIND> winds; // 風速・風向

// 外部ストレージでアクセスできるのは「/storage/emulated/0/Android/data/[パッケージ名]/files/」に限られる
static const std::string LOG_DIRECTORY = "/storage/emulated/0/Android/data/com.wasa.eet23/files/";
static const std::string LOG_EXTENSION = ".csv";
static bool log_state = false;
static std::ofstream ofs;

std::string time_string() {
    DATEDATA Date;
    GetDateTime(&Date);
    return
            std::to_string(Date.Year) + "_" +
            std::to_string(Date.Mon) + "_" +
            std::to_string(Date.Day) + "_" +
            std::to_string(Date.Hour) + "_" +
            std::to_string(Date.Min) + "_" +
            std::to_string(Date.Sec);
}

// ログ保存用
void start_log() {
    std::string path = LOG_DIRECTORY + time_string() + LOG_EXTENSION;
    // フォルダが存在しないとファイルを作成できないので予めフォルダは作っておくこと！
    // あと権限の設定もお忘れなく（これに関しては端末側で有効化する必要もある）
    ofs.open(path);
    if (!ofs) {
        clsDx();
        printfDx("Failed to open %s\n", path.c_str());
        return;
    }

    // とりあえず1行目を埋める
    ofs
            << "Date, Time, Latitude, Longitude, GPSAltitude, GPSCourse, GPSSpeed, Roll, Pitch, Yaw, Temperature, Pressure, GroundPressure, DPSAltitude, Altitude, AirSpeed, PropellerRotationSpeed, Cadence, Power, Rudder, Elevator, RunningTime"
            << std::endl;

    std::thread ofs_thread = std::thread([]() {
        while (ofs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ofs << JsonInput_Sensor["Year"] << "/";
            ofs << JsonInput_Sensor["Month"] << "/";
            ofs << JsonInput_Sensor["Day"] << ", ";
            ofs << JsonInput_Sensor["Hour"] << ":";
            ofs << JsonInput_Sensor["Minute"] << ":";
            ofs << JsonInput_Sensor["Second"] << ", ";
            ofs << JsonInput_Sensor["Latitude"] << ", ";
            ofs << JsonInput_Sensor["Longitude"] << ", ";
            ofs << JsonInput_Sensor["GPSAltitude"] << ", ";
            ofs << JsonInput_Sensor["GPSCourse"] << ", ";
            ofs << JsonInput_Sensor["GPSSpeed"] << ", ";
            ofs << JsonInput_Sensor["Roll"] << ", ";
            ofs << JsonInput_Sensor["Pitch"] << ", ";
            ofs << JsonInput_Sensor["Yaw"] << ", ";
            ofs << JsonInput_Sensor["Temperature"] << ", ";
            ofs << JsonInput_Sensor["Pressure"] << ", ";
            ofs << JsonInput_Sensor["GroundPressure"] << ", ";
            ofs << JsonInput_Sensor["DPSAltitude"] << ", ";
            ofs << JsonInput_Sensor["Altitude"] << ", ";
            ofs << JsonInput_Sensor["AirSpeed"] << ", ";
            ofs << JsonInput_Sensor["PropellerRotationSpeed"] << ", ";
            ofs << JsonInput_Sensor["Cadence"] << ", ";
            ofs << JsonInput_Sensor["Power"] << ", ";
            ofs << JsonInput_Sensor["Rudder"] << ", ";
            ofs << JsonInput_Sensor["Elevator"] << ", ";
            ofs << JsonInput_Sensor["RunningTime"] << ", ";
            ofs << std::endl;
        }
        log_state = false;
    });
    ofs_thread.detach();

    log_state = true;
}

void stop_log() {
    ofs.close();
    log_state = false;
}


void get_json_data() {
#ifdef TEST_CASE
    roll = GetRand(10) - 5;
    pitch = GetRand(10) - 5;
    yaw = GetRand(10) - 5;
    speed = GetRand(10);
    altitude = GetRand(1000);
    rpm = GetRand(300);

    // 35.199357, 136.050708
    // latitude = 35.199357;
    // longitude = 136.050708;
    latitude += 0.0001;
    longitude += 0.0008;
#else
    // 通信関係のこととかを色々書く
    try {
        if (JsonString_Sensor.empty())
            return;
        JsonInput_Sensor = nlohmann::json::parse(JsonString_Sensor);
        roll = JsonInput_Sensor["Roll"];
        pitch = JsonInput_Sensor["Pitch"];
        yaw = JsonInput_Sensor["Yaw"];
        speed = JsonInput_Sensor["AirSpeed"];
        altitude = JsonInput_Sensor["Altitude"];
        rpm = JsonInput_Sensor["PropellerRotationSpeed"];
        latitude = JsonInput_Sensor["Latitude"];
        longitude = JsonInput_Sensor["Longitude"];

        if (JsonString_Server.empty())
            return;
        JsonInput_Server = nlohmann::json::parse(JsonString_Server);
        // TODO: ベクトル長変更、データ代入

    }
    catch (nlohmann::json::exception &e) {
        // 文字列をJsonに変換できない場合に行う処理
        // Jsonに入っているべきデータが存在しない場合
        clsDx();
        printfDx(e.what());
    }
#endif
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
    int image_map[PLACE_MAX];
    for (int i = 0; i < PLACE_MAX; i++) {
        image_map[i] = LoadGraph(IMAGE_MAP_PATH[i]);
    }
    int image_plane = LoadGraph(IMAGE_PLANE_PATH);
    int touch_time = 0; // 連続でタッチされている時間
    int bar_width = 50;

    // マニフェストに <uses-permission android:name="android.permission.INTERNET" /> の記載をお忘れなく

    std::thread microcontroller_http_thread = std::thread([]() {
        httplib::Client cli_microcontroller("http://192.168.4.1"); // このアドレスは変える。MDNSはAndroidでは使えない

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifndef TEST_CASE
            httplib::Result res_data = cli_microcontroller.Get("/GetMeasurementData");
            if (res_data) JsonString_Sensor = res_data->body;
#endif
            get_json_data();
        }
    });
    microcontroller_http_thread.detach();

    std::thread server_http_thread = std::thread([]() {
        httplib::Client cli_server("http://anemometer.staging.tyama.mydns.jp");
        const std::string PASSWORD = "LMAJjvOi";
        uint8_t KEY[32];
        uint8_t HMAC[32];
        sha256(PASSWORD.data(), PASSWORD.size(), KEY, 32);

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // 風速・風向をサーバーから取得
            httplib::Result res_data = cli_server.Get("/data/get");
            if (res_data) JsonString_Server = res_data->body;

            // HMAC認証符号を追加してサーバーにPOST
            std::string hmac_base64;
            hmac_sha256(KEY, sizeof(KEY), JsonString_Sensor.data(), JsonString_Sensor.size(), HMAC, sizeof(HMAC));
            algorithm::encode_base64(std::vector<uint8_t>(HMAC, HMAC + sizeof(HMAC)), hmac_base64);
            httplib::Headers headers = { { "Authorization", hmac_base64 } };
            auto res = cli_server.Post("/data/create/", headers, JsonString_Sensor, "application/json");

            if (res->status == httplib::StatusCode::Created_201) {
                // TODO: 右上塗りつぶしON

            } else {
                // TODO: 塗りつぶしフラグOFF

                clsDx();
                printfDx(res->body.c_str());
            }
        }
    });
    server_http_thread.detach();

    // 1秒間に60回繰り返される
    while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0) {

        // 地図の表示
        DrawExtendGraph(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, image_map[current_place], false);

        // 数値の表示
        int wide;
        wide = GetDrawFormatStringWidthToHandle(font, "%.1f", speed);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 200,
                                 GetColor(255, 255, 255), font, "%.1f", speed);
        wide = GetDrawFormatStringWidthToHandle(font, "%.2f", altitude);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 700,
                                 GetColor(255, 255, 255), font, "%.2f", altitude);
        wide = GetDrawFormatStringWidthToHandle(font, "%d", rpm);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 1200,
                                 GetColor(255, 255, 255), font, "%d", rpm);
        wide = GetDrawFormatStringWidthToHandle(font, "%d", power);
        DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 1700,
                                 GetColor(255, 255, 255), font, "%d", power);

        // ロールとピッチに応じて色を変える
        // （1.0度以下→緑、1.0~2.0度→黄色、2.0~3.0度→オレンジ、3.0度以上→赤）
        unsigned int color_top, color_bottom, color_left, color_right;
        if (pitch - DEFAULT_PITCH > 3.0) { color_top = COLOR_RED; }
        else if (pitch - DEFAULT_PITCH > 2.0) { color_top = COLOR_YELLOW_RED; }
        else if (pitch - DEFAULT_PITCH > 1.0) { color_top = COLOR_YELLOW; }
        else { color_top = COLOR_GREEN; }
        if (pitch - DEFAULT_PITCH < -3.0) { color_bottom = COLOR_RED; }
        else if (pitch - DEFAULT_PITCH < -2.0) { color_bottom = COLOR_YELLOW_RED; }
        else if (pitch - DEFAULT_PITCH < -1.0) { color_bottom = COLOR_YELLOW; }
        else { color_bottom = COLOR_GREEN; }
        if (roll > 3.0) { color_right = COLOR_RED; }
        else if (roll > 2.0) { color_right = COLOR_YELLOW_RED; }
        else if (roll > 1.0) { color_right = COLOR_YELLOW; }
        else { color_right = COLOR_GREEN; }
        if (roll < -3.0) { color_left = COLOR_RED; }
        else if (roll < -2.0) { color_left = COLOR_YELLOW_RED; }
        else if (roll < -1.0) { color_left = COLOR_YELLOW; }
        else { color_left = COLOR_GREEN; }

        // 描画
        DrawBox(0, 0, SCREEN_WIDTH, bar_width, color_top, true);
        DrawBox(0, 0, bar_width, SCREEN_HEIGHT, color_left, true);
        DrawBox(0, SCREEN_HEIGHT - bar_width, SCREEN_WIDTH, SCREEN_HEIGHT, color_bottom, true);
        DrawBox(SCREEN_WIDTH - bar_width, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color_right, true);

        // 緯度経度をピクセルの座標に変換する
        int x = (int) ((longitude - C_LON[current_place]) * X_SCALE[current_place]);
        int y = (int) ((latitude - C_LAT[current_place]) * Y_SCALE[current_place]);
        x += SCREEN_WIDTH / 2;
        y += SCREEN_HEIGHT / 2;

        // 現在地に矢印を表示
        int w, h;
        GetGraphSize(image_plane, &w, &h);
        DrawRotaGraph(x, y, 0.3, yaw, image_plane, true);
        // DrawCircle(x, y, 5, RED);

        // 軌跡の追加
        if (x > 0 && x < SCREEN_WIDTH && y > 0 && y < SCREEN_HEIGHT) {
            if (trajectory_x.empty() || trajectory_y.empty() || trajectory_x.back() != x ||
                trajectory_y.back() != y) {
                trajectory_x.push_back(x);
                trajectory_y.push_back(y);
            }
        }

        // 軌跡の描画
        for (int i = 1; i < trajectory_x.size(); i++) {
            DrawLine(trajectory_x[i - 1], trajectory_y[i - 1], trajectory_x[i], trajectory_y[i],
                     COLOR_RED, 5);
        }

        // 画面にタッチされている場合（タッチパネルのタッチされている箇所の数が1以上の場合）
        if (GetTouchInputNum() > 0)
            touch_time++;
        else
            touch_time = 0;

        // 1秒以上連続2本以上の指で画面に触れたら
        if (touch_time > 60) {
            touch_time = 0;
            // 1本なら軌跡を削除
            if (GetTouchInputNum() == 1) {
                trajectory_x.clear();
                trajectory_y.clear();
            }
            // 2本の場合はログの記録を開始（または停止）
            if (GetTouchInputNum() == 2) {
                if (!log_state)
                    start_log();
                else
                    stop_log();
            }
            // 3本の場合は地図の切り替え
            if (GetTouchInputNum() == 3) {
                current_place = (current_place + 1) % PLACE_MAX;
                trajectory_x.clear();
                trajectory_y.clear();
            }
        }

        // ログを収集していなければ左上に四角を描画
        if (!log_state) {
            DrawBox(0, 0, bar_width, bar_width, 0xffffffff, true);
        }

        // TODO: サーバーと通信できているかどうかの確認用に右上を塗りつぶす

    }

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}