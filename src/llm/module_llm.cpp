#include "module_llm.h"

#include <ArduinoJson.h>
#include <M5Unified.h>

namespace vlmapp {

const char* const kDefaultModel            = "llm-model-internvl2.5-1b-364-ax630c";
const char* const kDefaultSetupPrompt      = "Answer in English.";
// 実機で確認したところ、qwen2.5-1.5b系は未インストールで、モジュールに実際に
// 入っていたのは "llm-qwen2.5-0.5b-prefill-20e" ([installed,local]) のみだった。
// これはApiLlmSetupConfig_tの既定モデル名"qwen2.5-0.5B-prefill-20e"とほぼ一致する
// (大文字小文字のみ違う)ため、こちらを使う(2026-09-09)。
const char* const kDefaultTranslatorModel  = "qwen2.5-0.5B-prefill-20e";

bool ModuleLlmClient::begin(HardwareSerial& serial, uint32_t baud_rate) {
    // sketch_sep7a.ino準拠: M5.getPin()でPort CのRXD/TXDを解決する。
    // M5.begin()より後に呼ぶこと(呼ぶ前だとピンが解決できない)。
    int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
    int txd = M5.getPin(m5::pin_name_t::port_c_txd);
    serial.begin(baud_rate, SERIAL_8N1, rxd, txd);
    return module_.begin(&serial);
}

void ModuleLlmClient::waitForConnection() {
    while (true) {
        if (module_.checkConnection()) {
            break;
        }
        delay(10);
    }
}

void ModuleLlmClient::resetModule() {
    module_.sys.reset();
}

bool ModuleLlmClient::setupVlm(const String& model, const String& prompt) {
    m5_module_llm::ApiVlmSetupConfig_t config;
    config.model  = model;
    config.prompt = prompt;

    // 実機で、sys.reset()直後にvlm.setup()すると、モジュールがまだリセット処理中で
    // 正式なセッションwork_id(例: "vlm.1000")ではなく汎用の"vlm"を返してしまう
    // (=セッションが作られていない)ことを確認した。"vlm"固定文字列が返った場合は
    // リセット未完了とみなし、間隔を空けてリトライする(2026-09-09)。
    for (int attempt = 1; attempt <= 5; ++attempt) {
        work_id_ = module_.vlm.setup(config, "vlm_setup");
        Serial.printf("[llm] setupVlm attempt=%d work_id=[%s]\n", attempt, work_id_.c_str());

        if (work_id_.length() > 0 && work_id_ != "vlm") {
            return true;
        }
        delay(1000);
    }
    return isReady();
}

void ModuleLlmClient::exitVlm() {
    if (work_id_.length() > 0) {
        module_.vlm.exit(work_id_);
    }
    work_id_ = "";
}

void ModuleLlmClient::pushImage(const uint8_t* jpeg_data, size_t jpeg_len) {
    if (!isReady() || jpeg_data == nullptr || jpeg_len == 0) {
        return;
    }
    size_t len = jpeg_len;
    // ApiVlm::inference()の生バイト列オーバーロードは読み取り専用だが引数がconstではない
    // (内部でModuleComm::sendRaw(const uint8_t*, size_t&)へ転送するのみ)。const_castで安全。
    module_.vlm.inference(work_id_, const_cast<uint8_t*>(jpeg_data), len);
}

bool ModuleLlmClient::inferenceAndWaitResult(const String& prompt, const std::function<void(const String&)>& onChunk,
                                             uint32_t timeout_ms) {
    if (!isReady()) {
        return false;
    }

    // ライブラリ内蔵のinferenceAndWaitResult()は既定のrequest_id("vlm_inference"固定)を
    // 使い回すため、実機で応答の取り違え(古いメッセージが即マッチしてしまう)らしき
    // 挙動("null"の中身をパースし続けてタイムアウトする)を確認した。撮影ごとに一意な
    // request_idを使い、生JSONをログに出す独自ポーリングに切り替える(2026-09-09)。
    String request_id("vlm_infer_");
    request_id += String(millis());

    module_.vlm.inference(work_id_, prompt, request_id);

    uint32_t last_activity = millis();
    bool finished           = false;
    bool timed_out          = false;

    while (!finished) {
        module_.update();

        module_.msg.takeMsg(request_id, [&](m5_module_llm::ResponseMsg_t& msg) {
            Serial.printf("[llm] raw: %s\n", msg.raw_msg.c_str());

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, msg.raw_msg);
            if (err) {
                Serial.printf("[llm] JSON parse error: %s\n", err.c_str());
                return;
            }

            String delta = doc["data"]["delta"].as<String>();
            bool finish  = false;
            if (!doc["data"]["finish"].isNull()) {
                finish = doc["data"]["finish"].as<bool>();
            }

            if (onChunk) {
                onChunk(delta);
            }
            if (finish) {
                finished = true;
            }
            last_activity = millis();
        });

        if (finished) {
            break;
        }
        if (millis() - last_activity > timeout_ms) {
            Serial.printf("[llm] inferenceAndWaitResult TIMEOUT after %lums\n", (unsigned long)timeout_ms);
            timed_out = true;
            break;
        }
    }

    // このrequest_id宛の未消費の断片(パース失敗した部分メッセージ等)が残り続けて
    // 次回以降に蓄積するのを防ぐため、必ず掃除する。
    module_.msg.clearMsg(request_id);

    return !timed_out;
}

bool ModuleLlmClient::setupTranslator(const String& model, const String& systemPrompt) {
    // ライブラリ内蔵のApiLlm::setup()は成功/失敗に関わらずwork_idしか返さず、
    // 失敗理由(エラーコード/メッセージ)が見えない。原因切り分けのため、
    // 一時的に生JSONコマンドを自前で組んで送り、生の応答をログに出す(2026-09-09)。
    String request_id = "llm_setup";

    JsonDocument doc;
    doc["request_id"]              = request_id;
    doc["work_id"]                 = "llm";
    doc["action"]                  = "setup";
    doc["object"]                  = "llm.setup";
    doc["data"]["model"]           = model;
    doc["data"]["response_format"] = "llm.utf-8.stream";
    doc["data"]["enoutput"]        = true;
    doc["data"]["enkws"]           = false;
    doc["data"]["max_token_len"]   = 127;
    doc["data"]["prompt"]          = systemPrompt;
    JsonArray inputArray = doc["data"]["input"].to<JsonArray>();
    inputArray.add("llm.utf-8.stream");

    String cmd;
    serializeJson(doc, cmd);
    Serial.printf("[llm] setupTranslator cmd: %s\n", cmd.c_str());

    module_.msg.sendCmd(cmd.c_str());

    translator_work_id_ = "";
    uint32_t start       = millis();
    while (millis() - start < 10000) {
        module_.update();
        module_.msg.takeMsg(request_id, [&](m5_module_llm::ResponseMsg_t& msg) {
            Serial.printf("[llm] setupTranslator raw: %s\n", msg.raw_msg.c_str());
            translator_work_id_ = msg.work_id;
        });
        if (translator_work_id_.length() > 0) {
            break;
        }
    }
    module_.msg.clearMsg(request_id);

    return isTranslatorReady();
}

void ModuleLlmClient::exitTranslator() {
    if (translator_work_id_.length() > 0) {
        module_.llm.exit(translator_work_id_);
    }
    translator_work_id_ = "";
}

bool ModuleLlmClient::translateToJapanese(const String& englishText, const std::function<void(const String&)>& onChunk,
                                          uint32_t timeout_ms) {
    if (!isTranslatorReady()) {
        return false;
    }

    String prompt =
        "Translate the following English text into natural Japanese. "
        "Output only the Japanese translation, nothing else.\n\n" +
        englishText;

    String request_id("llm_infer_");
    request_id += String(millis());

    module_.llm.inference(translator_work_id_, prompt, request_id);

    uint32_t last_activity = millis();
    bool finished           = false;
    bool timed_out          = false;

    while (!finished) {
        module_.update();

        module_.msg.takeMsg(request_id, [&](m5_module_llm::ResponseMsg_t& msg) {
            Serial.printf("[llm] translate raw: %s\n", msg.raw_msg.c_str());

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, msg.raw_msg);
            if (err) {
                Serial.printf("[llm] translate JSON parse error: %s\n", err.c_str());
                return;
            }

            String delta = doc["data"]["delta"].as<String>();
            bool finish  = false;
            if (!doc["data"]["finish"].isNull()) {
                finish = doc["data"]["finish"].as<bool>();
            }

            if (onChunk) {
                onChunk(delta);
            }
            if (finish) {
                finished = true;
            }
            last_activity = millis();
        });

        if (finished) {
            break;
        }
        if (millis() - last_activity > timeout_ms) {
            Serial.printf("[llm] translateToJapanese TIMEOUT after %lums\n", (unsigned long)timeout_ms);
            timed_out = true;
            break;
        }
    }

    module_.msg.clearMsg(request_id);

    return !timed_out;
}

}  // namespace vlmapp
