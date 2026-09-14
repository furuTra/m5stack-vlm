# M5Stack VLM Project

## 概要
M5Stack CoreS3 + M5Stack Module LLM(旧称 LLM Module、LLM630 Compute Kit相当)を使い、
オンデバイスのYOLOでカメラ画像の物体検知を行うファームウェアをPlatformIO/Arduinoで開発する。
検出した被写体を、カメラのライブ映像の上にバウンディングボックス+「クラス名 信頼度」ラベルで
重ねてLCDに表示する。

詳細はトピック別に`docs/`以下に分割してある。迷ったら該当ドキュメントを読むこと。

## ドキュメント索引
- [docs/hardware.md](docs/hardware.md) — ハードウェア構成、依存ライブラリ、参考リンク
- [docs/protocol.md](docs/protocol.md) — 通信プロトコル(StackFlow)、実績のあるAPI呼び出し順序
- [docs/module_llm_api.md](docs/module_llm_api.md) — `src/llm/module_llm.h`が提供するラッパーAPI
- [docs/known_issues.md](docs/known_issues.md) — 既知の不具合(修正済み)
- [docs/open_questions.md](docs/open_questions.md) — 未確定事項・実機で要検証の項目
- [docs/architecture.md](docs/architecture.md) — コード構成、エージェントの担当範囲、`src/main.cpp`のステートマシン
- [docs/yolo_example_loop.md](docs/yolo_example_loop.md) — `reference/yolo_example.ino`の`loop()`処理のmermaid図解

## 実機確認済みリファレンス
`reference/yolo_example.ino` — YOLO物体検知の一次情報。カメラ初期化・UARTピン取得・ボーレート切替・
`M5ModuleLLM` の YOLO API 呼び出し順序(`yolo.setup()` / `yolo.inferenceAndWaitResult()`)の正解は
このファイルとする。**現行の実装はこのリファレンスをベースにしている。迷ったら推測せずこのファイルを読むこと。**
ライブラリ同梱の `examples/YOLO_CoreS3/YOLO_CoreS3.ino` も同等の内容。

`reference/vlm_touch_example.cpp` — ユーザーが以前作成し実機で動作させたVLMサンプル(参考)。
現在のファームウェアはVLMではなくYOLOを使う構成のため一次情報ではないが、カメラ初期化・UARTピン取得の
書き方は共通しているので補助的に参照してよい。

## エージェントの担当範囲(概要)
| ディレクトリ/ファイル | 担当エージェント |
|---|---|
| `src/hal/` | `cores3-hal` |
| `src/llm/` | `module-llm-protocol` |
| `src/main.cpp` | `vlm-app` |
| (プロジェクト全体、ビルド検証) | `pio-build-check` |
| (プロジェクト全体、C++コード品質レビュー) | `cpp-code-review` |

詳細・注意事項は[docs/architecture.md](docs/architecture.md)を参照。

## エージェント委譲ルール(必須)

過去に方針転換(VLM→YOLO等)を繰り返した際、担当分割が有名無実化し、メインセッションが各ファイルを直接編集して
`.claude/agents/*.md`の記述と実装が乖離する事態が起きた。再発を防ぐため以下を必須とする:

- `src/hal/`・`src/llm/`・`src/main.cpp`への実装変更(バグ修正・新機能・リファクタ問わず)は、
  **原則として必ず対応するエージェント(上表参照)にAgent toolで委譲すること。** メインセッションが
  Editツールで直接編集してよいのは、typo/コメント修正など1〜2行程度の些末な変更に限る
- 複数ファイルにまたがる変更が必要な場合も、メインが一括編集せず、各ファイルをそれぞれの担当エージェントに割り振る
- 実装エージェントの作業が完了しコミット前の段階になったら、`pio-build-check`(ビルド検証)と
  `cpp-code-review`(コード品質レビュー)の両方を必ず呼ぶ。この2エージェントは実装を行わないので、
  実装エージェントへの依頼と別に呼び出すこと
- **エージェント定義(`.claude/agents/*.md`)の担当範囲・前提が実際の実装とズレていると気づいた場合、
  実装を進める前にまずそのエージェント定義ファイルを修正すること。** ズレたまま実装を進めない
- 各エージェントは、実装中に[docs/open_questions.md](docs/open_questions.md)の項目が確定したら該当ドキュメントを更新すること。
  新たに不具合を修正した場合は[docs/known_issues.md](docs/known_issues.md)に追記すること
