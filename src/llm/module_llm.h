/**
 * @file module_llm.h
 * @brief M5Stack Module LLM (StackFlow protocol) UART wrapper — YOLO物体検出用。
 *
 * 2026-09-14: リファレンス実装(reference/yolo_example.ino、ライブラリ同梱 YOLO_CoreS3.ino)を
 *   ベースにYOLO物体検出専用へ整理。以前併存していたVLM(vlm.setup/inference)系のラッパーは
 *   現行の構成では使わないため撤去した(必要になれば git 履歴 / reference/vlm_touch_example.cpp を参照)。
 *
 * 本ラッパーはStackFlowの詳細に立ち入らず、`M5ModuleLLM`ライブラリの呼び出しを薄くラップする。
 * カメラ画像のJPEGをフレーム毎にYOLOへ送り、検出結果(class/confidence/bbox)を受け取る。
 */
#pragma once

#include <Arduino.h>
#include <M5ModuleLLM.h>
#include <functional>

namespace vlmapp {

// YOLO物体検出の既定モデル。リファレンス(yolo_example.ino)/ YOLO_CoreS3.ino が
// yolo.setup() を引数なしで呼ぶときの既定 "yolo11n" に合わせる。
extern const char* const kDefaultYoloModel;    // "yolo11n"

// YOLOの高スループット通信用ボーレート。リファレンスに合わせて 1.5Mbps。
// フレーム毎にJPEGをUART送信するため、既定の115200bpsでは表示レートが不足する。
extern const uint32_t kYoloBaudRate;           // 1500000

// YOLOが返す検出結果1件。座標はYOLOへ渡した画像(=カメラフレーム)のピクセル空間。
// bboxは [x1,y1,x2,y2] の対角コーナー座標として解釈する(リファレンスのフィールド名準拠。
// 実機で枠がずれる場合は [x,y,w,h] の可能性があるので docs/open_questions.md 参照)。
struct YoloDetection {
    String class_name;
    float confidence = 0.0f;
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
};

/**
 * @brief M5Stack Module LLM (StackFlow) との通信をラップするYOLOクライアント。
 * `src/hal/` には依存しない(JPEGバイト列は呼び出し元が渡す)。
 *
 * 呼び出し順序(reference/yolo_example.ino の setup() と同じ):
 *   ModuleLlmClient client;
 *   client.begin(Serial2);
 *   client.connectAutoBaud();          // 接続できるまでブロック(接続後は115200へ戻す)
 *   client.resetModule();
 *   client.setBaudRate(vlmapp::kYoloBaudRate);
 *   client.setupYolo();
 *
 * 呼び出し順序(loop()、撮影→推論):
 *   client.detectObjects(jpeg, jpeg_len, [](const YoloDetection& d) { ... });
 */
class ModuleLlmClient {
public:
    ModuleLlmClient()  = default;
    ~ModuleLlmClient() = default;

    /**
     * @brief UART(Serial2)を初期化し、Module LLMへ接続する。
     * ピンは M5.getPin(port_c_rxd/txd) で自動解決する(リファレンス準拠)。
     * M5.begin()より後に呼ぶこと。
     */
    bool begin(HardwareSerial& serial, uint32_t baud_rate = 115200);

    /**
     * @brief 接続できるまでブロックする(リファレンスの `while(1) if(checkConnection()) break;`
     * と同じ、タイムアウトなし)。呼び出し前にステータス表示するのは呼び出し側の役目。
     */
    void waitForConnection();

    /**
     * @brief ボーレートを自動検出して接続する(115200 ↔ kYoloBaudRate を交互に試す)。
     *
     * 背景: `setBaudRate()` で上げたボーレートはモジュール側の電源を切るまで保持されるため、
     * CoreS3だけ再起動すると「モジュール=1.5Mbps / CoreS3=115200」で永久に接続できなくなる
     * (`checkConnection()`は1回2秒待つため、止まって見える)。本メソッドは各候補ボーレートで
     * pingを試し、接続できたら**必ず115200へ戻して**から返る。以降は
     * `resetModule()` → `setBaudRate(kYoloBaudRate)` → `setupYolo()` の順序を前提とする。
     * 接続できるまでブロックする(タイムアウトなし)。
     */
    void connectAutoBaud();

    /// Module LLMをリセットする(戻り値は確認しない。リファレンス準拠)。
    void resetModule();

    /**
     * @brief モジュール側とホスト側(UART)双方のボーレートを変更する。
     * リファレンスと同じ手順: `sys` 経由でモジュールへ新ボーレートを通知し、
     * こちら側のSerialも同じボーレートで開き直す。begin() の後に呼ぶこと。
     * @return モジュール側の設定成功でtrue。
     */
    bool setBaudRate(uint32_t baud_rate);

    /**
     * @brief `yolo.setup` を呼び、YOLOのwork_idを保持する。
     * @return 成功時true(work_idが空でない)。
     */
    bool setupYolo(const String& model = kDefaultYoloModel);

    /// setupYolo()が成功しているか。
    bool isYoloReady() const { return yolo_work_id_.length() > 0 && yolo_work_id_ != "yolo"; }

    const String& yoloWorkId() const { return yolo_work_id_; }

    /**
     * @brief JPEG画像1枚をYOLOで推論し、検出ごとに onDetection を呼ぶ(同期・ブロッキング)。
     * ライブラリの `ApiYolo::inferenceAndWaitResult()` を使い、返ってくる検出JSON
     * ({"class","confidence","bbox":[x1,y1,x2,y2]})を YoloDetection にパースする。
     * finish時の空チャンクや不正JSONは onDetection を呼ばずにスキップする。
     *
     * @param timeout_ms 応答が途切れてからのタイムアウト。検出0件でモジュールがfinishを
     *        返さない場合の空振り時間になるため、ライブフレームレートに影響する(要実機調整)。
     * @return true=正常完了、false=未setup/タイムアウト。
     */
    bool detectObjects(const uint8_t* jpeg_data, size_t jpeg_len,
                       const std::function<void(const YoloDetection&)>& onDetection,
                       uint32_t timeout_ms = 500);

private:
    M5ModuleLLM module_;
    String yolo_work_id_;
    // setBaudRate()/connectAutoBaud() でSerialを開き直すために begin() で受け取ったSerialを保持する。
    HardwareSerial* serial_ = nullptr;

    // serial_ を指定ボーレートで開き直し、module_.begin() し直す(RXD/TXDは毎回M5.getPin()で解決)。
    void reopenSerial(uint32_t baud_rate);
};

}  // namespace vlmapp
