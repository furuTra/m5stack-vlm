/*
 * main.cpp
 *
 * アプリケーション層(vlm-app担当)。
 *
 * 2026-09-09: ユーザーの実機動作実績コード
 * (c:\Users\takes\OneDrive\Documents\Arduino\sketch_sep7a\sketch_sep7a.ino =
 *  reference/vlm_touch_example.cpp) のsetup()/loop()のロジックに忠実に合わせて全面書き直し。
 * 独自のステートマシン(BOOT_ERRORリトライ、ストリーミング蓄積表示等)は一旦廃止し、
 * 「動作実績のある挙動をそのまま再現する」ことを最優先にしている。
 *
 * 操作方法(sketch_sep7a.ino準拠): 画面をダブルタップ(800ms以内に2回クリック)すると撮影→
 * VLM推論を実行する。それ以外はライブカメラプレビューを表示し続ける。
 */
#include <Arduino.h>

#include "hal/cores3_hal.h"
#include "llm/module_llm.h"

namespace {

vlmapp::ModuleLlmClient g_llm_client;

const char* const kQuestion = "Describe the content of the image";

// 0=プレビュー中, 1=撮影直後の1フレーム待ち, 2=撮影・推論を実行する
int g_vlm_inference = 0;

}  // namespace

void setup() {
    cores3_hal::begin();

    cores3_hal::printLine(">> Check ModuleLLM connection..\n");
    g_llm_client.begin(Serial2);
    g_llm_client.waitForConnection();

    cores3_hal::printLine(">> Reset ModuleLLM..\n");
    g_llm_client.resetModule();

    cores3_hal::printLine(">> Setup vlm..\n");
    bool ok = g_llm_client.setupVlm(vlmapp::kDefaultModel, vlmapp::kDefaultSetupPrompt);
    if (!ok) {
        cores3_hal::printError(">> VLM setup failed\n");
    }
    // 翻訳用qwenは常駐させない: 実機で、VLMとqwenを同時にロードすると
    // qwen側のsetupが常に失敗する(メモリ/リソース不足と思われる)ことを確認したため、
    // 翻訳が必要になったタイミングでVLMをexitしてから都度setupする(2026-09-09)。
}

void loop() {
    cores3_hal::update();

    if (cores3_hal::touchWasClicked()) {
        static unsigned long lastClickTime = 0;
        unsigned long currentMillis        = millis();
        if (currentMillis - lastClickTime < 800) {
            g_vlm_inference = 2;
        }
        lastClickTime = currentMillis;
    }

    if (cores3_hal::touchWasFlicked()) {
        g_vlm_inference--;
    }

    if (cores3_hal::cameraFrameAvailable()) {
        if (g_vlm_inference == 2) {
            uint8_t* jpg   = nullptr;
            size_t jpg_len = 0;
            cores3_hal::cameraFrameToJpeg(&jpg, &jpg_len);
            g_llm_client.pushImage(jpg, jpg_len);
            cores3_hal::releaseJpeg(jpg);

            // sketch_sep7a.ino(元コード)は delay(10) だが、実機検証で画像サイズによっては
            // 不足し、モジュール側が次のコマンドを"json format error"として処理してしまう
            // ケースを確認したため、余裕を持たせて delay(50) にしている(2026-09-09)。
            delay(50);
            cores3_hal::setResultCursorTopLeft();
            cores3_hal::printLine("Analyzing...\n");

            // VLMの出力(英語)はいったん貯めてから翻訳にかける
            // (逐次画面に出さない。翻訳後の日本語だけを表示する)。
            String englishDescription;
            g_llm_client.inferenceAndWaitResult(
                kQuestion, [&englishDescription](const String& result) { englishDescription += result; });

            // VLMとqwen(翻訳)は同時にロードできないため、VLMを一旦exitしてから
            // qwenをsetupする。
            g_llm_client.exitVlm();

            cores3_hal::clearDisplay();
            cores3_hal::setResultCursorTopLeft();
            cores3_hal::printLine("Translating...\n");

            bool translatorOk = g_llm_client.setupTranslator();
            cores3_hal::clearDisplay();
            cores3_hal::setResultCursorTopLeft();

            if (translatorOk) {
                cores3_hal::useJapaneseFont();
                bool translated = g_llm_client.translateToJapanese(
                    englishDescription, [](const String& chunk) { cores3_hal::printResultChunk(chunk); });
                cores3_hal::useDefaultFont();
                if (!translated) {
                    cores3_hal::printError("\n[translation timed out]\n");
                }
                g_llm_client.exitTranslator();
            } else {
                // 翻訳セットアップに失敗した場合は英語のまま表示する。
                cores3_hal::printError("[translator setup failed]\n");
                cores3_hal::printResultChunk(englishDescription);
            }

            // 次の撮影に備えてVLMを再度セットアップしておく。
            cores3_hal::printLine("\n\n(preparing next shot...)\n");
            g_llm_client.setupVlm(vlmapp::kDefaultModel, vlmapp::kDefaultSetupPrompt);

            g_vlm_inference--;
        } else if (g_vlm_inference == 1) {
            delay(10);
        } else {
            cores3_hal::showCameraFramePreview();
        }
        cores3_hal::cameraFrameRelease();
    }
}
