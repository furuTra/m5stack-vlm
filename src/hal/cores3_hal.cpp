#include "cores3_hal.h"

#include <M5CoreS3.h>
#include <esp_camera.h>
#include <cstdarg>

namespace cores3_hal {

namespace {

// 検出矩形をカメラ映像の上へちらつきなく合成するためのオフスクリーンバッファ
// (YOLO_CoreS3.ino の M5Canvas と同じ役割)。begin() で画面サイズ分を確保する。
M5Canvas g_frame_canvas(&M5.Display);
bool g_frame_canvas_ready = false;

void vprint(uint16_t color, const char* fmt, va_list args) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    M5.Display.setTextColor(color, BLACK);
    M5.Display.printf("%s", buf);
}

}  // namespace

void begin() {
    // sketch_sep7a.ino(実機動作実績あり)と同じ初期化順序。
    Serial.begin(115200);
    delay(100);

    M5.begin();
    M5.Display.setTextSize(2);
    M5.Display.setTextScroll(true);
    M5.Display.setTextColor(WHITE, BLACK);

    // 検出オーバーレイ用canvas(PSRAM前提。platformio.iniで board_build.psram = enable)。
    // 確保に失敗した場合はオーバーレイ描画を諦め、素のプレビューにフォールバックする。
    g_frame_canvas_ready = g_frame_canvas.createSprite(M5.Display.width(), M5.Display.height());
    if (g_frame_canvas_ready) {
        g_frame_canvas.setFont(&fonts::Font0);
        g_frame_canvas.setTextSize(1);
    }

    CoreS3.Camera.begin();
    CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
}

void update() {
    M5.update();
}

bool touchWasClicked() {
    return M5.Touch.getDetail().wasClicked();
}

bool touchWasFlicked() {
    return M5.Touch.getDetail().wasFlicked();
}

bool cameraFrameAvailable() {
    return CoreS3.Camera.get();
}

void cameraFrameRelease() {
    CoreS3.Camera.free();
}

bool cameraFrameToJpeg(uint8_t** out_jpg, size_t* out_len, uint8_t quality) {
    if (out_jpg) *out_jpg = nullptr;
    if (out_len) *out_len = 0;
    if (!out_jpg || !out_len) {
        return false;
    }
    return frame2jpg(CoreS3.Camera.fb, quality, out_jpg, out_len);
}

void releaseJpeg(uint8_t* jpg) {
    if (jpg) {
        free(jpg);
    }
}

void showCameraFramePreview() {
    CoreS3.Display.pushImage(0, 0, CoreS3.Display.width(), CoreS3.Display.height(),
                              (uint16_t*)CoreS3.Camera.fb->buf);
}

void showCameraFrameWithOverlay(const OverlayBox* boxes, size_t count) {
    // canvas未確保時は素のプレビューへフォールバック(矩形は描けないが映像は出す)。
    if (!g_frame_canvas_ready) {
        showCameraFramePreview();
        return;
    }

    const int disp_w = CoreS3.Display.width();
    const int disp_h = CoreS3.Display.height();

    // まずカメラ映像をcanvasへ転写する。
    g_frame_canvas.pushImage(0, 0, disp_w, disp_h, (uint16_t*)CoreS3.Camera.fb->buf);

    // 検出座標はカメラフレームのピクセル空間なので、表示解像度へ拡縮する。
    // (CoreS3のLCDとQVGAは共に320x240で通常1:1だが、将来の解像度変更に備えて計算する。)
    const int frame_w = CoreS3.Camera.fb->width;
    const int frame_h = CoreS3.Camera.fb->height;
    const float sx    = (frame_w > 0) ? (float)disp_w / frame_w : 1.0f;
    const float sy    = (frame_h > 0) ? (float)disp_h / frame_h : 1.0f;

    for (size_t i = 0; i < count; ++i) {
        const OverlayBox& b = boxes[i];
        int x = (int)(b.x1 * sx);
        int y = (int)(b.y1 * sy);
        int w = (int)((b.x2 - b.x1) * sx);
        int h = (int)((b.y2 - b.y1) * sy);
        if (w < 0) w = -w, x -= w;  // 座標が逆転して届いても矩形が潰れないよう正規化する。
        if (h < 0) h = -h, y -= h;

        g_frame_canvas.drawRect(x, y, w, h, b.color);

        if (b.label != nullptr && b.label[0] != '\0') {
            // ラベルは矩形の左上外側に。画面上端で切れる場合は矩形内側へ寄せる。
            int label_y = (y >= 10) ? (y - 10) : (y + 2);
            g_frame_canvas.setTextColor(b.color, BLACK);
            g_frame_canvas.drawString(b.label, x, label_y);
        }
    }

    g_frame_canvas.pushSprite(0, 0);
}

void clearDisplay() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
}

void printLine(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(WHITE, fmt, args);
    va_end(args);
}

void printError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(RED, fmt, args);
    va_end(args);
}

void setResultCursorTopLeft() {
    M5.Lcd.setCursor(0, 0);
}

void printResultChunk(const String& text) {
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.printf("%s", text.c_str());
}

void useJapaneseFont() {
    // M5GFX(LovyanGFX)内蔵の日本語ゴシックフォント。外部フォントファイル不要。
    M5.Display.setFont(&fonts::lgfxJapanGothic_20);
}

void useDefaultFont() {
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(2);
}

}  // namespace cores3_hal
