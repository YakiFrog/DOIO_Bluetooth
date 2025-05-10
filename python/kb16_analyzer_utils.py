import sys
import struct
import json
import argparse
import os
from collections import defaultdict

def analyze_dump_file(filename):
    """HIDレポートダンプファイルを解析してマトリックス構造を推測する"""
    reports = []
    
    try:
        # JSONファイル形式
        if filename.endswith('.json'):
            with open(filename, 'r') as f:
                data = json.load(f)
                if 'reports' in data:
                    reports = data['reports']
                    print(f"{len(reports)}個のレポートを読み込みました")
        # CSVファイル形式
        elif filename.endswith('.csv'):
            import csv
            with open(filename, 'r') as f:
                reader = csv.reader(f)
                next(reader)  # ヘッダーをスキップ
                for row in reader:
                    if len(row) >= 3:  # Hex形式のデータを含む列がある場合
                        hex_data = row[2]
                        data_bytes = bytes([int(b, 16) for b in hex_data.split()])
                        reports.append({'data': list(data_bytes)})
            print(f"{len(reports)}個のレポートを読み込みました")
                        
    except Exception as e:
        print(f"ファイル読み込み中にエラーが発生: {e}")
        return None
    
    if not reports:
        print("解析可能なレポートがありません。")
        return None
    
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
    matrix_mapping = {}
    
    print("\n== ビットマップ解析 ==")
    print("変化頻度の高いビットはキーボードの物理ボタンに対応している可能性があります\n")
    
    for byte_idx, bits in sorted(bit_changes.items()):
        for bit, count in sorted(bits.items(), key=lambda x: x[1], reverse=True):
            bit_id = f"byte{byte_idx}_bit{bit}"
            print(f"{bit_id}: {count}回変化")
            
            if count >= 2:  # 十分な変化があるビットのみ
                key_bitmap[bit_id] = {
                    'byte': byte_idx,
                    'bit': bit,
                    'count': count
                }
    
    # マトリックス構造の推測（簡易版）
    print("\n== 推測されるキーマトリックス構造 ==")
    
    # DOIO KB16の4×4マトリックスを想定
    rows, cols = 4, 4
    
    # 最も変化の多い16個のビットをマトリックスに割り当て
    top_bits = sorted(key_bitmap.items(), key=lambda x: x[1]['count'], reverse=True)[:16]
    
    if top_bits:
        matrix = [[None for _ in range(cols)] for _ in range(rows)]
        
        for i, (bit_id, data) in enumerate(top_bits):
            row = i // cols
            col = i % cols
            
            if row < rows and col < cols:
                matrix[row][col] = bit_id
                matrix_mapping[bit_id] = (row, col)
                
        # マトリックスの表示
        print("  | 0   | 1   | 2   | 3  ")
        print("--+-----+-----+-----+-----")
        for r in range(rows):
            row_str = f"{r} |"
            for c in range(cols):
                bit_id = matrix[r][c]
                if bit_id:
                    row_str += f" {bit_id} |"
                else:
                    row_str += " --- |"
            print(row_str)
        
        # 各キーの詳細
        print("\n== 個別キー情報 ==")
        for bit_id, pos in matrix_mapping.items():
            data = key_bitmap[bit_id]
            print(f"マトリックス位置 {pos}: {bit_id} (変化回数: {data['count']})")
    else:
        print("十分なデータがなく、マトリックス構造を推測できません。")
    
    # レポートパターンの分析
    unique_patterns = {}
    for report in reports:
        data = tuple(report.get('data', []))
        unique_patterns[data] = unique_patterns.get(data, 0) + 1
    
    # 最も頻繁に現れるパターン（上位5つ）
    print("\n== 最頻出レポートパターン ==")
    for i, (pattern, count) in enumerate(
        sorted(unique_patterns.items(), key=lambda x: x[1], reverse=True)[:5]
    ):
        hex_pattern = ' '.join([f"{b:02X}" for b in pattern])
        print(f"パターン {i+1}: {hex_pattern} ({count}回, {count/len(reports)*100:.1f}%)")
    
    # キー状態の推測
    kb16_matrix = {
        (0, 0): "0,0", (0, 1): "0,1", (0, 2): "0,2", (0, 3): "0,3",
        (1, 0): "1,0", (1, 1): "1,1", (1, 2): "1,2", (1, 3): "1,3",
        (2, 0): "2,0", (2, 1): "2,1", (2, 2): "2,2", (2, 3): "2,3",
        (3, 0): "3,0", (3, 1): "3,1", (3, 2): "3,2", (3, 3): "3,3"
    }
    
    if matrix_mapping and len(matrix_mapping) >= 16:
        print("\n== KB16物理キー配置への推測マッピング ==")
        for kb16_pos, kb16_label in kb16_matrix.items():
            # 最も変化の多い順に割り当て
            if len(top_bits) > (kb16_pos[0] * cols + kb16_pos[1]):
                bit_id, data = top_bits[kb16_pos[0] * cols + kb16_pos[1]]
                print(f"KB16キー {kb16_label} -> 可能性の高いビット: {bit_id} (変化回数: {data['count']})")
    
    # EspUsbHost.cpp用コード生成
    print("\n== EspUsbHost.cpp用コード生成 ==")
    print("以下のコードをEspUsbHost.cppのキーボード処理関数に統合してみてください:")
    
    print("""
// キーボードマトリックスマッピング構造体
struct KeyMapping {
    uint8_t byte_idx;  // レポート内のバイトインデックス
    uint8_t bit_mask;  // ビットマスク (1 << bit)
    uint8_t row;       // キーボードマトリックス行
    uint8_t col;       // キーボードマトリックス列
};

// DOIO KB16キーマッピング
const KeyMapping kb16_key_map[] = {""")
    
    # マッピングデータの生成
    for bit_id, pos in matrix_mapping.items():
        data = key_bitmap[bit_id]
        byte_idx = data['byte']
        bit = data['bit']
        bit_mask = 1 << bit
        row, col = pos
        
        print(f"    {{ {byte_idx}, 0x{bit_mask:02X}, {row}, {col} }},  // {bit_id}")
    
    print("};")
    
    print("""
// KB16キーボードのレポート解析関数
void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {
    // 標準HIDキーボードレポート解析（既存のコード）
    
    // DOIO KB16カスタムキー解析
    if (device_product_id == 0x1601 && device_vendor_id == 0xD010) {
        const uint8_t* data = (const uint8_t*)&report;
        const uint8_t* last_data = (const uint8_t*)&last_report;
        bool key_state_changed = false;
        
        // 各キーマッピングをチェック
        for (int i = 0; i < sizeof(kb16_key_map) / sizeof(KeyMapping); i++) {
            const KeyMapping& mapping = kb16_key_map[i];
            
            // バイトインデックスが範囲内か確認
            if (mapping.byte_idx < sizeof(report)) {
                bool current_state = data[mapping.byte_idx] & mapping.bit_mask;
                bool last_state = last_data[mapping.byte_idx] & mapping.bit_mask;
                
                // 状態が変化した場合
                if (current_state != last_state) {
                    key_state_changed = true;
                    ESP_LOGI("KB16", "キー (%d,%d) %s", 
                             mapping.row, mapping.col, 
                             current_state ? "押下" : "解放");
                    
                    // ここでキーの状態変化に応じた処理を実装
                }
            }
        }
        
        if (key_state_changed) {
            // キー状態が変化した場合の全体的な処理
        }
    }
}""")
    
    return {
        'key_bitmap': key_bitmap,
        'matrix_mapping': matrix_mapping
    }
    
def main():
    parser = argparse.ArgumentParser(description='DOIO KB16ダンプファイル解析ツール')
    parser.add_argument('file', help='解析するJSONまたはCSVファイル')
    args = parser.parse_args()
    
    if not os.path.exists(args.file):
        print(f"ファイル '{args.file}' が見つかりません")
        return
    
    print(f"ファイル '{args.file}' を解析中...")
    analyze_dump_file(args.file)

if __name__ == "__main__":
    main()
