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
 *  C++ Version ... C++20 （CMakeLists.txtでの設定が必須）
 */

// マイコンネットワークに接続しない場合のテスト用
// #define TEST_CASE

// サーバーにアップロードしない場合
// #define NO_UPLOAD

// 風向・風速の表示
// #define SHOW_WIND

static const std::string PLANE_AID = "7777";

static const int SCREEN_WIDTH = 1080;
static const int SCREEN_HEIGHT = 2340;
[[maybe_unused]] static const unsigned int COLOR_BLACK = GetColor(0x00, 0x00, 0x00);
[[maybe_unused]] static const unsigned int COLOR_RED = GetColor(0xff, 0x4b, 0x00);
[[maybe_unused]] static const unsigned int COLOR_YELLOW_RED = GetColor(0xf6, 0xaa, 0x00);
[[maybe_unused]] static const unsigned int COLOR_YELLOW = GetColor(0xf2, 0xe7, 0x00);
[[maybe_unused]] static const unsigned int COLOR_GREEN = GetColor(0x00, 0xb0, 0x6b);
[[maybe_unused]] static const unsigned int COLOR_WHITE = GetColor(0xff, 0xff, 0xff);
static const char *IMAGE_CURRENT_PATH = "current.png";
static const char *IMAGE_ARROW_PATH = "arrow.png";
static const char *AUDIO_START_PATH = "start.wav";
static const char *AUDIO_STOP_PATH = "stop.wav";
static const char *AUDIO_WARNING1_PATH = "warning1.wav";
static const char *AUDIO_WARNING2_PATH = "warning2.wav";

static const double START_POINT[2] = {136.254344, 35.294230};
static const double TURNING_POINT[2][2] = {{136.124324, 35.416626}, {136.063712, 35.250789}};

// MapBoxにおける倍率は指数なので、以下の式から倍率を導出する。（パラメータは試行錯誤で出した）
// X座標の倍率=2.8312×(2^MapBoxの倍率)
// Y座標の倍率=-3.5217×(2^MapBoxの倍率)
enum [[maybe_unused]] {
    PLACE_BIWAKO,
    PLACE_HUJIKAWA,
    PLACE_OOTONE,
    PLACE_MAX,
};
// 各場所のURL（琵琶湖、富士川、大利根）※ MAPBOXからダウンロード可能
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/136.16,35.35,10.8,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/138.6315,35.121,16.25,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
// https://api.mapbox.com/styles/v1/mapbox/dark-v10/static/140.2412,35.8594,15.75,0/540x1170?access_token=pk.eyJ1IjoiMjFrbTQiLCJhIjoiY2xhdHFmM3BpMDB0NTNxcDl3b2pqN3Q1ZyJ9.8jqJf75DqkkTv5IYb8c1Pg
static const char *IMAGE_MAP_PATH[PLACE_MAX] = {"biwako.png", "hujikawa.png", "ootone.png"};
static const double C_LAT[PLACE_MAX] = {35.35, 35.121, 35.8594}; // 中心の緯度
static const double C_LON[PLACE_MAX] = {136.16, 138.6315, 140.2412}; // 中心の経度
static const double X_SCALE[PLACE_MAX] = {4900.0, 220650.0, 156500.0}; // X座標の拡大率
static const double Y_SCALE[PLACE_MAX] = {-6000.0, -274500.0, -194000.0}; // Y座標の拡大率

static int current_place = 0;

// std::string JSON_SAMPLE = "{\"AID\": \"7777\", \"Time\": \"2024-07-25 16:35:23\", \"data\": {\"Yaw\": 209.3125, \"Roll\": -7.8125, \"Trim\": -7, \"Pitch\": -3.625, \"Rudder\": 0, \"AirSpeed\": 0, \"Altitude\": 344, \"Elevator\": 0.514500022, \"GPSSpeed\": 0, \"Latitude\": 0, \"Pressure\": 999.5440674, \"GPSCourse\": 0, \"Longitude\": 0, \"DPSAltitude\": 159.7719879, \"GPSAltitude\": 0, \"Temperature\": 31.6811142, \"GroundPressure\": 1018.690002, \"PropellerRotationSpeed\": 0}}";
std::string JsonString_Sensor;
nlohmann::json JsonInput_Sensor;
#ifdef SHOW_WIND
std::string JsonString_Server;
nlohmann::json JsonInput_Server;
#endif
// 参考：https://qiita.com/yohm/items/0f389ba5c5de4e2df9cf

// マイコンから収集したデータ
std::vector<int> trajectory_x; // 可変長ベクトル x成分
std::vector<int> trajectory_y; // 可変長ベクトル y成分
static double roll = 0.0; // 左右の傾き
static double pitch = 0.0; // 前後の傾き
static double yaw = 0.0; // 方向
static double standard_roll = 0.0;
static double standard_pitch = 0.0;
static double gpsCourse = 0.0; // GPSの方向
static double speed = 0.0; // 対気速度
static double altitude = 0; // 高度（m）
static int rpm = 0; // ペラ回転数（rpm）
static double latitude = 0.0; // 緯度
static double longitude = 0.0; // 経度
static int distance = 0.0; // プラットホームからの距離

// サーバーから収集したデータ
struct WIND {
    std::string WindSpeed;
    std::string WindDirection;
    std::string Latitude;
    std::string Longitude;
};

std::vector<WIND> winds; // 風速・風向

// 外部ストレージでアクセスできるのは「/storage/emulated/0/Android/data/[パッケージ名]/files/」に限られる
static const std::string LOG_DIRECTORY = "/storage/emulated/0/Android/data/com.wasa.eet23/files/";
static const std::string LOG_EXTENSION = ".csv";
static bool log_state = false;
static int log_count = 0;
static const int LOG_START_STOP_MARK_TIME = 60;
static std::ofstream ofs;

double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

double RX = 6378.137; // 回転楕円体の長半径（赤道半径）[km]
double RY = 6356.752; // 回転楕円体の短半径（極半径) [km]

double cal_distance(double x1, double y1, double x2, double y2) {
    x1 = deg2rad(x1);
    x2 = deg2rad(x2);
    y1 = deg2rad(y1);
    y2 = deg2rad(y2);

    double dx = x2 - x1, dy = y2 - y1;
    double mu = (y1 + y2) / 2.0; // μ
    double E = sqrt(1 - pow(RY / RX, 2.0)); // 離心率
    double W = sqrt(1 - pow(E * sin(mu), 2.0));
    double M = RX * (1 - pow(E, 2.0)) / pow(W, 3.0); // 子午線曲率半径
    double N = RX / W; // 卯酉線曲率半径
    return sqrt(pow(M * dy, 2.0) + pow(N * dx * cos(mu), 2.0)) * 1000; // 距離[m]
}

// ログ保存用
void start_log() {

    // 時刻取得、文字列変換
    char time_string[] = "19700101000000";
    DATEDATA Date;
    GetDateTime(&Date);
    sprintf(time_string, "%04d%02d%02d%02d%02d%02d", Date.Year, Date.Mon, Date.Day, Date.Hour,
            Date.Min, Date.Sec);

    std::string path = LOG_DIRECTORY + time_string + LOG_EXTENSION;
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
            << "Time, Latitude, Longitude, GPSAltitude, GPSCourse, GPSSpeed, Roll, Pitch, Yaw, Temperature, Pressure, GroundPressure, DPSAltitude, Altitude, AirSpeed, PropellerRotationSpeed, Rudder, Elevator, Trim, RunningTime"
            << std::endl;

    std::thread ofs_thread = std::thread([]() {
        while (ofs) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ofs << JsonInput_Sensor["Time"] << ", ";
                ofs << JsonInput_Sensor["data"]["Latitude"] << ", ";
                ofs << JsonInput_Sensor["data"]["Longitude"] << ", ";
                ofs << JsonInput_Sensor["data"]["GPSAltitude"] << ", ";
                ofs << JsonInput_Sensor["data"]["GPSCourse"] << ", ";
                ofs << JsonInput_Sensor["data"]["GPSSpeed"] << ", ";
                ofs << roll << ", ";
                ofs << pitch << ", ";
                ofs << JsonInput_Sensor["data"]["Yaw"] << ", ";
                ofs << JsonInput_Sensor["data"]["Temperature"] << ", ";
                ofs << JsonInput_Sensor["data"]["Pressure"] << ", ";
                ofs << JsonInput_Sensor["data"]["GroundPressure"] << ", ";
                ofs << JsonInput_Sensor["data"]["DPSAltitude"] << ", ";
                ofs << JsonInput_Sensor["data"]["Altitude"] << ", ";
                ofs << JsonInput_Sensor["data"]["AirSpeed"] << ", ";
                ofs << JsonInput_Sensor["data"]["PropellerRotationSpeed"] << ", ";
                ofs << JsonInput_Sensor["data"]["Rudder"] << ", ";
                ofs << JsonInput_Sensor["data"]["Elevator"] << ", ";
                ofs << JsonInput_Sensor["data"]["Trim"] << ", ";
                ofs << JsonInput_Sensor["RunningTime"] << ", ";
                ofs << std::endl;
            } catch (...) {
                // catch all exception
            }
        }
        log_state = false;
        log_count = LOG_START_STOP_MARK_TIME;
    });
    ofs_thread.detach();

    log_state = true;
    log_count = LOG_START_STOP_MARK_TIME;
}

void stop_log() {
    ofs.close();
    log_state = false;
    log_count = LOG_START_STOP_MARK_TIME;
}

void get_json_data() {
#ifdef TEST_CASE
    roll = 5;
    pitch = 1;
    yaw = 3;
    speed = 4;
    altitude = 2.2;
    rpm = 10;

    // 35.199357, 136.050708
    // latitude = 35.199357;
    // longitude = 136.050708;
    //latitude += 0.0001;
    //longitude += 0.0008;

    winds.resize(2);

    winds[0].WindSpeed = std::to_string(2);
    winds[0].WindDirection = std::to_string(123);
    winds[0].Longitude = std::to_string(136.175774);
    winds[0].Latitude = std::to_string(35.293717);

    winds[1].WindSpeed = std::to_string(0.5);
    winds[1].WindDirection = std::to_string(321);
    winds[1].Longitude = std::to_string(136.085166);
    winds[1].Latitude = std::to_string(35.315548);
#else
    try {
        // JsonString_Sensor = JSON_SAMPLE;
        if (!JsonString_Sensor.empty()) {
            if (nlohmann::json::accept(JsonString_Sensor)) {
                JsonInput_Sensor = nlohmann::json::parse(JsonString_Sensor);
            }
            roll = JsonInput_Sensor["data"]["Roll"];
            pitch = JsonInput_Sensor["data"]["Pitch"];
            roll = (-1 * roll) - standard_roll;
            pitch = (-1 * pitch) - standard_pitch;
            yaw = JsonInput_Sensor["data"]["Yaw"];
            gpsCourse = JsonInput_Sensor["data"]["GPSCourse"];
            speed = JsonInput_Sensor["data"]["AirSpeed"];
            altitude = JsonInput_Sensor["data"]["Altitude"];
            rpm = JsonInput_Sensor["data"]["PropellerRotationSpeed"];
            latitude = JsonInput_Sensor["data"]["Latitude"];
            longitude = JsonInput_Sensor["data"]["Longitude"];

            if (!log_state && JsonInput_Sensor["LOG"] == "ON") {
                start_log();
            } else if (log_state && JsonInput_Sensor["LOG"] == "OFF") {
                stop_log();
            }
        }
#ifdef SHOW_WIND
        if (!JsonString_Server.empty()) {
            if (nlohmann::json::accept(JsonString_Server)) {
                JsonInput_Server = nlohmann::json::parse(JsonString_Server);
            }
            winds.resize(JsonInput_Server.size());
            int plane_index = -1;
            for (int i = 0; i < winds.size(); i++) {
                if (JsonInput_Server[i]["AID"] == PLANE_AID) {
                    plane_index = i;
                    continue;
                }
                winds[i].WindSpeed = JsonInput_Server[i]["data"]["WindSpeed"];
                winds[i].WindDirection = JsonInput_Server[i]["data"]["WindDirection"];
                winds[i].Longitude = JsonInput_Server[i]["data"]["Longitude"];
                winds[i].Latitude = JsonInput_Server[i]["data"]["Latitude"];
            }
            if (plane_index != -1) {
                winds.erase(winds.begin() + plane_index);
            }
        }
#endif
    }
    catch (nlohmann::json::exception &e) {
        // 文字列をJsonに変換できない場合に行う処理
        // Jsonに入っているべきデータが存在しない場合
        clsDx();
        printfDx(e.what());
    }
    catch (...) {
        // catch all exception
    }
#endif
}

// エントリーポイント、[[maybe_unused]]は警告抑制用
[[maybe_unused]] int android_main() {

    // 画面の解像度、カラービットの設定
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    // 背景色の指定
    SetBackgroundColor(0, 0, 0);
    // DXライブラリの初期化
    DxLib_Init();
    // 描画先を裏画面に設定
    SetDrawScreen(DX_SCREEN_BACK);

    // ここで画像のロード、初期設定を行う
    int font = CreateFontToHandle(nullptr, 300, 50);
    int font_unit = CreateFontToHandle(nullptr, 100, 30);
    int image_map[PLACE_MAX];
    for (int i = 0; i < PLACE_MAX; i++) {
        image_map[i] = LoadGraph(IMAGE_MAP_PATH[i]);
    }
    int image_current = LoadGraph(IMAGE_CURRENT_PATH);
    int image_arrow = LoadGraph(IMAGE_ARROW_PATH);
    int audio_start = LoadSoundMem(AUDIO_START_PATH);
    int audio_stop = LoadSoundMem(AUDIO_STOP_PATH);
    int audio_warning1 = LoadSoundMem(AUDIO_WARNING1_PATH);
    int audio_warning2 = LoadSoundMem(AUDIO_WARNING2_PATH);

    int touch_time = 0; // 連続でタッチされている時間
    int bar_width = 50;

    // マニフェストに <uses-permission android:name="android.permission.INTERNET" /> の記載をお忘れなく
    std::thread microcontroller_http_thread = std::thread([]() {
        httplib::Client cli_microcontroller("http://192.168.43.4"); // 計測マイコンのIPアドレス
        while (true) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifndef TEST_CASE
                httplib::Result res_data = cli_microcontroller.Get("/GetMeasurementData");
                if (res_data) JsonString_Sensor = res_data->body;
#endif
            } catch (...) {
                // catch all exception
            }
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
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifndef NO_UPLOAD
#ifdef SHOW_WIND
                // 風速・風向をサーバーから取得
                httplib::Result res_data = cli_server.Get("/data/LD/?format=json");
                if (res_data) JsonString_Server = res_data->body;
#endif
                // HMAC認証符号を追加してサーバーにPOST
                std::string hmac_base64;
                hmac_sha256(KEY, sizeof(KEY), JsonString_Sensor.data(), JsonString_Sensor.size(),
                            HMAC,
                            sizeof(HMAC));
                algorithm::encode_base64(std::vector<uint8_t>(HMAC, HMAC + sizeof(HMAC)),
                                         hmac_base64);
                httplib::Headers headers = {{"Authorization", hmac_base64}};
                auto res = cli_server.Post("/data/create/", headers, JsonString_Sensor,
                                           "application/json");
#endif
            } catch (...) {
                // catch all exception
            }
        }
    });
    server_http_thread.detach();

    // 1秒間に60回繰り返される
    while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0) {

        try {
            // JsonをDeserialize
            get_json_data();

            // 地図の表示
            DrawExtendGraph(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, image_map[current_place], false);

            // 数値の表示
            int wide;
            wide = GetDrawFormatStringWidthToHandle(font, "%.1f", speed);
            DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 200,
                                     GetColor(255, 255, 255), font, "%.1f", speed);
            DrawStringToHandle(800, 350, "m/s", GetColor(255, 255, 255), font_unit);
            wide = GetDrawFormatStringWidthToHandle(font, "%.2f", altitude / 100.0);
            DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 700,
                                     GetColor(255, 255, 255), font, "%.2f", altitude / 100.0);
            DrawStringToHandle(850, 850, "m", GetColor(255, 255, 255), font_unit);
            wide = GetDrawFormatStringWidthToHandle(font, "%d", rpm);
            DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 1200,
                                     GetColor(255, 255, 255), font, "%d", rpm);
            DrawStringToHandle(800, 1350, "rpm", GetColor(255, 255, 255), font_unit);
            wide = GetDrawFormatStringWidthToHandle(font_unit, "%d", distance);
            DrawFormatStringToHandle(SCREEN_WIDTH / 2 - wide / 2, 1850,
                                     GetColor(255, 255, 0), font_unit, "%d", distance);
            DrawStringToHandle(800, 1850, "m", GetColor(255, 255, 255), font_unit);

            // ロールとピッチに応じて色を変える
            // （1.0度以下→緑、1.0~2.0度→黄色、2.0~3.0度→オレンジ、3.0度以上→赤）
            unsigned int color_top, color_bottom, color_left, color_right;
            if (pitch > 3.0) { color_top = COLOR_RED; }
            else if (pitch > 2.0) { color_top = COLOR_YELLOW_RED; }
            else if (pitch > 1.0) { color_top = COLOR_YELLOW; }
            else { color_top = COLOR_GREEN; }
            if (pitch < -3.0) { color_bottom = COLOR_RED; }
            else if (pitch < -2.0) { color_bottom = COLOR_YELLOW_RED; }
            else if (pitch < -1.0) { color_bottom = COLOR_YELLOW; }
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
            const double exRate = 0.2;
            GetGraphSize(image_current, &w, &h);
            DrawRotaGraph(x - (int)(w * exRate) / 2, y - (int)(h * exRate) / 2, exRate, deg2rad(gpsCourse), image_current, true);
            // DrawCircle(x, y, 5, COLOR_RED);

            // 軌跡の追加
            if (x > 0 && x < SCREEN_WIDTH && y > 0 && y < SCREEN_HEIGHT) {
                if (trajectory_x.empty() || trajectory_y.empty() || trajectory_x.back() != x ||
                    trajectory_y.back() != y) {
                    trajectory_x.push_back(x);
                    trajectory_y.push_back(y);
                }
            }

            // 飛行距離計算
            distance = (int)cal_distance(longitude, latitude, START_POINT[0], START_POINT[1]);

            // 軌跡の描画
            for (int i = 1; i < trajectory_x.size(); i++) {
                DrawLine(trajectory_x[i - 1], trajectory_y[i - 1], trajectory_x[i], trajectory_y[i],
                         COLOR_RED, 10);
            }

#ifdef SHOW_WIND
            // 風向・風速描画
            for (int i = 0; i < winds.size(); i++) {
                x = (int) ((std::stod(winds[i].Longitude) - C_LON[current_place]) *
                           X_SCALE[current_place]);
                y = (int) ((std::stod(winds[i].Latitude) - C_LAT[current_place]) *
                           Y_SCALE[current_place]);
                x += SCREEN_WIDTH / 2;
                y += SCREEN_HEIGHT / 2;
                GetGraphSize(image_current, &w, &h);
                DrawRotaGraph(x, y, std::stod(winds[i].WindSpeed) * 1.0,
                              deg2rad(std::stod(winds[i].WindDirection)),
                              image_arrow, true);
            }
#endif

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
                // 2本ならログの記録を開始（または終了）
                if (GetTouchInputNum() == 2) {
                    standard_roll = roll + standard_roll;
                    standard_pitch = pitch + standard_pitch;
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

            if (current_place == PLACE_BIWAKO) {
                const int POINT_SIZE = 20;
                // 旋回ポイントにプロット
                for (auto i : TURNING_POINT) {
                    int px = (int) ((i[0] - C_LON[current_place]) *
                                    X_SCALE[current_place]);
                    int py = (int) ((i[1] - C_LAT[current_place]) *
                                    Y_SCALE[current_place]);
                    px += SCREEN_WIDTH / 2;
                    py += SCREEN_HEIGHT / 2;

                    DrawCircle(px, py, POINT_SIZE, COLOR_YELLOW_RED);
                }

                int px = (int) ((START_POINT[0] - C_LON[current_place]) * X_SCALE[current_place]);
                int py = (int) ((START_POINT[1] - C_LAT[current_place]) * Y_SCALE[current_place]);
                px += SCREEN_WIDTH / 2;
                py += SCREEN_HEIGHT / 2;
                // 5, 10, 15, 18kmに扇形を描画
                const int DISTANCE_BORDER[4] = {5, 10, 15, 18};
                for (int i : DISTANCE_BORDER) {
                    SetDrawBlendMode(DX_BLENDGRAPHTYPE_ALPHA, 0x40);
                    DrawCircle(px, py, (int)(i * 53.8), COLOR_WHITE, false, 5);
                    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, (int)NULL);
                }
                // プラットホーム場所にプロット
                DrawBox(px - POINT_SIZE, py - POINT_SIZE, px + POINT_SIZE, py + POINT_SIZE, COLOR_YELLOW_RED, true);
            }

            // ログを収集していなければ左上に四角を描画
            if (!log_state) {
                DrawBox(0, 0, bar_width, bar_width, COLOR_WHITE, true);
            }

            // スタート・ストップ時に三角形または四角形を描画し、音を再生
            const static int START_STOP_ICON_WIDTH = 480;
            if (log_count > 0) {
                if (log_state) {
                    if (log_count == LOG_START_STOP_MARK_TIME) {
                        PlaySoundMem(audio_start, DX_PLAYTYPE_BACK);
                    }
                    DrawTriangleAA((float)SCREEN_WIDTH / 2 - (float)START_STOP_ICON_WIDTH / 2,
                                   (float)SCREEN_HEIGHT / 2 - (float)START_STOP_ICON_WIDTH / 2,
                                   (float)SCREEN_WIDTH / 2 - (float)START_STOP_ICON_WIDTH / 2,
                                   (float)SCREEN_HEIGHT / 2 + (float)START_STOP_ICON_WIDTH / 2,
                                   (float)SCREEN_WIDTH / 2 + (float)START_STOP_ICON_WIDTH / 2,
                                   (float)SCREEN_HEIGHT / 2, COLOR_GREEN, true);
                } else {
                    if (log_count == LOG_START_STOP_MARK_TIME) {
                        PlaySoundMem(audio_stop, DX_PLAYTYPE_BACK);
                    }
                    DrawBox(SCREEN_WIDTH / 2 - START_STOP_ICON_WIDTH / 2,
                            SCREEN_HEIGHT / 2 - START_STOP_ICON_WIDTH / 2,
                            SCREEN_WIDTH / 2 + START_STOP_ICON_WIDTH / 2,
                            SCREEN_HEIGHT / 2 + START_STOP_ICON_WIDTH / 2, COLOR_RED, true);
                }
                log_count--;
            }

            // Roll, Pitchが基準値以上の時の警報音
            if (abs(roll) > 10 || abs(pitch) > 10) {
                if (CheckSoundMem(audio_warning1) == 0)
                    PlaySoundMem(audio_warning1, DX_PLAYTYPE_LOOP);
            } else {
                StopSoundMem(audio_warning1);
            }

            // 対気速度が基準値以上の時の警報音
            if (speed > 7.5) {
                if (CheckSoundMem(audio_warning2) == 0)
                    PlaySoundMem(audio_warning2, DX_PLAYTYPE_LOOP);
            } else {
                StopSoundMem(audio_warning2);
            }

        } catch (...) {
            // catch all exception
        }
    }

    // DXライブラリ終了処理
    DxLib_End();
    return 0;
}
