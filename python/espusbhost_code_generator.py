#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/espusbhost_code_generator.py
# ESP32 USB Host コード生成ツール
# DOIO KB16キーボード用のEspUsbHost.cpp用コードを生成

import json
import argparse
import os
import sys
from collections import defaultdict

# EspUsbHost.cpp用のコードテンプレート
CODE_TEMPLATE = """
// DOIO KB16 キーボード用マッピング - 自動生成コード
// 生成日時: {datetime}
// 解析ファイル: {source_file}

// キーボードマトリックスマッピング構造体
struct KeyMapping {{
    uint8_t byte_idx;  // レポート内のバイトインデックス
    uint8_t bit_mask;  // ビットマスク (1 << bit)
    uint8_t row;       // キーボードマトリックス行
    uint8_t col;       // キーボードマトリックス列
}};

// DOIO KB16キーマッピング
const KeyMapping kb16_key_map[] = {{
{mapping_entries}
}};

// EspUsbHost.cppのonKeyboard関数に挿入するコード例
void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {{
    // まず通常のHIDキーボードレポート解析を行う
    // （既存のコードはそのまま残す）
    // ...
    
    // DOIO KB16カスタムキー解析
    if (device_product_id == 0x1601 && device_vendor_id == 0xD010) {{
        const uint8_t* data = (const uint8_t*)&report;
        const uint8_t* last_data = (const uint8_t*)&last_report;
        bool key_state_changed = false;
        
        // 各キーマッピングをチェック
        for (int i = 0; i < sizeof(kb16_key_map) / sizeof(KeyMapping); i++) {{
            const KeyMapping& mapping = kb16_key_map[i];
            
            // バイトインデックスが範囲内か確認
            if (mapping.byte_idx < sizeof(report)) {{
                bool current_state = data[mapping.byte_idx] & mapping.bit_mask;
                bool last_state = last_data[mapping.byte_idx] & mapping.bit_mask;
                
                // 状態が変化した場合
                if (current_state != last_state) {{
                    key_state_changed = true;
                    ESP_LOGI("KB16", "キー (%d,%d) %s", 
                             mapping.row, mapping.col, 
                             current_state ? "押下" : "解放");
                    
                    // キーイベントの発行例
                    // keyboard_manager.processMatrixKey(mapping.row, mapping.col, current_state);
                }}
            }}
        }}
        
        if (key_state_changed) {{
            // キー状態が変化した場合の全体的な処理
            // 例: LEDの更新など
        }}
    }}
}}

// 参考：EspUsbHost.cppの変更推奨箇所
/*
1. ヘッダファイルに追加:
#include "keyboard_manager.h"  // 必要に応じて

2. onKeyboard関数の末尾に上記のコードを追加

3. ファイル終了付近にある既存のonKeyboard関数を修正:
void EspUsbHost::onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {{
    // 既存のコードをそのまま維持
    // ...

    // DOIO KB16 カスタムキーの処理を追加
    if (device_product_id == 0x1601 && device_vendor_id == 0xD010) {{
        const uint8_t* data = (const uint8_t*)&report;
        const uint8_t* last_data = (const uint8_t*)&last_report;
        
        // KB16キーマッピングに基づいた処理
        // (上記コードをコピー)
    }}
}}
*/
"""

def process_capture_file(filename):
    """キャプチャJSONファイルからキーマトリックス構造を解析"""
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
        
        if 'reports' not in data or not data['reports']:
            print("エラー: レポートデータが見つかりません")
            return None
        
        reports = data['reports']
        print(f"{len(reports)}個のレポートを読み込みました")
        
        # レポート間の差分を分析してビットマップを作成
        bit_changes = defaultdict(lambda: defaultdict(int))
        previous_report = None
        
        for report in reports:
            if previous_report:
                data1 = previous_report.get('data', [])
                data2 = report.get('data', [])
                
                # 同じ長さのデータのみ比較
                min_len = min(len(data1), len(data2))
                
                for i in range(min_len):
                    if data1[i] != data2[i]:
                        changed_bits = data1[i] ^ data2[i]
                        for bit in range(8):
                            if changed_bits & (1 << bit):
                                bit_changes[i][bit] += 1
            
            previous_report = report
        
        # 変化したビットの解析
        key_bitmap = {}
        
        for byte_idx, bits in sorted(bit_changes.items()):
            for bit, count in sorted(bits.items(), key=lambda x: x[1], reverse=True):
                bit_id = f"byte{byte_idx}_bit{bit}"
                
                if count >= 2:  # 十分な変化があるビットのみ
                    key_bitmap[bit_id] = {
                        'byte': byte_idx,
                        'bit': bit,
                        'count': count
                    }
        
        # KB16の4×4マトリックスを想定
        rows, cols = 4, 4
        top_bits = sorted(key_bitmap.items(), key=lambda x: x[1]['count'], reverse=True)[:16]
        
        # マトリックスマッピングの作成
        matrix_mapping = {}
        if top_bits:
            for i, (bit_id, data) in enumerate(top_bits):
                row = i // cols
                col = i % cols
                
                if row < rows and col < cols:
                    matrix_mapping[bit_id] = (row, col)
        
        return {
            'key_bitmap': key_bitmap,
            'matrix_mapping': matrix_mapping,
            'top_bits': top_bits
        }
        
    except Exception as e:
        print(f"ファイル処理中にエラーが発生: {e}")
        return None

def generate_code(analysis_result, source_file):
    """解析結果からコードを生成"""
    if not analysis_result:
        return "// 解析エラー：データが利用できません"
    
    import datetime
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    # マッピングエントリの生成
    mapping_entries = []
    for bit_id, pos in analysis_result['matrix_mapping'].items():
        data = analysis_result['key_bitmap'][bit_id]
        byte_idx = data['byte']
        bit = data['bit']
        bit_mask = 1 << bit
        row, col = pos
        
        entry = f"    {{ {byte_idx}, 0x{bit_mask:02X}, {row}, {col} }},  // {bit_id}, 変化回数: {data['count']}"
        mapping_entries.append(entry)
    
    # テンプレートに挿入
    code = CODE_TEMPLATE.format(
        datetime=now,
        source_file=os.path.basename(source_file),
        mapping_entries="\n".join(mapping_entries)
    )
    
    return code

def main():
    parser = argparse.ArgumentParser(description='ESP32 USB Host コード生成ツール')
    parser.add_argument('file', help='解析するJSONキャプチャファイル')
    parser.add_argument('-o', '--output', help='出力ファイル名', default=None)
    args = parser.parse_args()
    
    if not os.path.exists(args.file):
        print(f"ファイル '{args.file}' が見つかりません")
        return 1
    
    print(f"ファイル '{args.file}' を解析中...")
    analysis_result = process_capture_file(args.file)
    
    if not analysis_result:
        print("解析に失敗しました。")
        return 1
    
    code = generate_code(analysis_result, args.file)
    
    # コードの出力
    if args.output:
        with open(args.output, 'w') as f:
            f.write(code)
        print(f"コードを '{args.output}' に保存しました")
    else:
        print("\n" + "=" * 80)
        print("生成したコード:")
        print("=" * 80)
        print(code)
    
    print("\n以下の手順でEspUsbHost.cppを修正してください:")
    print("1. 上記のコードをEspUsbHost.cppの適切な場所にコピーします")
    print("2. onKeyboard関数を修正して、DOIO KB16を特別に処理するコードを追加します")
    print("3. 必要に応じてkeyboard_manager.hを修正し、マトリックスキーイベントを処理します")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
