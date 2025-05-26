/**
 * @file kb16_keytester.h
 * @brief DOIO KB16キーテスター（ESP32版）
 */

#ifndef KB16_KEYTESTER_H
#define KB16_KEYTESTER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "EspUsbHost.h"

/**
 * @class KB16KeyTester
 * @brief DOIO KB16用のキーテスターモジュール
 */
class KB16KeyTester {
public:
    // 色の定義
    static constexpr uint16_t COLOR_BG = 0x0000;        // 背景色（黒）
    static constexpr uint16_t COLOR_TEXT = 0xFFFF;      // テキスト色（白）
    static constexpr uint16_t COLOR_KEY_BG = 0x7BEF;    // キー背景色（グレー）
    static constexpr uint16_t COLOR_KEY_ACTIVE = 0x07E0; // アクティブキー色（緑）

    /**
     * @brief コンストラクタ
     * @param tft ST7735ディスプレイのインスタンス
     * @param usbHost EspUsbHostのインスタンス
     */
    KB16KeyTester(Adafruit_ST7735& tft, EspUsbHost& usbHost);

    /**
     * @brief 初期化処理
     */
    void begin();

    /**
     * @brief 周期的な更新処理
     */
    void update();

    /**
     * @brief キーテスター画面を表示
     */
    void drawKeyTester();

    /**
     * @brief 押されているキーを更新
     * @param keyCode 押されたキーのコード
     * @param isDown キーが押されたかどうか
     */
    void updateKey(uint8_t keyCode, bool isDown);

    /**
     * @brief キー位置を直接指定して更新（KB16用）
     * @param row 行位置（0-3）
     * @param col 列位置（0-3）
     * @param isDown キーが押されたかどうか
     */
    void updateKeyPosition(uint8_t row, uint8_t col, bool isDown);

    /**
     * @brief 全てのキーハイライトをリセット
     */
    void resetKeys();

private:
    /**
     * @brief 指定位置のキーを描画
     * @param row 行位置
     * @param col 列位置
     * @param highlight ハイライト表示するかどうか
     */
    void drawKey(uint8_t row, uint8_t col, bool highlight);

    Adafruit_ST7735& _tft;  ///< ディスプレイのインスタンス
    EspUsbHost& _usbHost;   ///< USBホストのインスタンス

    // キー状態の保存用配列
    bool _keyStates[4][4];

    // キー名のマトリックス
    static const char KEY_NAMES[4][4];

    // キーコードからマトリックス位置へのマッピング
    struct KeyMapEntry {
        uint8_t keyCode;  ///< キーコード
        uint8_t row;      ///< マトリックスの行
        uint8_t col;      ///< マトリックスの列
    };

    // キーコードマッピングテーブル
    static const KeyMapEntry KEY_MAP[];
    static const size_t KEY_MAP_SIZE;

    unsigned long _lastKeyUpdateTime;  ///< 最後にキーが更新された時間
    uint8_t _lastKeyCode;              ///< 最後に押されたキーのコード
    bool _needRedraw;                  ///< 再描画が必要かどうか
    
    // 画面サイズとキーグリッドのパラメータ
    static constexpr uint16_t GRID_OFFSET_X = 5;
    static constexpr uint16_t GRID_OFFSET_Y = 18;
    static constexpr uint16_t KEY_SIZE = 18;
    static constexpr uint16_t KEY_GAP = 2;
};

#endif // KB16_KEYTESTER_H
