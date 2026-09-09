#include "cores3_hal.h"

#include <M5CoreS3.h>
#include <esp_camera.h>
#include <cstdarg>

namespace cores3_hal {

namespace {

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
