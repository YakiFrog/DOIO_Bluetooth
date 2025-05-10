#include <Adafruit_GFX.h>    // グラフィックスコアライブラリ
#include <Adafruit_ST7735.h> // ST7735ディスプレイ用ライブラリ
#include <SPI.h>

// ESP32のメモリ監視関数
#include "esp_system.h"

// XIAO ESP32S3のピン定義
#define TFT_CS         4  // チップセレクト
#define TFT_RST        6  // リセット
#define TFT_DC         5  // データ/コマンド

// ハードウェアSPIを使用したST7735ディスプレイのインスタンス化
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// メモリ情報変数（前回値の保存用）
uint32_t lastFreeHeap = 0;
uint32_t lastMinFreeHeap = 0;
uint8_t lastUsedPercentage = 0;
bool needFullRedraw = true; // 完全な再描画が必要かのフラグ

// 表示が消えるのを防ぐためのカウンター
uint8_t updateCounter = 0;
uint8_t forceRedrawInterval = 10; // この回数ごとに強制的に再描画

void showMemoryInfo();

void setup(void) {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(1000); // 起動時の安定化待機

  Serial.println(F("ESP32 メモリモニター - XIAO ESP32S3"));

  // 0.96インチST7735Sの初期化（小型ディスプレイ用）
  tft.initR(INITR_MINI160x80);  // 0.96インチ、160x80サイズの初期化
  
  // 横向き表示（横長）に設定
  tft.setRotation(3);  // 1: 時計回りに90度（横）、3: 時計回りに270度（横、反転）

  // SPIスピードを調整（チラつき防止のため少し遅くする）
  tft.setSPISpeed(12000000); // 12MHz（デフォルトより遅め）

  Serial.println(F("ディスプレイ初期化完了"));
  Serial.print(F("画面サイズ: "));
  Serial.print(tft.width());
  Serial.print(F("x"));
  Serial.println(tft.height());

  // 画面を黒でクリア
  tft.fillScreen(ST77XX_BLACK);
  
  // 初回のメモリ情報表示
  showMemoryInfo();
}

void loop() {
  // メモリ情報の定期更新
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 3000) {  // 3秒ごとに更新
    lastUpdate = millis();
    
    // 定期的に強制再描画フラグを立てる (表示が消えるのを防ぐため)
    updateCounter++;
    if (updateCounter >= forceRedrawInterval) {
      updateCounter = 0;
      needFullRedraw = true;
      Serial.println(F("強制的に画面をリフレッシュします"));
    }
    
    // メモリ情報を表示
    showMemoryInfo();
  }
}

// メモリ情報表示関数（常に表示されるよう改良）
void showMemoryInfo() {
  // ESP32のメモリ情報を取得
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  uint8_t usedPercentage = 100 - (freeHeap * 100) / totalHeap;
  
  // シリアルモニターにメモリ情報を表示
  Serial.print(F("メモリ情報 - 空き: "));
  Serial.print(freeHeap);
  Serial.print(F(" bytes, 合計: "));
  Serial.print(totalHeap);
  Serial.print(F(" bytes, 最小空き: "));
  Serial.print(minFreeHeap);
  Serial.print(F(" bytes, 使用率: "));
  Serial.print(usedPercentage);
  Serial.println(F("%"));

  // 完全再描画が必要な場合、または初回の場合
  if (needFullRedraw) {
    tft.fillScreen(ST77XX_BLACK);
    
    // ヘッダー部分
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_WHITE);
    tft.println("ESP32 Memory Monitor");
  }
  
  // 常にデータを表示（needFullRedrawフラグに関係なく）
  // 空きメモリ表示
  tft.fillRect(0, 10, tft.width(), 10, ST77XX_BLACK);
  tft.setCursor(0, 10);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Free: ");
  tft.print(freeHeap / 1024);
  tft.println(" KB");
  lastFreeHeap = freeHeap;
  
  // 合計メモリ表示
  tft.fillRect(0, 20, tft.width(), 10, ST77XX_BLACK);
  tft.setCursor(0, 20);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Total: ");
  tft.print(totalHeap / 1024);
  tft.println(" KB");
  
  // 最小空きメモリ表示
  tft.fillRect(0, 30, tft.width(), 10, ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("Min Free: ");
  tft.print(minFreeHeap / 1024);
  tft.println(" KB");
  lastMinFreeHeap = minFreeHeap;
  
  // 使用率とプログレスバーの表示
  // テキスト部分を更新
  tft.fillRect(0, 40, tft.width(), 10, ST77XX_BLACK);
  tft.setCursor(0, 40);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Used: ");
  tft.print(usedPercentage);
  tft.println("%");
  
  // プログレスバーを描画
  int barWidth = tft.width() - 20;  // 左右に少し余白を残す
  int barHeight = 8;
  int barX = 10;
  int barY = tft.height() - 15;
  
  // プログレスバーの枠
  tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);
  
  // プログレスバーの中身（一度黒でクリアしてから描画）
  tft.fillRect(barX+1, barY+1, barWidth-2, barHeight-2, ST77XX_BLACK);
  int filledWidth = ((barWidth-2) * usedPercentage) / 100;
  if (filledWidth > 0) {
    tft.fillRect(barX+1, barY+1, filledWidth, barHeight-2, ST77XX_RED);
  }
  
  lastUsedPercentage = usedPercentage;
  
  // 時間情報を更新
  tft.fillRect(5, tft.height() - 25, tft.width() - 10, 10, ST77XX_BLACK);
  tft.setCursor(5, tft.height() - 25);
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print("Runtime: ");
  tft.print(millis() / 1000);
  tft.print("s");
  
  // 再描画フラグをリセット
  needFullRedraw = false;
}