# 未確定事項 / 実機で要検証

現在の構成はオンデバイスYOLO物体検出のみ(VLM・WiFi・外部API連携は2026-09-14に撤去)。実機で要検証の項目:

- UART TX/RXピン: `M5.getPin(m5::pin_name_t::port_c_rxd)` / `port_c_txd` で取得する(解決済み。固定の数値ではなくAPIで取得すること)
- YOLOモデル`yolo11n`(`kDefaultYoloModel`、ライブラリ既定)がModule LLM側にインストール済みか。
  未インストールなら`setupYolo`が失敗する(→[known_issues.md](known_issues.md)のsetup失敗の項)。初回ダウンロード/
  書き込み手順は未確定
- **`yolo11n`が返すクラスラベルの正確な文字列(COCO 80クラス想定だが要確認)**。ラベル表示・将来のフィルタ実装時に
  実機のSerial/画面表示でラベル表記を確認すること
- `detect()`の`timeout_ms`が妥当か。`main.cpp`はリファレンスに合わせて`kInferenceTimeoutMs`(既定10ms)を渡す。
  検出0件でモジュールが`finish`を返さない場合、この時間だけ空振りしてライブフレームレートに影響する可能性がある
  (ラッパーの既定値は500ms)
- YOLOのbbox座標系: **実機検証済み・確定**。`DetectedObject`(旧`YoloDetection`)の`[x1,y1,x2,y2]`は
  入力JPEG画像のピクセル空間における対角コーナー座標(左上・右下)。`showCameraFrameWithOverlay()`は
  `w=x2-x1, h=y2-y1`として矩形を描く。この座標系を`lib/yolo_object_detector`の公開契約として確定した
  (幅/高さ`[x,y,w,h]`形式ではない)。QVGA(320x240)とCoreS3 LCD(320x240)は1:1のためオフセット不要。
- `kMinConfidence`(既定0.30、`src/main.cpp`)の閾値が誤検出抑制として妥当か
- YOLO用ボーレート1.5Mbps(`kYoloBaudRate`)への切り替えが実機のM5-Bus配線で安定するか
- `connectAutoBaud()`による復帰(モジュールが前回1.5Mbpsの状態から115200へ戻す)が実機で確実に動くか

各エージェントは、実装中に上記が確定したらこのファイルを更新すること。
