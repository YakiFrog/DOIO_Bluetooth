#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Peripherals.h"

// ディスプレイの設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1

// 特殊文字の定義
#define CHAR_RIGHT 0x10
#define CHAR_LEFT 0x11
#define CHAR_UP 0x12
#define CHAR_DOWN 0x13
#define CHAR_ENTER 0x14

// 特殊キー表示用の構造体
struct KeyDisplay {
    const char* name;  // キーの表示名
    char displayChar;  // 単一文字の場合の表示文字
    bool isSpecial;    // 特殊表示が必要かどうか
};

class DisplayController {
public:
    void begin();
    void updateDisplay();
    void updateStatusDisplay();
    void showKeyPress(char keyChar, uint8_t keycode);
    void showDeviceInfo(const String& manufacturer, const String& productName, 
                        uint16_t idVendor, uint16_t idProduct);
    
    // 生のキーコードを表示するための新しいメソッド
    void showRawKeyCode(uint8_t keycode, const char* description);
    
    void setUsbConnected(bool connected);
    void setBleConnected(bool connected);
    
    void addDisplayText(char c);
    void clearDisplayText();
    
    // 特殊キーの表示名を取得
    const char* getSpecialKeyName(uint8_t keycode);
    
private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    bool usbConnected = false;
    bool bleConnected = false;
    
    String displayText = "";
    const int maxChars = 64;  // 表示テキストの最大文字数
    
    String deviceName = "";
    uint16_t vendorId = 0;
    uint16_t productId = 0;
};

// グローバルインスタンス
extern DisplayController displayController;

#endif // DISPLAY_CONTROLLER_H