/*
 * main.cpp
 *
 * アプリケーション層(vlm-app担当)。
 *
 * 2026-09-14: リファレンス実装(reference/yolo_example.ino、およびライブラリ同梱の
 *   YOLO_CoreS3.ino)をベースに、YOLO物体検知のみを行う構成へ全面書き直し。
 *   - 以前の VLM / WiFi / 外部AIサーバー送信 / 対象クラス絞り込み(SUBJECT出力)は撤去。
 *   - 毎フレーム、カメラ画像をYOLOで推論し、検出した被写体すべてをバウンディングボックス
 *     +「クラス名 信頼度」ラベルでライブ映像に重ねて表示する(リファレンスと同じ挙動)。
 *   - 検出結果はUSBシリアル(Serial, 115200)にもログ出力する(デバッグ・確認用)。
 *   起動シーケンス(接続→reset→ボーレート1.5M→yolo.setup)はリファレンスに忠実に合わせ、
 *   YOLOモデルはライブラリ既定 "yolo11n"(kDefaultYoloModel)を使う。
 */
#include <Arduino.h>

#include <vector>

#include "hal/cores3_hal.h"
// YOLO検出処理は lib/yolo_object_detector パッケージへ切り出した(外部プロジェクトへ移植可能)。
#include <yolo_object_detector.h>

namespace {

yolo_object_detector::Detector g_detector;

// これ未満の確信度の検出は枠表示から除外する(誤検出抑制、要実機調整)。
const float kMinConfidence = 0.30f;

// --- [一時的に無効化] 表示用の色定数。LCD表示ブロックのコメントアウトに伴い未使用となるため無効化。戻す時はこの定義とloop()内の表示ブロックを有効化する ---
// 検出枠・ラベルの色(RGB565)。リファレンスは ORANGE 単色で描いている。
// const uint16_t kBoxColor = 0xFD20;  // オレンジ

// YOLO推論の応答タイムアウト。リファレンス(yolo_example.ino)のloop()と同じく短く取り、
// ライブフレームレートを優先する。応答が途切れてからこの時間で打ち切る。
const uint32_t kInferenceTimeoutMs = 10;

// YOLOへUART送信するJPEGの圧縮品質(0-100)。値を下げるほどJPEGが小さくなりUART転送が
// 速い(カクつき対策)。実機で画質と速度のバランスを見て調整する。
constexpr uint8_t kJpegQuality = 30;

}  // namespace

void setup() {
    cores3_hal::begin();

    cores3_hal::printLine(">> Check ModuleLLM connection..\n");
    g_detector.begin(Serial2);
    // ボーレート自動検出で接続する。CoreS3だけ再起動してモジュールが前回の1.5Mbpsのまま
    // 残っていても、115200と交互に試して復帰する(接続後は必ず115200へ戻る)。
    g_detector.connectAutoBaud();

    cores3_hal::printLine(">> Reset ModuleLLM..\n");
    g_detector.resetModule();

    // リファレンス準拠: フレーム毎のJPEG送信に耐えるようボーレートを上げる。
    cores3_hal::printLine(">> Set baud rate..\n");
    g_detector.setBaudRate(yolo_object_detector::kYoloBaudRate);

    cores3_hal::printLine(">> Setup yolo..\n");
    if (!g_detector.setupYolo(yolo_object_detector::kDefaultYoloModel)) {
        cores3_hal::printError(">> YOLO setup failed\n");
    }
}

void loop() {
    cores3_hal::update();

    if (!cores3_hal::cameraFrameAvailable()) {
        return;
    }

    uint8_t* jpg   = nullptr;
    size_t jpg_len = 0;
    cores3_hal::cameraFrameToJpeg(&jpg, &jpg_len, kJpegQuality);

    // このフレームの検出結果を集める。
    std::vector<yolo_object_detector::DetectedObject> dets;
    g_detector.detect(
        jpg, jpg_len,
        [&dets](const yolo_object_detector::DetectedObject& d) {
            if (d.confidence >= kMinConfidence) {
                dets.push_back(d);
            }
        },
        kInferenceTimeoutMs);

    // 検出結果をUSBシリアルへログ出力する(デバッグ・確認用)。0件のフレームは
    // 毎回出すとうるさいので出力しない。表示・検出ロジックには一切影響しない。
    if (!dets.empty()) {
        Serial.printf("[yolo] %u detections\n", static_cast<unsigned>(dets.size()));
        for (const auto& d : dets) {
            Serial.printf("[yolo] %s conf=%.2f bbox=(%d,%d)-(%d,%d)\n",
                          d.class_name.c_str(), d.confidence,
                          d.x1, d.y1, d.x2, d.y2);
        }
    }

    // --- [一時的に無効化] LCDへの映像+検出枠表示。yolo_object_detectorパッケージの境界確認のためコメントアウト。戻す時はこのブロックを有効化する ---
    // 表示専用の処理(ラベル文字列構築・オーバーレイ枠構築・画面描画)を一括で無効化している。
    // 検出結果は上のシリアルログ([yolo] ...)で確認できる。
    //
    // // ラベル文字列を先に確定させてから、それを指すオーバーレイ枠を作る。
    // std::vector<String> labels;
    // labels.reserve(dets.size());
    // for (const auto& d : dets) {
    //     labels.push_back(d.class_name + " " + String(d.confidence, 2));
    // }
    //
    // std::vector<cores3_hal::OverlayBox> boxes;
    // boxes.reserve(dets.size());
    // for (size_t i = 0; i < dets.size(); ++i) {
    //     boxes.push_back({dets[i].x1, dets[i].y1, dets[i].x2, dets[i].y2, labels[i].c_str(), kBoxColor});
    // }
    //
    // cores3_hal::showCameraFrameWithOverlay(boxes.data(), boxes.size());
    // --- [一時的に無効化] ここまで ---

    cores3_hal::releaseJpeg(jpg);
    cores3_hal::cameraFrameRelease();
}
