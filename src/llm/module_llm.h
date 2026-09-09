/**
 * @file module_llm.h
 * @brief M5Stack Module LLM (StackFlow protocol) UART wrapper.
 *
 * 2026-09-09: ユーザーの実機動作実績コード
 * (c:\Users\takes\OneDrive\Documents\Arduino\sketch_sep7a\sketch_sep7a.ino =
 *  reference/vlm_touch_example.cpp) の呼び出し順序・APIに忠実に合わせて全面書き直し。
 *
 * 以前の実装は`ModuleMsg::takeMsg`を自前でポーリングしてストリーミング応答を組み立てて
 * いたが、実機で「画像送信直後にテキスト推論を送るとjson format errorになる」
 * 「応答メッセージの断片がresponseMsgListに残留し続ける」といった不具合が見つかった。
 * これらは全て、ライブラリが元々提供している `ApiVlm::inferenceAndWaitResult()` を
 * そのまま使えば発生しない(sketch_sep7a.inoが実機で動作した実績があるため)。
 * よって本ラッパーはStackFlowの詳細に立ち入らず、`M5ModuleLLM`ライブラリの
 * 呼び出しを薄くラップするだけに留める。
 */
#pragma once

#include <Arduino.h>
#include <M5ModuleLLM.h>
#include <functional>

namespace vlmapp {

// sketch_sep7a.ino で実機確認済みの既定値。
// kDefaultSetupPrompt はひとまず英語回答のため空文字にしている
// (日本語指定は文字コード/フォント周りの切り分けが済んでから再検討する)。
extern const char* const kDefaultModel;        // "llm-model-internvl2.5-1b-364-ax630c"
extern const char* const kDefaultSetupPrompt;  // ""

// VLMの英語出力を日本語に訳し直すための翻訳用LLM(qwen)。
// int4量子化版はVLM(internvl2.5-1B)と同時にロードしてもメモリに収まりやすいことを
// 期待してこちらを既定にしている。他候補: "llm-model-qwen2.5-1.5b-ax630c",
// "llm-model-qwen2.5-1.5b-p256-ax630c"。
extern const char* const kDefaultTranslatorModel;  // "llm-model-qwen2.5-1.5b-int4-ax630c"

/**
 * @brief M5Stack Module LLM (StackFlow) との通信をラップするクライアント。
 * `src/hal/` には依存しない(JPEGバイト列は呼び出し元が渡す)。
 *
 * 呼び出し順序(sketch_sep7a.inoのsetup()と同じ):
 *   ModuleLlmClient client;
 *   client.begin(Serial2);
 *   client.waitForConnection();      // 接続できるまでブロック
 *   client.resetModule();
 *   client.setupVlm(vlmapp::kDefaultModel, vlmapp::kDefaultSetupPrompt);
 *
 * 呼び出し順序(loop()と同じ、撮影→推論):
 *   client.pushImage(jpeg, jpeg_len);
 *   delay(10);
 *   client.inferenceAndWaitResult("Describe the content of the image",
 *                                  [](const String& chunk) { ... });
 */
class ModuleLlmClient {
public:
    ModuleLlmClient()  = default;
    ~ModuleLlmClient() = default;

    /**
     * @brief UART(Serial2)を初期化し、Module LLMへ接続する。
     * ピンは M5.getPin(port_c_rxd/txd) で自動解決する(sketch_sep7a.ino準拠)。
     * M5.begin()より後に呼ぶこと。
     */
    bool begin(HardwareSerial& serial, uint32_t baud_rate = 115200);

    /**
     * @brief 接続できるまでブロックする(sketch_sep7a.inoの `while(1) if(checkConnection())
     * break;` と同じ、タイムアウトなし)。呼び出し前にステータス表示するのは呼び出し側の役目。
     */
    void waitForConnection();

    /// Module LLMをリセットする(戻り値は確認しない。sketch_sep7a.ino準拠)。
    void resetModule();

    /**
     * @brief `vlm.setup` を呼び、work_idを保持する。
     * @return 成功時true(work_idが空でない)。
     */
    bool setupVlm(const String& model, const String& prompt);

    /**
     * @brief VLMセッションを終了しリソースを解放する。実機で、VLMとqwen(翻訳用)を
     * 同時にロードしようとすると翻訳側のsetupが常に失敗する(work_idが汎用の"llm"の
     * まま)ことを確認したため、翻訳する間はVLMを一旦exitしてメモリを空ける
     * 運用にしている(2026-09-09)。
     */
    void exitVlm();

    /// setupVlm()が成功しているか(汎用のフォールバック値"vlm"は不成功として扱う)。
    bool isReady() const { return work_id_.length() > 0 && work_id_ != "vlm"; }

    const String& workId() const { return work_id_; }

    /**
     * @brief JPEG画像フレームをModule LLMへ送る(fire-and-forget、応答を待たない)。
     */
    void pushImage(const uint8_t* jpeg_data, size_t jpeg_len);

    /**
     * @brief テキストプロンプトを送り、応答が完了するまでブロックして待つ。
     * ライブラリの `ApiVlm::inferenceAndWaitResult()` をそのまま使うため、応答は
     * チャンク(delta)ごとに `onChunk` が呼ばれる(finish時にも1回呼ばれ、末尾に改行が付く)。
     *
     * @param timeout_ms 応答が途切れてからのタイムアウト(ライブラリ既定は5000ms)。
     * @return true=正常完了、false=タイムアウト。
     */
    bool inferenceAndWaitResult(const String& prompt, const std::function<void(const String&)>& onChunk,
                                uint32_t timeout_ms = 15000);

    /**
     * @brief 翻訳用LLM(qwen)を`llm.setup`でセットアップする。VLMのsetupVlm()同様、
     * work_idが汎用の"llm"のまま返ってくる場合はリトライする。
     */
    bool setupTranslator(const String& model = kDefaultTranslatorModel, const String& systemPrompt = "");

    /// setupTranslator()が成功しているか(汎用のフォールバック値"llm"は不成功として扱う)。
    bool isTranslatorReady() const { return translator_work_id_.length() > 0 && translator_work_id_ != "llm"; }

    /**
     * @brief 翻訳用LLM(qwen)セッションを終了しリソースを解放する。
     */
    void exitTranslator();

    /**
     * @brief 与えた英文を日本語へ翻訳するようqwenへ依頼し、ストリーミングで結果を受け取る。
     * @return true=正常完了、false=タイムアウトまたは翻訳LLM未セットアップ。
     */
    bool translateToJapanese(const String& englishText, const std::function<void(const String&)>& onChunk,
                             uint32_t timeout_ms = 30000);

private:
    M5ModuleLLM module_;
    String work_id_;
    String translator_work_id_;
};

}  // namespace vlmapp
