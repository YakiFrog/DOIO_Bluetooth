#!/bin/bash
# DOIO KB16 キーボード自動解析スクリプト

echo "DOIO KB16 キーボード自動解析スクリプト"
echo "============================================"
echo

# 出力ディレクトリ作成
OUTPUT_DIR="kb16_analysis_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_DIR"
echo "出力ディレクトリを作成しました: $OUTPUT_DIR"

# 基本的なシステム情報の収集
echo "システム情報を収集中..."
echo "OS: $(uname -a)" > "$OUTPUT_DIR/system_info.txt"
echo "Python: $(python3 --version)" >> "$OUTPUT_DIR/system_info.txt"
echo "HIDデバイス一覧:" >> "$OUTPUT_DIR/system_info.txt"

# HIDデバイスの検出
echo "接続されているHIDデバイスを検出中..."
python3 ./kb16_raw_input.py --all-interfaces > "$OUTPUT_DIR/connected_devices.txt"

# DOIO KB16 検出情報の抜き出し
grep -A 5 "DOIO" "$OUTPUT_DIR/connected_devices.txt" > "$OUTPUT_DIR/doio_device_info.txt" 2>/dev/null
if [ $? -ne 0 ]; then
  echo "警告: DOIO KB16キーボードが検出されませんでした。デバイスが接続されていることを確認してください。"
else
  echo "DOIO KB16キーボードが検出されました。"
  cat "$OUTPUT_DIR/doio_device_info.txt"
fi

# プロトコル解析
echo
echo "プロトコル解析を開始します (30秒間)..."
echo "キーボードのキーを連続して押してください..."
python3 ./kb16_protocol_analyzer.py --duration 30 --output "$OUTPUT_DIR/kb16_protocol"

# 収集したデータの解析
echo
echo "収集したデータを解析しています..."
python3 ./kb16_analyzer_utils.py "$OUTPUT_DIR/kb16_protocol.json" > "$OUTPUT_DIR/analysis_results.txt" 2>&1

echo
echo "解析が完了しました。"
echo "結果は $OUTPUT_DIR ディレクトリに保存されました。"
echo
echo "重要なファイル:"
echo "- $OUTPUT_DIR/kb16_protocol.json - 生データ"
echo "- $OUTPUT_DIR/kb16_protocol.csv - 表形式データ"
echo "- $OUTPUT_DIR/analysis_results.txt - 解析結果"
echo
echo "EspUsbHost.cppの修正に必要なコードは analysis_results.txt の最後に記載されています。"
