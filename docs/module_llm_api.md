# YOLO検出パッケージ 公開API(module-llm-protocol担当、vlm-appから利用)

> **2026-09-15:** このラッパーは `src/llm/module_llm.h/.cpp`(`vlmapp::ModuleLlmClient`)から
> 外部移植可能なPlatformIOローカルライブラリ **`lib/yolo_object_detector/`**(`yolo_object_detector::Detector`)へ
> 切り出した。**正式な仕様・使い方・依存関係・移植手順は自己完結した [`lib/yolo_object_detector/README.md`](../lib/yolo_object_detector/README.md) を参照すること。**
> 本ファイルはシグネチャの索引のみを残す(詳細はREADMEを正とする)。

`yolo_object_detector::Detector` が `M5ModuleLLM` ライブラリの薄いラッパー(YOLO物体検出用)。
「JPEGバイト列 IN → 検出物体の座標列 OUT」だけを提供し、カメラ/LCD(`src/hal/`)には依存しない。

## 接続・起動
- `bool begin(HardwareSerial& serial, uint32_t baud_rate = 115200)` — UART初期化込み(pinは`M5.getPin(port_c_rxd/txd)`を自動解決)。`M5.begin()`より後に呼ぶこと
- `void waitForConnection()` — 接続できるまでブロック(タイムアウトなし)
- `void connectAutoBaud()` — 115200 ↔ `kYoloBaudRate` を交互にpingして接続し、**接続後は必ず115200へ戻す**。CoreS3だけ再起動してモジュールが前回の1.5Mbpsのまま残っていても復帰できる([known_issues.md](known_issues.md)参照)。`main.cpp`は`waitForConnection()`の代わりにこちらを使う
- `void resetModule()` — `sys.reset()`
- `bool setBaudRate(uint32_t baud_rate)` — モジュール/ホスト双方のUARTボーレートを変更。`begin()`後・YOLOセットアップ前に`kYoloBaudRate`(1.5Mbps)で呼ぶ

## YOLO物体検出
- `bool setupYolo(const String& model = kDefaultYoloModel)` — `yolo.setup`。既定モデルは`kDefaultYoloModel`(`"yolo11n"`)。`sys.reset()`直後は汎用work_id`"yolo"`が返ることがあるため内部でリトライする
- `bool isReady() const`, `const String& yoloWorkId() const`
- `bool detect(const uint8_t* jpeg_data, size_t jpeg_len, const std::function<void(const DetectedObject&)>& onDetection, uint32_t timeout_ms = 500)` — JPEG1枚をYOLO推論し、検出ごとに`onDetection`を呼ぶ(同期・ブロッキング)。finish時の空チャンク・不正JSONはスキップする。`main.cpp`はリファレンスに合わせて`timeout_ms`に短い値(既定10ms)を渡す
- `struct DetectedObject { String class_name; float confidence; int x1,y1,x2,y2; }` — bboxは入力JPEG画像のピクセル空間、`[x1,y1,x2,y2]`の対角コーナー座標(実機検証済み・公開契約として確定)

## 定数
- `extern const char* const kDefaultYoloModel;` — `"yolo11n"`
- `extern const uint32_t kYoloBaudRate;` — `1500000`

## 関連ドキュメント
- パッケージ本体(自己完結したREADME): [`lib/yolo_object_detector/README.md`](../lib/yolo_object_detector/README.md)
- 切り出し計画: [portable_yolo_package_plan.md](portable_yolo_package_plan.md)
- StackFlowプロトコル全体像: [protocol.md](protocol.md)
- このAPI実装に関わる既知の不具合(修正済み): [known_issues.md](known_issues.md)
- コード構成: [architecture.md](architecture.md)
