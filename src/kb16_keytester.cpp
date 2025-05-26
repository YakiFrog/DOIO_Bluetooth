/**
 * @file kb16_keytester.cpp
 * @brief DOIO KB16キーテスター（ESP32版）
 */

#include "kb16_keytester.h"

// staticメンバー変数の定義
const char KB16KeyTester::KEY_NAMES[4][4] = {
    {'A', 'B', 'C', 'D'},
    {'E', 'F', 'G', 'H'},
    {'I', 'J', 'K', 'L'},
    {'M', 'N', 'O', 'P'}
};

// キーコードマッピングテーブルを定義
// このマッピングはキーボードの実際のUSBキーコードとマトリックス位置の対応
// KB16キーボードマトリックスは abcd efgh ijkl mnop 配置
const KB16KeyTester::KeyMapEntry KB16KeyTester::KEY_MAP[] = {
    // 標準キーボード配列（HIDキーコード）
    {0x04, 0, 0}, // A
    {0x05, 0, 1}, // B
    {0x06, 0, 2}, // C
    {0x07, 0, 3}, // D
    {0x08, 1, 0}, // E
    {0x09, 1, 1}, // F
    {0x0A, 1, 2}, // G
    {0x0B, 1, 3}, // H
    {0x0C, 2, 0}, // I
    {0x0D, 2, 1}, // J
    {0x0E, 2, 2}, // K
    {0x0F, 2, 3}, // L
    {0x10, 3, 0}, // M
    {0x11, 3, 1}, // N
    {0x12, 3, 2}, // O
    {0x13, 3, 3}, // P

    // 数字キー（1-9, 0）もマッピング
    {0x1E, 0, 0}, // 1
    {0x1F, 0, 1}, // 2
    {0x20, 0, 2}, // 3
    {0x21, 0, 3}, // 4
    {0x22, 1, 0}, // 5
    {0x23, 1, 1}, // 6
    {0x24, 1, 2}, // 7
    {0x25, 1, 3}, // 8
    {0x26, 2, 0}, // 9
    {0x27, 2, 1}, // 0

    // ファンクションキー
    {0x3A, 0, 0}, // F1
    {0x3B, 0, 1}, // F2
    {0x3C, 0, 2}, // F3
    {0x3D, 0, 3}, // F4
    {0x3E, 1, 0}, // F5
    {0x3F, 1, 1}, // F6
    {0x40, 1, 2}, // F7
    {0x41, 1, 3}, // F8
    {0x42, 2, 0}, // F9
    {0x43, 2, 1}, // F10
    {0x44, 2, 2}, // F11
    {0x45, 2, 3}, // F12
};

// 配列サイズの計算
const size_t KB16KeyTester::KEY_MAP_SIZE = sizeof(KEY_MAP) / sizeof(KEY_MAP[0]);

KB16KeyTester::KB16KeyTester(Adafruit_ST7735& tft, EspUsbHost& usbHost)
    : _tft(tft), _usbHost(usbHost), _lastKeyUpdateTime(0), _lastKeyCode(0), _needRedraw(true) {
    
    // キー状態を初期化
    memset(_keyStates, 0, sizeof(_keyStates));
}

void KB16KeyTester::begin() {
    // 画面の初期化
    _tft.fillScreen(COLOR_BG);
    _tft.setTextColor(COLOR_TEXT);
    _tft.setTextSize(1);
    
    // タイトルの表示（小さいサイズで）
    _tft.setCursor(2, 2);
    _tft.print("KB16 Tester v1.0");
    _tft.drawLine(0, 12, _tft.width(), 12, COLOR_TEXT);
    
    // キーグリッドの描画
    drawKeyTester();
    
    // 情報表示
    _tft.fillRect(0, _tft.height() - 18, _tft.width(), 18, COLOR_BG);
    _tft.setCursor(2, _tft.height() - 18);
    _tft.println("Ready for key input");
    _tft.setCursor(2, _tft.height() - 9);
    _tft.println("Press any key...");
}

void KB16KeyTester::update() {
    // キーの状態が変わったか一定時間経過したら再描画
    if (_needRedraw) {
        drawKeyTester();
        _needRedraw = false;
    }

    // 最後のキー更新から5秒以上経過したらリセット
    if (_lastKeyUpdateTime > 0 && (millis() - _lastKeyUpdateTime > 5000)) {
        resetKeys();
        _lastKeyUpdateTime = 0;
        
        Serial.println("キー状態タイムアウト: 全キーをリセットしました");
    }
    
    // キーテスター情報の定期更新
    static unsigned long lastDisplayUpdateTime = 0;
    if (millis() - lastDisplayUpdateTime > 60000) { // 1分ごとに表示を更新
        lastDisplayUpdateTime = millis();
        
        // キーが押されていない状態が続いている場合の表示
        if (_lastKeyUpdateTime == 0) {
            _tft.fillRect(0, _tft.height() - 18, _tft.width(), 18, COLOR_BG);
            _tft.setCursor(2, _tft.height() - 18);
            _tft.println("KB16 Key Tester");
            _tft.setCursor(2, _tft.height() - 9);
            _tft.println("Waiting for keys...");
        }
    }
}

void KB16KeyTester::drawKeyTester() {
    // キーグリッドの描画
    for (uint8_t row = 0; row < 4; row++) {
        for (uint8_t col = 0; col < 4; col++) {
            drawKey(row, col, _keyStates[row][col]);
        }
    }
}

void KB16KeyTester::drawKey(uint8_t row, uint8_t col, bool highlight) {
    uint16_t x = GRID_OFFSET_X + col * (KEY_SIZE + KEY_GAP);
    uint16_t y = GRID_OFFSET_Y + row * (KEY_SIZE + KEY_GAP);
    
    // キーの背景を描画
    uint16_t keyColor = highlight ? COLOR_KEY_ACTIVE : COLOR_KEY_BG;
    _tft.fillRect(x, y, KEY_SIZE, KEY_SIZE, keyColor);
    
    // キーの境界線を描画（押されたキーは太く）
    if (highlight) {
        _tft.drawRect(x, y, KEY_SIZE, KEY_SIZE, COLOR_TEXT);
    }
    
    // キーの文字を描画
    _tft.setTextColor(COLOR_TEXT);
    _tft.setTextSize(1);
    
    // 中央に文字を表示
    char keyChar = KEY_NAMES[row][col];
    uint8_t charWidth = 6; // 6x8フォントの場合の幅
    uint8_t charHeight = 8; // 6x8フォントの場合の高さ
    
    uint16_t textX = x + (KEY_SIZE - charWidth) / 2;
    uint16_t textY = y + (KEY_SIZE - charHeight) / 2;
    
    _tft.setCursor(textX, textY);
    _tft.print(keyChar);
}

void KB16KeyTester::updateKey(uint8_t keyCode, bool isDown) {
    // デバッグ出力（デバッグのために常に出力）
    Serial.printf("キー入力: コード=0x%02X, 状態=%s\n", 
                 keyCode, isDown ? "ON" : "OFF");

    // キーコードからマトリックス位置を検索
    for (size_t i = 0; i < KEY_MAP_SIZE; i++) {
        if (KEY_MAP[i].keyCode == keyCode) {
            uint8_t row = KEY_MAP[i].row;
            uint8_t col = KEY_MAP[i].col;
            
            Serial.printf("キーマップが見つかりました: 0x%02X -> (%d,%d)\n", keyCode, row, col);
            
            // キー状態を更新
            if (_keyStates[row][col] != isDown) {
                _keyStates[row][col] = isDown;
                _needRedraw = true;
            }
            
            // キーが押された場合は最後のキーコードとして記録
            if (isDown) {
                _lastKeyCode = keyCode;
                _lastKeyUpdateTime = millis();
                
                // 最後に押されたキーの表示を更新
                _tft.fillRect(5, _tft.height() - 15, _tft.width() - 10, 10, COLOR_BG);
                _tft.setCursor(5, _tft.height() - 15);
                _tft.print("Key: 0x");
                _tft.print(keyCode, HEX);
                _tft.print(" (");
                _tft.print(KEY_NAMES[row][col]);
                _tft.print(")");
            }
            
            break;
        }
    }
}

void KB16KeyTester::updateKeyPosition(uint8_t row, uint8_t col, bool isDown) {
    // 範囲チェック
    if (row >= 4 || col >= 4) {
        return;
    }
    
    // キー状態を更新
    if (_keyStates[row][col] != isDown) {
        _keyStates[row][col] = isDown;
        _needRedraw = true;
    }
    
    // キーが押された場合は最後の情報として記録
    if (isDown) {
        _lastKeyUpdateTime = millis();
        
        // キー情報表示エリアをクリア
        _tft.fillRect(0, _tft.height() - 18, _tft.width(), 18, COLOR_BG);
        
        // キー情報を表示（一行目）
        _tft.setCursor(2, _tft.height() - 18);
        _tft.print("Key: ");
        _tft.print(KEY_NAMES[row][col]);
        _tft.print(" (");
        _tft.print(row);
        _tft.print(",");
        _tft.print(col);
        _tft.print(")");
        
        // キー情報を表示（二行目）
        _tft.setCursor(2, _tft.height() - 9);
        _tft.print("HID: 0x");
        uint8_t keyHID = 0x04 + (row * 4 + col); // HIDコードの計算
        _tft.print(keyHID, HEX);
        
        // シリアルでも出力
        Serial.printf("キー情報: %c (位置: %d,%d), HIDコード: 0x%02X\n", 
                     KEY_NAMES[row][col], row, col, keyHID);
    } else {
        // キーが離された場合
        Serial.printf("キーを離しました: %c (位置: %d,%d)\n", KEY_NAMES[row][col], row, col);
    }
}

void KB16KeyTester::resetKeys() {
    // 全キー状態をリセット
    memset(_keyStates, 0, sizeof(_keyStates));
    _needRedraw = true;
    
    // キー情報表示もリセット
    _tft.fillRect(0, _tft.height() - 18, _tft.width(), 18, COLOR_BG);
    _tft.setCursor(2, _tft.height() - 18);
    _tft.println("Keys reset");
    _tft.setCursor(2, _tft.height() - 9);
    _tft.println("Ready for input");
}
