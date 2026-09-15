# コード構成とエージェントの担当範囲

| ディレクトリ/ファイル | 担当エージェント | 内容 |
|---|---|---|
| `src/hal/` | `cores3-hal` | カメラ・LCD・ボタン等CoreS3周辺機器の抽象化 |
| `lib/yolo_object_detector/` | `module-llm-protocol` | Module LLMとのUART/StackFlow通信ラッパー(YOLO用)。JPEG IN→座標 OUTの外部移植可能パッケージ |
| `src/main.cpp` | `vlm-app` | 全体のループ・UIフロー |
| (プロジェクト全体) | `pio-build-check` | `pio run` によるビルド検証専任(実装はしない) |
| (プロジェクト全体) | `cpp-code-review` | 命名規則・メモリ所有権・ポインタ寿命等のコード品質レビュー専任(実装はしない) |

新しいエージェントを起動する際は上記の担当範囲を尊重し、他エージェントの担当ファイルを編集する必要がある場合は理由を明記すること。

## src/main.cpp 実装済み(vlm-app担当)

`cores3_hal`・`yolo_object_detector::Detector`(`lib/yolo_object_detector/`パッケージ)を組み合わせたループを
実装済み。各モジュールの中身には手を入れず、公開APIのみ呼び出している。制御フローはリファレンス実装(`reference/yolo_example.ino`、
ライブラリ同梱の `YOLO_CoreS3.ino`)に忠実なオンデバイスYOLO物体検出で、カメラ映像の上に検出した
被写体すべてをバウンディングボックス+「クラス名 信頼度」ラベルで重ねてライブ表示する。

撮影・検出シーケンス(`loop()`内、毎フレーム):
1. `cameraFrameToJpeg()`でJPEG取得
2. `g_llm_client.detect(jpg, len, onDetection, kInferenceTimeoutMs)`で検出結果(`DetectedObject`:
   class/confidence/bbox)を集める。`kMinConfidence`(既定0.30)未満は除外する。タイムアウトは
   リファレンスのloop()に合わせて`kInferenceTimeoutMs`(既定10ms)
3. 検出を`cores3_hal::OverlayBox`へ変換し、`showCameraFrameWithOverlay()`でカメラ映像+枠+ラベルを
   合成表示する(オフスクリーンcanvasへ描いてから一括転送、ちらつき防止)。枠色は`kBoxColor`
   (オレンジ)の単色
4. JPEGバッファをループ末尾で`releaseJpeg()`、カメラフレームを`cameraFrameRelease()`で解放する

LCD表示は「ライブ映像+検出枠」のみで、WiFi・外部サーバー連携・テキスト対話は行わない。

起動シーケンス(`setup()`)の順序: `cores3_hal::begin()` → `g_llm_client.begin(Serial2)` →
`connectAutoBaud()`(接続後は必ず115200へ戻す) → `resetModule()` → `setBaudRate(kYoloBaudRate)`
(1.5Mbps、フレーム毎JPEG送信のスループット確保) → `setupYolo()`(既定モデル`yolo11n`)。
`Detector::begin()`は`cores3_hal::begin()`(内部で`M5.begin()`を呼ぶ)より後に呼ぶ順序を
厳守している(`M5.getPin()`でUARTピンを解決するため)。

`pio run -e m5stack-cores3` でビルド成功済み(2026-09-14時点)。

## 変更履歴
- 2026-09-15: YOLO検出処理(`src/llm/module_llm.h/.cpp`、`vlmapp::ModuleLlmClient`)を外部移植可能な
  PlatformIOローカルライブラリ`lib/yolo_object_detector/`(`yolo_object_detector::Detector` /
  `DetectedObject` / `detect()`)として切り出した。`src/main.cpp`はincludeと型名の差し替えのみで追従。
  詳細は[portable_yolo_package_plan.md](portable_yolo_package_plan.md)。
- 2026-09-12: オンデバイスのqwen翻訳を廃止し、VLMによる被写体有無判定+外部AIサーバーへの画像送信という
  構成を試みた(後に撤回)。
- 2026-09-12(2)〜2026-09-13: オンデバイスの主機能をVLMからYOLO物体検出へ移行。YOLOモデル指定・
  対象クラス絞り込み・検出時の外部API送信などを試みたが、YOLOモデル名(`llm-yolo`)でsetupに失敗する
  など不安定だった。
- **2026-09-14: リファレンス実装(`reference/yolo_example.ino`)をベースに、YOLO物体検知のみを行う構成へ
  全面整理。** VLM系ラッパー(`src/llm/`)・WiFi/外部API連携(`src/net/`)・秘密情報(`src/secrets.h`)・
  対象クラス絞り込み(`kTargetClasses`)・Serialログ契約(`SUBJECT:`等)をすべて撤去。YOLOモデルは
  ライブラリ既定の`yolo11n`(`kDefaultYoloModel`)へ戻し、`yolo.setup`失敗を解消。検出した被写体は
  すべてオレンジ枠+ラベルでライブ表示する。

## 関連ドキュメント
- 実機確認済みリファレンス: `reference/yolo_example.ino`(カメラ初期化・UARTピン取得・YOLO API呼び出し順序の
  一次情報。迷ったら推測せずこのファイルを読むこと)
- YOLO検出パッケージの公開API: [module_llm_api.md](module_llm_api.md)(実体は `lib/yolo_object_detector/README.md`)
- 既知の不具合: [known_issues.md](known_issues.md)
- 未確定事項・実機で要検証: [open_questions.md](open_questions.md)
