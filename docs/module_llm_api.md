# src/llm/module_llm.h 提供API(module-llm-protocol担当、vlm-appから利用)

`vlmapp::ModuleLlmClient` が `M5ModuleLLM` ライブラリの薄いラッパー(YOLO物体検出用)。
`src/hal/` には依存しない(JPEGバイト列は呼び出し元が渡す)。実際の公開メソッド(2026-09-14時点、
実装と一致):

## 接続・起動
- `bool begin(HardwareSerial& serial, uint32_t baud_rate = 115200)` — UART初期化込み(pinは`M5.getPin(port_c_rxd/txd)`を自動解決)。`M5.begin()`より後に呼ぶこと
- `void waitForConnection()` — 接続できるまでブロック(タイムアウトなし)。リファレンスの`while(1) if(checkConnection()) break;`相当
- `void connectAutoBaud()` — 115200 ↔ `kYoloBaudRate` を交互にpingして接続し、**接続後は必ず115200へ戻す**。CoreS3だけ再起動してモジュールが前回の1.5Mbpsのまま残っていても復帰できる([known_issues.md](known_issues.md)参照)。`main.cpp`は`waitForConnection()`の代わりにこちらを使う
- `void resetModule()` — `sys.reset()`
- `bool setBaudRate(uint32_t baud_rate)` — モジュール/ホスト双方のUARTボーレートを変更(リファレンス準拠)。`begin()`後・YOLOセットアップ前に`kYoloBaudRate`(1.5Mbps)で呼ぶ

## YOLO物体検出
- `bool setupYolo(const String& model = kDefaultYoloModel)` — `yolo.setup`。既定モデルは`kDefaultYoloModel`(`"yolo11n"`、ライブラリ既定・リファレンス準拠)。`sys.reset()`直後は汎用work_id`"yolo"`が返ることがあるため内部でリトライする
- `bool isYoloReady() const`, `const String& yoloWorkId() const`
- `bool detectObjects(const uint8_t* jpeg_data, size_t jpeg_len, const std::function<void(const YoloDetection&)>& onDetection, uint32_t timeout_ms = 500)` — JPEG1枚をYOLO推論し、検出ごとに`onDetection`を呼ぶ(同期・ブロッキング)。finish時の空チャンク・不正JSONはスキップする。`main.cpp`はリファレンスに合わせて`timeout_ms`に短い値(既定10ms)を渡す
- `struct YoloDetection { String class_name; float confidence; int x1,y1,x2,y2; }` — bboxはカメラフレームのピクセル空間、`[x1,y1,x2,y2]`の対角コーナー座標として解釈([open_questions.md](open_questions.md)に実機検証項目あり)

## 定数
- `extern const char* const kDefaultYoloModel;` — `"yolo11n"`
- `extern const uint32_t kYoloBaudRate;` — `1500000`

> VLM(`vlm.setup`/`inference`)系のラッパーは2026-09-14に撤去した(現行構成では未使用)。VLMを再度使う
> 場合は git 履歴、または `reference/vlm_touch_example.cpp` を参照すること。

## 関連ドキュメント
- StackFlowプロトコル全体像: [protocol.md](protocol.md)
- このAPI実装に関わる既知の不具合(修正済み): [known_issues.md](known_issues.md)
- コード構成: [architecture.md](architecture.md)
