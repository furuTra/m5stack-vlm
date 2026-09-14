#include "module_llm.h"

#include <ArduinoJson.h>
#include <M5Unified.h>

namespace vlmapp {

// YOLOモデルパッケージ名。リファレンス実装(reference/yolo_example.ino)およびライブラリ同梱の
// YOLO_CoreS3.ino は yolo.setup() を引数なしで呼び、ライブラリ既定の "yolo11n" を使っている。
// 実機で動作実績のあるこの既定に合わせる(2026-09-13: 以前指定していた "llm-yolo" では
// yolo.setup が work_id を返さずセットアップに失敗していたため、リファレンス準拠に戻した)。
const char* const kDefaultYoloModel = "yolo11n";
const uint32_t kYoloBaudRate        = 1500000;

void ModuleLlmClient::reopenSerial(uint32_t baud_rate) {
    if (serial_ == nullptr) {
        return;
    }
    // sketch_sep7a.ino準拠: M5.getPin()でPort CのRXD/TXDを解決する。
    // M5.begin()より後に呼ぶこと(呼ぶ前だとピンが解決できない)。
    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    serial_->begin(baud_rate, SERIAL_8N1, rxd, txd);
    module_.begin(serial_);
}

bool ModuleLlmClient::begin(HardwareSerial& serial, uint32_t baud_rate) {
    serial_ = &serial;
    reopenSerial(baud_rate);
    return true;
}

void ModuleLlmClient::waitForConnection() {
    while (true) {
        if (module_.checkConnection()) {
            break;
        }
        delay(10);
    }
}

void ModuleLlmClient::connectAutoBaud() {
    // モジュールの現在のボーレートが不明(電源を切らない限り前回のsetBaudRateが残る)なので、
    // 候補を交互に試す。checkConnection()は失敗時に約2秒待つため、1候補=約2秒/周。
    const uint32_t candidates[] = {115200, kYoloBaudRate};

    while (true) {
        for (uint32_t baud : candidates) {
            reopenSerial(baud);
            delay(50);
            if (module_.checkConnection()) {
                Serial.printf("[llm] connected at baud=%lu\n", (unsigned long)baud);

                // 以降の起動シーケンス(resetModule→setBaudRate(1.5M))は「今115200」を前提に
                // 組まれているため、既定の115200へ確実に戻してから返る。
                if (baud != 115200) {
                    Serial.println("[llm] module was at high baud; restoring 115200");
                    module_.setBaudRate(115200);  // 現ボーレートでコマンド送信→モジュールが115200へ
                    reopenSerial(115200);         // こちらも115200へ追従
                    delay(50);
                    module_.checkConnection();    // ベストエフォート確認(失敗しても続行)
                }
                return;
            }
        }
    }
}

void ModuleLlmClient::resetModule() {
    module_.sys.reset();
}

bool ModuleLlmClient::setBaudRate(uint32_t baud_rate) {
    // YOLO_CoreS3.ino と同じ手順: モジュール側へ新ボーレートを通知した後、
    // ホスト側のSerialも同じボーレートで開き直す。
    bool ok = module_.setBaudRate(baud_rate);
    reopenSerial(baud_rate);
    return ok;
}

bool ModuleLlmClient::setupYolo(const String& model) {
    m5_module_llm::ApiYoloSetupConfig_t config;
    config.model = model;

    // VLMのsetupと同様、sys.reset()直後は汎用のwork_id("yolo")が返ることがあるためリトライする。
    for (int attempt = 1; attempt <= 5; ++attempt) {
        yolo_work_id_ = module_.yolo.setup(config, "yolo_setup");
        Serial.printf("[llm] setupYolo attempt=%d work_id=[%s]\n", attempt, yolo_work_id_.c_str());

        if (yolo_work_id_.length() > 0 && yolo_work_id_ != "yolo") {
            return true;
        }
        delay(1000);
    }
    return isYoloReady();
}

bool ModuleLlmClient::detectObjects(const uint8_t* jpeg_data, size_t jpeg_len,
                                    const std::function<void(const YoloDetection&)>& onDetection,
                                    uint32_t timeout_ms) {
    if (!isYoloReady() || jpeg_data == nullptr || jpeg_len == 0) {
        return false;
    }

    size_t raw_len = jpeg_len;
    int ret        = module_.yolo.inferenceAndWaitResult(
        yolo_work_id_, const_cast<uint8_t*>(jpeg_data), raw_len,
        [&onDetection](String& result) {
            result.trim();
            if (result.length() == 0) {
                // finish時の空チャンク等。検出ではないのでスキップ。
                return;
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, result);
            if (err) {
                // 検出JSON以外(finishメッセージ本体など)が来ることがあるため、
                // パースできないものは黙ってスキップする。
                return;
            }

            JsonObject obj = doc.as<JsonObject>();
            if (!obj["bbox"].is<JsonArray>() || obj["class"].isNull()) {
                return;
            }

            // YOLO_CoreS3.ino準拠: class/confidence/bboxはいずれも文字列で返る。
            YoloDetection det;
            det.class_name = obj["class"].as<const char*>();
            det.confidence = atof(obj["confidence"].as<const char*>());
            JsonArray bbox = obj["bbox"].as<JsonArray>();
            if (bbox.size() == 4) {
                det.x1 = (int)atof(bbox[0].as<const char*>());
                det.y1 = (int)atof(bbox[1].as<const char*>());
                det.x2 = (int)atof(bbox[2].as<const char*>());
                det.y2 = (int)atof(bbox[3].as<const char*>());
            }

            if (onDetection) {
                onDetection(det);
            }
        },
        timeout_ms);

    return ret == MODULE_LLM_OK;
}

}  // namespace vlmapp
