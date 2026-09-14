/*
 * cores3_hal.h
 *
 * M5Stack CoreS3 ハードウェア抽象化層(HAL)。
 *
 * 担当範囲: カメラ(GC0308)フレーム取得+JPEG圧縮、LCD表示、タッチ入力。
 * このヘッダは他エージェント(vlm-app等)が呼び出すための公開APIのみを定義する。
 *
 * 2026-09-09: ユーザーの実機動作実績コード
 * (c:\Users\takes\OneDrive\Documents\Arduino\sketch_sep7a\sketch_sep7a.ino =
 *  reference/vlm_touch_example.cpp) の呼び出し順序・APIに忠実に合わせて全面書き直し。
 * 独自の抽象化(CoreS3.begin(cfg)経由の初期化、BOOT_ERRORリトライ等)は排除し、
 * 「動作実績のある呼び出しをそのまま行う」ことを最優先にしている。
 *
 * 使い方の概要(main.cppのloop()から呼ばれる想定、sketch_sep7a.inoのloop()と同型):
 *   if (cores3_hal::cameraFrameAvailable()) {
 *       if (...) {
 *           uint8_t* jpg; size_t len;
 *           cores3_hal::cameraFrameToJpeg(&jpg, &len);
 *           ...
 *           cores3_hal::releaseJpeg(jpg);
 *       } else {
 *           cores3_hal::showCameraFramePreview();
 *       }
 *       cores3_hal::cameraFrameRelease();
 *   }
 */
#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace cores3_hal {

// --- 初期化 ---

// Serial(デバッグログ用)・M5本体・LCD・カメラを初期化する。setup()の先頭で一度だけ呼ぶこと。
void begin();

// CoreS3内部状態(タッチ等)を最新化する。loop()の先頭で毎回呼ぶこと。
void update();

// --- タッチ入力 ---
// sketch_sep7a.ino と同じく、loop()毎に呼んでクリック/フリックを判定する。

bool touchWasClicked();
bool touchWasFlicked();

// --- カメラ ---
// sketch_sep7a.ino の `if (CoreS3.Camera.get()) { ... } CoreS3.Camera.free();` に対応する3つの関数。
// cameraFrameAvailable()がtrueを返した場合、必ず対になる cameraFrameRelease() を呼ぶこと。

bool cameraFrameAvailable();
void cameraFrameRelease();

// 現在取得中のカメラフレームをJPEGへ圧縮する(frame2jpg準拠)。
// out_jpg/out_len: 成功時にJPEGバッファの先頭ポインタとサイズを書き込む。
// quality: JPEG圧縮品質(0-255、値が小さいほど高画質・低圧縮率)。既定50はsketch_sep7a.inoに合わせている。
bool cameraFrameToJpeg(uint8_t** out_jpg, size_t* out_len, uint8_t quality = 50);

// cameraFrameToJpeg() で取得したJPEGバッファを解放する。
void releaseJpeg(uint8_t* jpg);

// 現在取得中のカメラフレーム(生RGB565)を画面全体にそのまま表示する(ライブプレビュー)。
void showCameraFramePreview();

// 検出結果を重ねて描画するための矩形1件。座標はカメラフレームのピクセル空間
// ([x1,y1]-[x2,y2] の対角コーナー)。表示解像度への拡縮はHAL側で行う。
// label は矩形の左上に添える文字列(nullptr可)。呼び出し中だけ有効なポインタでよい。
struct OverlayBox {
    int x1;
    int y1;
    int x2;
    int y2;
    const char* label;
    uint16_t color;
};

// 現在取得中のカメラフレームを画面全体に表示し、その上に検出矩形(boxes)を重ねて描画する。
// ちらつきを避けるためオフスクリーンcanvasへ合成してから一括転送する。
// cameraFrameToJpeg() 等と同様、対になる cameraFrameRelease() を呼ぶ前に使うこと。
void showCameraFrameWithOverlay(const OverlayBox* boxes, size_t count);

// --- LCD ---

// 画面全体を消去する。
void clearDisplay();

// ステータス/ログ用の白文字を現在のカーソル位置から追記する(改行なし、printf形式)。
void printLine(const char* fmt, ...);

// エラー用の赤文字を現在のカーソル位置から追記する(改行なし、printf形式)。
void printError(const char* fmt, ...);

// 推論結果表示用にカーソルを左上へ戻す(画面クリアはしない。sketch_sep7a.ino準拠)。
void setResultCursorTopLeft();

// 推論結果のテキストチャンクを現在のカーソル位置から追記する(改行なし)。
void printResultChunk(const String& text);

// 日本語(CJK)テキスト表示用に、M5GFX内蔵の日本語ゴシックフォントへ切り替える。
// 既定のフォント(setTextSize()ベース)は日本語グリフを含まないため、
// 日本語文字列を表示する前に必ず呼ぶこと。
void useJapaneseFont();

// useJapaneseFont()の前の既定フォント(ASCII用)に戻す。
void useDefaultFont();

}  // namespace cores3_hal
