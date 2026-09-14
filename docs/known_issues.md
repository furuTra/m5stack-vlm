# 既知の不具合(修正済み)

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
- **YOLOの`yolo.setup`が失敗する(work_idが返らない)**: `kDefaultYoloModel`に、モジュールに存在しない
  パッケージ名`"llm-yolo"`を指定していたため、`yolo.setup`が正式なwork_idを返さずセットアップに失敗していた
  (2026-09-13に一時導入)。リファレンス(`reference/yolo_example.ino`)/ライブラリ同梱`YOLO_CoreS3.ino`は
  `yolo.setup()`を引数なしで呼びライブラリ既定の`"yolo11n"`を使っている。これに合わせて`kDefaultYoloModel`を
  `"yolo11n"`へ戻して解消した(2026-09-14、`src/llm/module_llm.cpp`)。※`yolo11n`がモジュールにインストール済みで
  あることが前提([open_questions.md](open_questions.md)参照)。
- **CoreS3だけ再起動すると「Check ModuleLLM connection..」で無限に止まる**: YOLO化で起動時に
  `setBaudRate(1.5Mbps)`するようにしたが、モジュール側のUARTボーレートは**電源を切らない限り保持される**。
  CoreS3のみ再起動(再書き込み含む)すると、CoreS3は`begin()`で115200を開くのに対しモジュールは1.5Mbpsのままで
  会話できず、`checkConnection()`(=`sys.ping()`、1回あたり最大2秒待つ)が永久に成功しない不具合を確認した
  (2026-09-12(2))。`ModuleLlmClient::connectAutoBaud()`を追加し、115200 ↔ `kYoloBaudRate` を交互にpingして
  接続、接続後は必ず115200へ戻してから起動シーケンスを続けるよう修正(`src/main.cpp`は
  `waitForConnection()`の代わりにこれを呼ぶ)。※実機での最終確認は要(モジュールが前回1.5Mbpsの状態からの復帰)。

## 関連ドキュメント
- 全体構成: [../CLAUDE.md](../CLAUDE.md)
- ラッパーAPI: [module_llm_api.md](module_llm_api.md)
