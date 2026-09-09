# M5Stack VLM Project

## 概要
M5Stack CoreS3 + M5Stack Module LLM(旧称 LLM Module、LLM630 Compute Kit相当)を使い、
VLM(Vision Language Model)でカメラ画像の説明・対話を行うファームウェアをPlatformIO/Arduinoで開発する。

## ハードウェア構成
- メインボード: M5Stack CoreS3(内蔵カメラ GC0308、LCD、タッチ)
- 拡張モジュール: M5Stack Module LLM(SoC: Axera AX630C、3.2TOPS INT8 NPU、LPDDR4 4GB、eMMC 32GB)
- 接続: M5-Bus経由のUARTが基本(デフォルト115200bps 8N1。TX/RX候補ピンはG18/G7/G14/G10で、実機配線により確定 — **要実機検証**)。I2C/USB OTGにも対応。
- 電源: アイドル5V@0.5W、最大5V@1.5W

## 通信プロトコル: StackFlow
Module LLMとはUART(またはTCP)上でNDJSON(改行区切りJSON)をやり取りする「StackFlow」プロトコルで通信するが、
**Arduinoからは生JSONを組む必要はなく、公式`M5ModuleLLM`ライブラリ(クラス名は`M5ModuleLLM`、`m5stack/M5Module-LLM`パッケージ)が
C++ APIとしてラップしている。** 実機確認済みの使用例は `reference/vlm_touch_example.cpp` を参照(ユーザー提供の動作実績コード)。

実績のあるAPI呼び出し順序:
```cpp
M5ModuleLLM module_llm;
Serial2.begin(115200, SERIAL_8N1, rxd, txd);   // pinはM5.getPin()で取得(下記参照)
module_llm.begin(&Serial2);
module_llm.checkConnection();                   // 接続確認(true になるまで待つ)
module_llm.sys.reset();                         // モジュールリセット

m5_module_llm::ApiVlmSetupConfig_t vlm_config;
vlm_config.model  = "llm-model-internvl2.5-1b-364-ax630c";
vlm_config.prompt = "Answer in Japanese only.";
String vlm_work_id = module_llm.vlm.setup(vlm_config, "vlm_setup"); // 失敗時は空文字列

// 画像(JPEG)を送ってから推論
module_llm.vlm.inference(vlm_work_id, out_jpg, out_jpg_len);
module_llm.vlm.inferenceAndWaitResult(vlm_work_id, question.c_str(), [](String& result) {
    // 結果をLCDに表示
});
```

内部的にはStackFlowのNDJSON(`vlm.setup`/`vlm.inference`等)がやり取りされているが、実装上は上記C++ APIを使えば十分。
生プロトコルの詳細(公式ドキュメント記載の例)は参考として残す:
```json
{"request_id":"vlm_001","work_id":"vlm","action":"setup","object":"vlm.setup",
 "data":{"model":"internvl2.5-1B-ax630c","response_format":"vlm.utf-8.stream",
 "input":["vlm.utf-8","camera.xxx"],"max_token_len":1023}}
```
画像はbase64 JPEG/PNG、生YUV(カメラ直結時)、生RGB/BGRのいずれかで渡せる。応答はストリーミングで返る。

## 参考リンク
- https://docs.m5stack.com/en/module/Module-LLM (ハード仕様)
- https://docs.m5stack.com/en/stackflow/module_llm/arduino_api (Arduino API)
- https://docs.m5stack.com/en/stackflow/applications/vlm/chat (VLM API詳細)
- https://github.com/m5stack/M5Module-LLM (Arduinoライブラリ本体、examples/VLM・YOLO_CoreS3あり)
- https://github.com/m5stack/StackFlow (プロトコル/サーバ実装)
- https://github.com/nnn112358/awesome_M5Stack_ModuleLLM (日本語まとめ・関連記事集)

## 依存ライブラリ(platformio.ini へ追加予定)
- `m5stack/M5Unified`, `m5stack/M5CoreS3`(カメラ/LCD)
- `m5stack/M5Module-LLM`(StackFlow通信、Arduinoラッパー)
- `bblanchon/ArduinoJson`(M5Module-LLMの依存)

## 実機確認済みリファレンス
`reference/vlm_touch_example.cpp` — ユーザーが以前作成し実機で動作させたVLMサンプル(タッチ操作でカメラ画像をModule LLMへ送り、
LCDに説明文を表示)。カメラ初期化・UARTピン取得・M5ModuleLLM API呼び出し順序の正解はこのファイルを一次情報とする。
迷ったら推測せずこのファイルを読むこと。

## src/llm/module_llm.h 提供API(module-llm-protocol担当、vlm-appから利用)
`vlmapp::ModuleLlmClient` が `M5ModuleLLM` ライブラリの薄いラッパー。`src/hal/` には依存しない
(JPEGバイト列は呼び出し元が渡す)。主なメソッド:
- `bool begin(HardwareSerial& serial, const ModuleLlmConfig& = {})` — UART初期化込み(pinは`M5.getPin(port_c_rxd/txd)`を自動解決、取得失敗時はGPIO18/17にフォールバック)
- `bool begin(Stream* stream, const ModuleLlmConfig& = {})` — 呼び出し元がUART初期化済みの場合
- `bool checkConnection()` — `connection_wait_timeout_ms`まで`sys.ping()`をリトライ
- `bool resetModule(bool wait_reset_finish = true)` — `sys.reset()`
- `bool setupVlm(const VlmSetupConfig& = {})` — 既定モデルは`llm-model-internvl2.5-1b-364-ax630c`。成功で`isReady()`がtrueになる
- `bool sendImageJpeg(const uint8_t* jpeg_data, size_t jpeg_len)` — JPEG生バイト列送信(ライブラリ内部でbase64化)
- `bool inferPrompt(const String& prompt, const InferenceChunkCallback& onChunk)` — `onChunk(text, finish)`をストリーミングで呼ぶ(`inference_idle_timeout_ms`アイドルタイムアウト)
- `bool describeImage(const uint8_t* jpeg_data, size_t jpeg_len, const String& prompt, const InferenceChunkCallback& onChunk)` — 上記2つのショートカット
- `bool shutdownVlm()`, `void update()`, `M5ModuleLLM& raw()`(高度な用途向けエスケープハッチ)

## 既知の不具合(修正済み)
- **`CoreS3.BtnPWR`を直接使わない**: `M5CoreS3.h`の`Button_Class &BtnPWR = M5.BtnPWR;`は`CoreS3`と`M5`という
  2つのグローバルオブジェクト間の静的初期化順序(翻訳単位をまたぐため未規定)に依存しており、実機では参照が
  不正になり`wasClicked()`呼び出し時に`Guru Meditation Error (LoadProhibited)`でクラッシュ→リブートを
  無限に繰り返す不具合を確認した(2026-09-09、`src/hal/cores3_hal.cpp`の`wasTriggerPressed()`)。
  **`CoreS3.BtnPWR`ではなく`M5.BtnPWR`(M5Unifiedの実体オブジェクト)を直接使うこと。** 修正済み・実機検証済み。
  他の`CoreS3.XxxYyy`形式の参照メンバ(内部で`M5.`委譲しているもの)を新たに使う場合も同様の懸念があるため注意。
- **`M5.config()`の`serial_baudrate`既定値は`0`**: `M5.begin()`/`CoreS3.begin()`はこの値が0だと`Serial.begin()`を
  呼ばない。デバッグ用の`Serial.printf`が実機で一切出力されない不具合として発現した(ROM/パニックログは別経路の
  ハードウェアコンソールなので表示されており紛らわしい)。`cores3_hal::begin()`冒頭で明示的に`Serial.begin(115200)`
  するよう修正済み(2026-09-09)。
- **画像送信直後にテキスト推論を送るとモジュール側で`"json format error"`/`"reace reset"`になる**:
  `ApiVlm::inference(work_id, jpegBytes, len, ...)`(画像push、fire-and-forget)の直後に
  `ApiVlm::inference(work_id, prompt, ...)`(テキストpush)を間を置かず送ると、モジュール側のパーサーが
  画像バイト列の受信処理を終える前に次のコマンドを受け取ってしまい同期を崩す。`reference/vlm_touch_example.cpp`
  が画像push後に`delay(10)`を入れていたのはこのため。`src/llm/module_llm.cpp`の`describeImage()`で画像push後に
  `delay(30)`を入れて修正済み・実機検証済み(2026-09-09)。
- **`ModuleMsg::responseMsgList`に断片メッセージが残留する**: `takeMsg(request_id, ...)`は一致した1件のみを
  取り出すため、`finish:true`受信でポーリングループを抜けた時点でまだ処理していない同じrequest_id宛の
  断片(特にJSONがUART受信バッファ境界で分割されパース失敗したもの)が`responseMsgList`に残り、次回以降の
  撮影サイクルでも蓄積し続ける不具合を確認した。`inferPrompt()`の終了時(成功/タイムアウト問わず)に
  `module_.msg.clearMsg(request_id)`で明示的に掃除するよう修正済み(2026-09-09)。

## 日本語表示(2026-09-09時点)
VLM(internvl2.5-1B、英語で回答)の出力を、qwenモデルによる2段目のLLMセッションで日本語に翻訳してから
画面表示する構成にした。同時に2セッション(vlm work_id + llm work_id)をModule LLM上に保持する。
- 翻訳モデル既定: `llm-model-qwen2.5-1.5b-int4-ax630c`(メモリ節約のためint4量子化版を選択。
  他候補は`llm-model-qwen2.5-1.5b-ax630c`, `llm-model-qwen2.5-1.5b-p256-ax630c`)
- 日本語グリフの表示にはM5GFX内蔵の`fonts::lgfxJapanGothic_20`フォントを使う
  (既定のsetTextSize()ベースの数値フォントは日本語グリフを含まず、何も表示されない不具合を確認した)。
  `cores3_hal::useJapaneseFont()`/`useDefaultFont()`で切り替える。
- `ModuleLlmClient::setupVlm()`と同じく、`setupTranslator()`も`sys.reset()`直後は汎用work_id("llm")
  が返ることがあるためリトライする。

## 未確定事項 / 実機で要検証
- VLMとqwen(翻訳用)の2モデルを同時にModule LLM上にロードした場合のメモリ余裕(未検証。もし
  不安定なら、翻訳直前にvlmを`vlm.exit`してからllmをsetupする「都度ロード」方式に切り替える必要がある)
- UART TX/RXピン: `M5.getPin(m5::pin_name_t::port_c_rxd)` / `port_c_txd` で取得する(解決済み。固定の数値ではなくAPIで取得すること)
- 大きいJPEGフレーム送信時のUARTスループット/タイムアウト値(リファレンスコードでは`delay(10)`のみ、詳細は未検証)
- Module LLM側へのモデル(`llm-model-internvl2.5-1b-364-ax630c`)初回ダウンロード/書き込み手順
- `module_llm.vlm.inference()`(非同期送信)と`inferenceAndWaitResult()`(同期待ち)の使い分け・タイムアウト仕様の詳細

各エージェントは、実装中に上記が確定したらこのファイルを更新すること。

## コード構成とエージェントの担当範囲
| ディレクトリ/ファイル | 担当エージェント | 内容 |
|---|---|---|
| `src/hal/` | `cores3-hal` | カメラ・LCD・ボタン等CoreS3周辺機器の抽象化 |
| `src/llm/` | `module-llm-protocol` | Module LLMとのUART/StackFlow通信ラッパー |
| `src/main.cpp`, `src/app/` | `vlm-app` | 全体のステートマシン・UIフロー |
| (プロジェクト全体) | `pio-build-check` | `pio run` によるビルド検証専任(実装はしない) |

新しいエージェントを起動する際は上記の担当範囲を尊重し、他エージェントの担当ファイルを編集する必要がある場合は理由を明記すること。

## src/main.cpp 実装済み(vlm-app担当)
`cores3_hal`と`vlmapp::ModuleLlmClient`を組み合わせたステートマシンを実装済み。両モジュールの中身には手を入れず、公開APIのみ呼び出している。

状態: `kBootInit`(起動処理、setup()内で同期実行) → `kBootError`(初期化失敗、タップで`kBootInit`へリトライ) / `kPreview`(ライブプレビュー、トリガーで撮影へ) → `kCapture`(JPEG撮影+`describeImage()`ブロッキング呼び出し、ストリーミングチャンクを`showResultText()`に逐次追記) → `kResult`(結果 or エラー文言を表示したまま次のトリガー待ち、タップで`kPreview`へ)。

起動シーケンス(`runBootSequence()`)の順序: `cores3_hal::begin()` → (カメラ初期化失敗時は警告表示のみで続行) → `g_llm_client.begin(Serial2)` → `checkConnection()` → `resetModule()` → `setupVlm()`。各段階の結果を`showStatus()`で逐次表示し、途中失敗時はエラーメッセージを保持して`kBootError`へ。`ModuleLlmClient::begin()`は`cores3_hal::begin()`(内部で`M5.begin()`を呼ぶ)より後に呼ぶ順序を厳守している。

`pio run -e m5stack-cores3` でビルド成功済み(2026-09-09時点)。HAL/LLM層のAPIに追加・変更は行っていない。
