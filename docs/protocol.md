# 通信プロトコル: StackFlow

Module LLMとはUART(またはTCP)上でNDJSON(改行区切りJSON)をやり取りする「StackFlow」プロトコルで通信するが、
**Arduinoからは生JSONを組む必要はなく、公式`M5ModuleLLM`ライブラリ(クラス名は`M5ModuleLLM`、`m5stack/M5Module-LLM`パッケージ)が
C++ APIとしてラップしている。** 実機確認済みの使用例は `reference/yolo_example.ino`(およびライブラリ同梱の
`examples/YOLO_CoreS3/YOLO_CoreS3.ino`)を参照。

## 実績のあるAPI呼び出し順序(YOLO)
```cpp
M5ModuleLLM module_llm;
int rxd = M5.getPin(m5::pin_name_t::port_c_rxd);
int txd = M5.getPin(m5::pin_name_t::port_c_txd);
Serial2.begin(115200, SERIAL_8N1, rxd, txd);
module_llm.begin(&Serial2);
while (!module_llm.checkConnection()) {}        // 接続できるまで待つ
module_llm.sys.reset();                         // モジュールリセット

module_llm.setBaudRate(1500000);                // 高スループット化(フレーム毎JPEG送信のため)
Serial2.begin(1500000, SERIAL_8N1, rxd, txd);   // ホスト側UARTも開き直す
module_llm.begin(&Serial2);

String yolo_work_id = module_llm.yolo.setup();  // 既定モデル "yolo11n"。失敗時は空/"yolo"

// 毎フレーム: JPEGを送って推論し、検出ごとにコールバックが呼ばれる
module_llm.yolo.inferenceAndWaitResult(
    yolo_work_id, out_jpg, out_jpg_len,
    [](String& result) {
        // result は1検出分のJSON: {"class","confidence","bbox":[x1,y1,x2,y2]}
    },
    /*timeout=*/10);
```

内部的にはStackFlowのNDJSON(`yolo.setup`/`yolo.inference`等)がやり取りされているが、実装上は上記C++ APIを使えば十分。
`lib/yolo_object_detector/`パッケージの`yolo_object_detector::Detector`がこの順序を薄くラップしている
(接続はボーレート自動検出付きの`connectAutoBaud()`を使う)。

## 生プロトコルの詳細(参考)
`yolo.setup`が送るコマンドの例(ライブラリ`ApiYolo::setup`が組み立てる):
```json
{"request_id":"yolo_setup","work_id":"yolo","action":"setup","object":"yolo.setup",
 "data":{"model":"yolo11n","response_format":"yolo.box.stream",
 "input":["yolo.jpeg.base64"],"enoutput":true}}
```
画像はbase64 JPEGで渡す。検出結果はストリーミング(`yolo.box.stream`)で、1検出ごとに
`data.delta`へ`{"class","confidence","bbox"}`が返り、最後に`data.finish=true`が来る。

## 関連ドキュメント
- ハードウェア構成: [hardware.md](hardware.md)
- `lib/yolo_object_detector/`の薄いラッパーAPI: [module_llm_api.md](module_llm_api.md)
- 既知の不具合(修正済み): [known_issues.md](known_issues.md)
- 実機で要検証の項目: [open_questions.md](open_questions.md)
