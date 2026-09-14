# ハードウェア構成

## 構成
- メインボード: M5Stack CoreS3(内蔵カメラ GC0308、LCD、タッチ)
- 拡張モジュール: M5Stack Module LLM(SoC: Axera AX630C、3.2TOPS INT8 NPU、LPDDR4 4GB、eMMC 32GB)
- 接続: M5-Bus経由のUARTが基本(デフォルト115200bps 8N1。TX/RXピンは`M5.getPin(port_c_rxd/txd)`で
  取得する。YOLO推論時はフレーム毎のJPEG送信に耐えるため1.5Mbpsへ切り替える)。I2C/USB OTGにも対応。
- 電源: アイドル5V@0.5W、最大5V@1.5W

## 参考リンク
- https://docs.m5stack.com/en/module/Module-LLM (ハード仕様)
- https://docs.m5stack.com/en/stackflow/module_llm/arduino_api (Arduino API)
- https://github.com/m5stack/M5Module-LLM (Arduinoライブラリ本体、examples/YOLO_CoreS3あり)
- https://github.com/m5stack/StackFlow (プロトコル/サーバ実装)
- https://github.com/nnn112358/awesome_M5Stack_ModuleLLM (日本語まとめ・関連記事集)

## 依存ライブラリ(platformio.ini へ追加予定)
- `m5stack/M5Unified`, `m5stack/M5CoreS3`(カメラ/LCD)
- `m5stack/M5Module-LLM`(StackFlow通信、Arduinoラッパー)
- `bblanchon/ArduinoJson`(M5Module-LLMの依存)
