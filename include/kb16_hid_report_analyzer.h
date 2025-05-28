/**
 * @file kb16_hid_report_analyzer.h
 * @brief DOIO KB16 HIDレポート解析ツール（C++版）ヘッダー
 * 
 * Python版 kb16_hid_report_analyzer.py の C++ 移植版ヘッダー
 * 既存のmain.cppに組み込んで使用するためのインターフェース
 * 
 * @version 1.0.0
 * @date 2025-01-28
 */

#ifndef KB16_HID_REPORT_ANALYZER_H
#define KB16_HID_REPORT_ANALYZER_H

#include <Arduino.h>
#include <string>
#include <vector>
#include <map>

// 設定定数
const uint8_t HID_ANALYZER_REPORT_SIZE = 16;  // DOIO KB16は16バイトレポート
const uint8_t HID_ANALYZER_MODIFIER_INDEX = 1;    // 修飾キーのバイト位置
const uint8_t HID_ANALYZER_KEY_START_INDEX = 2;   // キーコード開始位置

// ログレベル設定
enum AnalyzerLogLevel {
    LOG_LEVEL_NONE = 0,      // ログなし
    LOG_LEVEL_BASIC = 1,     // 基本情報のみ
    LOG_LEVEL_DETAILED = 2,  // 詳細情報
    LOG_LEVEL_DEBUG = 3      // デバッグ情報
};

/**
 * @brief キーボード物理位置構造体
 */
struct AnalyzerKeyPosition {
    uint8_t row;
    uint8_t col;
    const char* label;
};

/**
 * @brief HIDレポート解析統計情報
 */
struct AnalyzerStatistics {
    uint32_t totalReports;                        // 総レポート数
    std::map<uint8_t, uint32_t> keycodeFrequency; // キーコード出現頻度
    std::vector<uint8_t> unresponsiveKeys;        // 反応しないキー一覧
    std::vector<uint8_t> problematicKeys;         // 問題のあるキー一覧
    unsigned long firstReportTime;                // 最初のレポート時刻
    unsigned long lastReportTime;                 // 最後のレポート時刻
};

/**
 * @brief 軽量版HIDレポート解析クラス（組み込み用）
 * 
 * 既存のコードに組み込みやすいように設計された軽量版
 * メモリ使用量を抑え、リアルタイム処理に最適化
 */
class KB16HIDReportAnalyzerLite {
private:
    uint8_t lastReport[HID_ANALYZER_REPORT_SIZE];
    bool hasLastReport;
    AnalyzerStatistics stats;
    AnalyzerLogLevel logLevel;
    unsigned long lastLogTime;
    
    // 内部メソッド
    String hidKeycodeToString(uint8_t keycode, bool shift = false);
    void updateStatistics(const uint8_t* report);
    
public:
    /**
     * @brief コンストラクタ
     * @param level ログレベル
     */
    KB16HIDReportAnalyzerLite(AnalyzerLogLevel level = LOG_LEVEL_BASIC);
    
    /**
     * @brief HIDレポートを解析（軽量版）
     * @param report HIDレポートデータ
     * @param logOutput ログ出力するかどうか
     * @return 解析結果（問題があればtrue）
     */
    bool analyzeReportLite(const uint8_t* report, bool logOutput = true);
    
    /**
     * @brief キーコードが期待された範囲内かチェック
     * @param keycode チェックするキーコード
     * @return 有効なキーコードかどうか
     */
    bool isValidKeycode(uint8_t keycode);
    
    /**
     * @brief 0x09問題の特別チェック
     * @param report HIDレポートデータ
     * @return 0x09キーコードが含まれているかどうか
     */
    bool checkKeycode0x09Issue(const uint8_t* report);
    
    /**
     * @brief 統計情報取得
     * @return 統計情報構造体への参照
     */
    const AnalyzerStatistics& getStatistics() const;
    
    /**
     * @brief 問題のあるキーをレポート
     * @param forceOutput 強制的に出力するかどうか
     */
    void reportProblematicKeys(bool forceOutput = false);
    
    /**
     * @brief ログレベル設定
     * @param level 新しいログレベル
     */
    void setLogLevel(AnalyzerLogLevel level);
    
    /**
     * @brief 統計リセット
     */
    void resetStatistics();
    
    /**
     * @brief 簡易キーマトリックス表示
     * @param report HIDレポートデータ
     */
    void displaySimpleMatrix(const uint8_t* report);
    
    /**
     * @brief レポートの変更点をチェック
     * @param report 新しいレポート
     * @return 変更があったかどうか
     */
    bool hasReportChanged(const uint8_t* report);
};

// グローバル関数（既存コードとの統合用）

/**
 * @brief HIDレポート解析の初期化
 * 既存のsetup()関数から呼び出す
 */
void initHIDReportAnalyzer();

/**
 * @brief HIDレポート解析処理（統合用）
 * 既存のprocessDOIOKB16Report()関数内から呼び出す
 * 
 * @param report HIDレポートデータ
 * @param lastReport 前回のレポートデータ
 * @return 問題が検出された場合true
 */
bool analyzeHIDReportIntegrated(const uint8_t* report, const uint8_t* lastReport);

/**
 * @brief 定期的な統計情報出力
 * メインループから定期的に呼び出す
 */
void periodicAnalyzerReport();

/**
 * @brief キーコードの有効性チェック（クイック）
 * @param keycode チェックするキーコード
 * @return 有効なキーコードかどうか
 */
bool isKeycodeValid(uint8_t keycode);

/**
 * @brief 0x09問題の検出（クイック）
 * @param report HIDレポートデータ
 * @return 0x09問題が検出された場合true
 */
bool detect0x09Issue(const uint8_t* report);

// デバッグ用マクロ
#define ANALYZER_DEBUG_PRINT(level, ...) do { \
    if (level <= getCurrentAnalyzerLogLevel()) { \
        Serial.printf("[ANALYZER] " __VA_ARGS__); \
        Serial.println(); \
    } \
} while(0)

// 現在のログレベル取得用
AnalyzerLogLevel getCurrentAnalyzerLogLevel();

#endif // KB16_HID_REPORT_ANALYZER_H
