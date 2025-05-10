#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// XIAO ESP32S3のピン定義
#define TFT_CS         4  // チップセレクト
#define TFT_RST        6  // リセット
#define TFT_DC         5  // データ/コマンド

// ディスプレイ管理クラス
class Display {
public:
    // コンストラクタ
    Display();
    
    // 初期化関数
    void init();
    
    // メモリ情報表示関数
    void showMemoryInfo(bool forceRedraw = false);
    
    // 表示用のTFTインスタンス（外部からアクセス可能）
    Adafruit_ST7735 tft;
    
private:
    
    // メモリ情報変数（前回値の保存用）
    uint32_t lastFreeHeap;
    uint32_t lastMinFreeHeap;
    uint8_t lastUsedPercentage;
    bool needFullRedraw; // 完全な再描画が必要かのフラグ
    
    // 表示が消えるのを防ぐためのカウンター
    uint8_t updateCounter;
    uint8_t forceRedrawInterval; // この回数ごとに強制的に再描画
};

#endif // DISPLAY_H
