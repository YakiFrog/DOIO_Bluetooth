/**
 * @file kb16_hid_report_analyzer_lite.cpp
 * @brief DOIO KB16 HIDレポート解析ツール（軽量統合版）
 * 
 * 既存のmain.cppに組み込んで使用する軽量版実装
 * Python版の主要機能をC++に移植し、組み込みに最適化
 * 
 * @version 1.0.0
 * @date 2025-01-28
 */

#include "kb16_hid_report_analyzer.h"
#include <Arduino.h>

// グローバル変数
static KB16HIDReportAnalyzerLite* g_analyzer = nullptr;
static AnalyzerLogLevel g_currentLogLevel = LOG_LEVEL_BASIC;

// DOIO KB16 キーマッピング（軽量版）
const uint8_t EXPECTED_KEYCODES[] = {
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // A-H
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17   // I-P
};
const uint8_t EXPECTED_KEYCODES_COUNT = sizeof(EXPECTED_KEYCODES) / sizeof(EXPECTED_KEYCODES[0]);

/**
 * @brief KB16HIDReportAnalyzerLite 実装
 */

KB16HIDReportAnalyzerLite::KB16HIDReportAnalyzerLite(AnalyzerLogLevel level) 
    : hasLastReport(false), logLevel(level), lastLogTime(0) {
    memset(lastReport, 0, sizeof(lastReport));
    resetStatistics();
}

String KB16HIDReportAnalyzerLite::hidKeycodeToString(uint8_t keycode, bool shift) {
    // DOIO KB16カスタムマッピング (0x08スタート: A-Z)
    if (keycode >= 0x08 && keycode <= 0x21) {
        char base = 'A' + (keycode - 0x08);
        return String(shift ? base : (base + 32));
    }
    
    // よく使われるキーのみ簡略化
    switch (keycode) {
        case 0x28: return "Enter";
        case 0x29: return "Esc";
        case 0x2A: return "Backspace";
        case 0x2C: return "Space";
        case 0xE0: return "LCtrl";
        case 0xE1: return "LShift";
        case 0xE2: return "LAlt";
        default: return "0x" + String(keycode, HEX);
    }
}

bool KB16HIDReportAnalyzerLite::analyzeReportLite(const uint8_t* report, bool logOutput) {
    bool problemDetected = false;
    stats.totalReports++;
    stats.lastReportTime = millis();
    
    if (stats.totalReports == 1) {
        stats.firstReportTime = millis();
    }
    
    // 基本ログ出力
    if (logOutput && logLevel >= LOG_LEVEL_BASIC) {
        // レポートに変更があった場合のみ出力
        if (hasReportChanged(report)) {
            Serial.printf("[ANALYZER] レポート#%u: ", stats.totalReports);
            
            // 押されているキーのみ表示
            bool hasKeys = false;
            for (int i = HID_ANALYZER_KEY_START_INDEX; i < HID_ANALYZER_REPORT_SIZE; i++) {
                if (report[i] != 0) {
                    if (!hasKeys) {
                        Serial.print("キー=");
                        hasKeys = true;
                    } else {
                        Serial.print(",");
                    }
                    Serial.printf("0x%02X", report[i]);
                }
            }
            
            if (!hasKeys) {
                Serial.print("キー=なし");
            }
            
            Serial.println();
        }
    }
    
    // 詳細ログ出力
    if (logOutput && logLevel >= LOG_LEVEL_DETAILED && hasReportChanged(report)) {
        Serial.print("[ANALYZER] RAW: ");
        for (int i = 0; i < HID_ANALYZER_REPORT_SIZE; i++) {
            Serial.printf("%02X ", report[i]);
        }
        Serial.println();
        
        // 修飾キー表示
        uint8_t modifier = report[HID_ANALYZER_MODIFIER_INDEX];
        if (modifier != 0) {
            Serial.printf("[ANALYZER] 修飾キー: 0x%02X\n", modifier);
        }
    }
    
    // 0x09問題の特別チェック
    if (checkKeycode0x09Issue(report)) {
        if (logLevel >= LOG_LEVEL_BASIC) {
            Serial.println("[ANALYZER] ✅ 0x09キーコード検出 (修正済み問題)");
        }
    }
    
    // 統計情報更新
    updateStatistics(report);
    
    // 無効なキーコードの検出
    for (int i = HID_ANALYZER_KEY_START_INDEX; i < HID_ANALYZER_REPORT_SIZE; i++) {
        if (report[i] != 0 && !isValidKeycode(report[i])) {
            problemDetected = true;
            if (logLevel >= LOG_LEVEL_BASIC) {
                Serial.printf("[ANALYZER] ⚠️ 無効なキーコード: 0x%02X\n", report[i]);
            }
        }
    }
    
    // レポートを保存
    memcpy(lastReport, report, HID_ANALYZER_REPORT_SIZE);
    hasLastReport = true;
    lastLogTime = millis();
    
    return problemDetected;
}

bool KB16HIDReportAnalyzerLite::isValidKeycode(uint8_t keycode) {
    // DOIO KB16で期待されるキーコード範囲
    if (keycode >= 0x08 && keycode <= 0x21) return true;  // A-Z (カスタムマッピング)
    if (keycode >= 0x22 && keycode <= 0x2B) return true;  // 数字
    if (keycode >= 0x28 && keycode <= 0x38) return true;  // 特殊キー
    if (keycode >= 0x3A && keycode <= 0x45) return true;  // ファンクションキー
    if (keycode >= 0xE0 && keycode <= 0xE7) return true;  // 修飾キー
    
    return false;
}

bool KB16HIDReportAnalyzerLite::checkKeycode0x09Issue(const uint8_t* report) {
    for (int i = HID_ANALYZER_KEY_START_INDEX; i < HID_ANALYZER_REPORT_SIZE; i++) {
        if (report[i] == 0x09) {
            return true;
        }
    }
    return false;
}

const AnalyzerStatistics& KB16HIDReportAnalyzerLite::getStatistics() const {
    return stats;
}

void KB16HIDReportAnalyzerLite::reportProblematicKeys(bool forceOutput) {
    static unsigned long lastReportTime = 0;
    unsigned long currentTime = millis();
    
    // 30秒ごと、または強制出力の場合
    if (!forceOutput && (currentTime - lastReportTime < 30000)) {
        return;
    }
    
    lastReportTime = currentTime;
    
    Serial.println("[ANALYZER] === 統計レポート ===");
    Serial.printf("[ANALYZER] 総レポート数: %u\n", stats.totalReports);
    Serial.printf("[ANALYZER] 検出キーコード種類: %u\n", stats.keycodeFrequency.size());
    
    // 0x09問題の確認
    auto it09 = stats.keycodeFrequency.find(0x09);
    if (it09 != stats.keycodeFrequency.end()) {
        Serial.printf("[ANALYZER] ✅ 0x09キーコード: %u回検出\n", it09->second);
    } else {
        Serial.println("[ANALYZER] ⚠️ 0x09キーコード: 未検出");
    }
    
    // 期待されるキーコードで未検出のものを報告
    Serial.println("[ANALYZER] 未検出キー:");
    bool foundUnresponsive = false;
    for (int i = 0; i < EXPECTED_KEYCODES_COUNT; i++) {
        uint8_t expectedKey = EXPECTED_KEYCODES[i];
        if (stats.keycodeFrequency.find(expectedKey) == stats.keycodeFrequency.end()) {
            Serial.printf("[ANALYZER]   - 0x%02X (%c)\n", 
                         expectedKey, 'A' + (expectedKey - 0x08));
            foundUnresponsive = true;
        }
    }
    
    if (!foundUnresponsive) {
        Serial.println("[ANALYZER]   なし");
    }
    
    Serial.println("[ANALYZER] ==================");
}

void KB16HIDReportAnalyzerLite::setLogLevel(AnalyzerLogLevel level) {
    logLevel = level;
    g_currentLogLevel = level;
}

void KB16HIDReportAnalyzerLite::resetStatistics() {
    stats.totalReports = 0;
    stats.keycodeFrequency.clear();
    stats.unresponsiveKeys.clear();
    stats.problematicKeys.clear();
    stats.firstReportTime = 0;
    stats.lastReportTime = 0;
}

void KB16HIDReportAnalyzerLite::displaySimpleMatrix(const uint8_t* report) {
    if (logLevel < LOG_LEVEL_DETAILED) return;
    
    Serial.println("[ANALYZER] マトリックス: ");
    Serial.print("[ANALYZER] ");
    
    bool hasKeys = false;
    for (int i = HID_ANALYZER_KEY_START_INDEX; i < HID_ANALYZER_REPORT_SIZE; i++) {
        if (report[i] != 0) {
            if (hasKeys) Serial.print(" ");
            Serial.printf("[0x%02X]", report[i]);
            hasKeys = true;
        }
    }
    
    if (!hasKeys) {
        Serial.print("[なし]");
    }
    
    Serial.println();
}

bool KB16HIDReportAnalyzerLite::hasReportChanged(const uint8_t* report) {
    if (!hasLastReport) return true;
    
    return memcmp(lastReport, report, HID_ANALYZER_REPORT_SIZE) != 0;
}

void KB16HIDReportAnalyzerLite::updateStatistics(const uint8_t* report) {
    // キーコード出現頻度を更新
    for (int i = HID_ANALYZER_KEY_START_INDEX; i < HID_ANALYZER_REPORT_SIZE; i++) {
        if (report[i] != 0) {
            stats.keycodeFrequency[report[i]]++;
        }
    }
}

/**
 * @brief グローバル関数実装（既存コード統合用）
 */

void initHIDReportAnalyzer() {
    if (!g_analyzer) {
        g_analyzer = new KB16HIDReportAnalyzerLite(LOG_LEVEL_BASIC);
        Serial.println("[ANALYZER] HIDレポート解析ツール初期化完了");
        Serial.println("[ANALYZER] Python版 kb16_hid_report_analyzer.py C++移植版");
    }
}

bool analyzeHIDReportIntegrated(const uint8_t* report, const uint8_t* lastReport) {
    if (!g_analyzer) {
        initHIDReportAnalyzer();
    }
    
    return g_analyzer->analyzeReportLite(report, true);
}

void periodicAnalyzerReport() {
    if (!g_analyzer) return;
    
    g_analyzer->reportProblematicKeys(false);
}

bool isKeycodeValid(uint8_t keycode) {
    if (!g_analyzer) {
        initHIDReportAnalyzer();
    }
    
    return g_analyzer->isValidKeycode(keycode);
}

bool detect0x09Issue(const uint8_t* report) {
    if (!g_analyzer) {
        initHIDReportAnalyzer();
    }
    
    return g_analyzer->checkKeycode0x09Issue(report);
}

AnalyzerLogLevel getCurrentAnalyzerLogLevel() {
    return g_currentLogLevel;
}
