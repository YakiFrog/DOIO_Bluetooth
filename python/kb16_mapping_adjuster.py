#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DOIO KB16 キーマッピング調整ツール

既存のデータや手動入力に基づいて、KB16のキーマッピングを修正するためのツール。
"""

import os
import sys
import json
import argparse
from datetime import datetime

class KB16MappingAdjuster:
    def __init__(self):
        self.timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        # KB16物理キー配列（4x4マトリックス）
        self.kb16_physical_layout = [
            ["1", "2", "3", "4"],
            ["5", "6", "7", "8"],
            ["9", "0", "Enter", "Esc"],
            ["Backspace", "Tab", "Space", "Alt"]
        ]
        
        # HIDキーコード定義
        self.hid_key_codes = {
            "1": 0x1E,
            "2": 0x1F, 
            "3": 0x20,
            "4": 0x21,
            "5": 0x22,
            "6": 0x23,
            "7": 0x24,
            "8": 0x25,
            "9": 0x26,
            "0": 0x27,
            "Enter": 0x28,
            "Esc": 0x29,
            "Backspace": 0x2A,
            "Tab": 0x2B,
            "Space": 0x2C,
            "Alt": 0xE6
        }
        
        # 現在のマッピング
        self.current_mapping = []
        
    def display_physical_layout(self):
        """現在の物理的なキー配置を表示"""
        print("\n=== KB16 物理キー配置 ===")
        for row_idx, row in enumerate(self.kb16_physical_layout):
            print(f"行 {row_idx}: {' | '.join(row)}")
        print()
        
    def load_existing_mapping(self, file_path=None):
        """既存のマッピングをロード"""
        if file_path and os.path.exists(file_path):
            try:
                with open(file_path, 'r') as f:
                    data = json.load(f)
                self.current_mapping = data.get("key_mappings", [])
                print(f"マッピングを読み込みました: {file_path}")
                return True
            except Exception as e:
                print(f"マッピングの読み込みエラー: {e}")
        
        # 現在のコードからマッピングを抽出
        print("現在のコードからマッピングを抽出します...")
        
        # サンプルデータ（実際には自動抽出されるべき）
        self.current_mapping = [
            {"byte_idx": 5, "bit_mask": "0x20", "row": 0, "col": 0, "key_name": "1"},
            {"byte_idx": 1, "bit_mask": "0x01", "row": 0, "col": 1, "key_name": "2"},
            {"byte_idx": 1, "bit_mask": "0x02", "row": 0, "col": 2, "key_name": "3"},
            {"byte_idx": 5, "bit_mask": "0x01", "row": 0, "col": 3, "key_name": "4"},
            {"byte_idx": 4, "bit_mask": "0x01", "row": 1, "col": 0, "key_name": "5"},
            {"byte_idx": 5, "bit_mask": "0x02", "row": 1, "col": 1, "key_name": "6"},
            {"byte_idx": 4, "bit_mask": "0x08", "row": 1, "col": 2, "key_name": "7"},
            {"byte_idx": 4, "bit_mask": "0x80", "row": 1, "col": 3, "key_name": "8"},
            {"byte_idx": 4, "bit_mask": "0x02", "row": 2, "col": 0, "key_name": "9"},
            {"byte_idx": 4, "bit_mask": "0x20", "row": 2, "col": 1, "key_name": "0"},
            {"byte_idx": 5, "bit_mask": "0x08", "row": 2, "col": 2, "key_name": "Enter"},
            {"byte_idx": 4, "bit_mask": "0x40", "row": 2, "col": 3, "key_name": "Esc"},
            {"byte_idx": 4, "bit_mask": "0x10", "row": 3, "col": 0, "key_name": "Backspace"},
            {"byte_idx": 5, "bit_mask": "0x10", "row": 3, "col": 1, "key_name": "Tab"},
            {"byte_idx": 4, "bit_mask": "0x04", "row": 3, "col": 2, "key_name": "Space"},
            {"byte_idx": 5, "bit_mask": "0x04", "row": 3, "col": 3, "key_name": "Alt"}
        ]
        
        # HIDキーコードを追加
        for mapping in self.current_mapping:
            key_name = mapping.get("key_name")
            if key_name in self.hid_key_codes:
                mapping["hid_keycode"] = f"0x{self.hid_key_codes[key_name]:02X}"
            
        return True
        
    def display_current_mapping(self):
        """現在のマッピングを表示"""
        if not self.current_mapping:
            print("マッピングが読み込まれていません")
            return
            
        print("\n=== 現在のキーマッピング ===")
        # マトリックス形式で表示
        matrix = [[None for _ in range(4)] for _ in range(4)]
        
        for mapping in self.current_mapping:
            row = mapping.get("row", 0)
            col = mapping.get("col", 0)
            key_name = mapping.get("key_name", "?")
            byte_idx = mapping.get("byte_idx", 0)
            bit_mask = mapping.get("bit_mask", "0x00")
            
            if 0 <= row < 4 and 0 <= col < 4:
                matrix[row][col] = f"{key_name} [{byte_idx}:{bit_mask}]"
        
        for row_idx, row in enumerate(matrix):
            print(f"行 {row_idx}: {' | '.join(item or '???' for item in row)}")
        print()
        
    def adjust_mapping(self):
        """マッピングを調整"""
        self.display_physical_layout()
        self.display_current_mapping()
        
        print("\n=== マッピング調整 ===")
        print("調整するキーの行と列を入力してください。何も入力せずEnterで終了します。")
        
        while True:
            try:
                pos = input("\n行,列 (例: 2,3): ").strip()
                if not pos:
                    break
                    
                row, col = map(int, pos.split(','))
                if not (0 <= row < 4 and 0 <= col < 4):
                    print("行と列は0-3の範囲で指定してください")
                    continue
                
                # 現在のキーを表示
                key_name = self.kb16_physical_layout[row][col]
                print(f"調整するキー: {key_name} (行{row}, 列{col})")
                
                # 現在のマッピングから該当キーを検索
                current = next((m for m in self.current_mapping if m["row"] == row and m["col"] == col), None)
                
                if current:
                    print(f"現在の設定: バイト{current['byte_idx']}, ビットマスク{current['bit_mask']}")
                else:
                    print("このキーには現在マッピングがありません")
                
                # 新しい設定を入力
                new_byte = input(f"バイト番号 (0-7) [{current['byte_idx'] if current else '?'}]: ").strip()
                if new_byte:
                    new_byte = int(new_byte)
                else:
                    new_byte = current['byte_idx'] if current else 0
                    
                new_bit = input(f"ビット番号 (0-7) [現在のマスク: {current['bit_mask'] if current else '?'}]: ").strip()
                if new_bit:
                    new_bit = int(new_bit)
                    new_mask = f"0x{1 << new_bit:02X}"
                else:
                    new_mask = current['bit_mask'] if current else "0x01"
                
                # マッピングを更新
                if current:
                    current["byte_idx"] = new_byte
                    current["bit_mask"] = new_mask
                else:
                    # 新しいマッピングを追加
                    new_mapping = {
                        "byte_idx": new_byte,
                        "bit_mask": new_mask,
                        "row": row,
                        "col": col,
                        "key_name": key_name,
                        "hid_keycode": f"0x{self.hid_key_codes.get(key_name, 0):02X}"
                    }
                    self.current_mapping.append(new_mapping)
                
                print(f"キー {key_name} のマッピングを更新: バイト{new_byte}, ビットマスク{new_mask}")
                
            except ValueError as e:
                print(f"入力エラー: {e}")
            except KeyboardInterrupt:
                break
    
    def generate_code(self):
        """C++コードを生成"""
        if not self.current_mapping:
            print("マッピングが読み込まれていません")
            return
            
        # マッピング順に並び替え
        sorted_mapping = sorted(self.current_mapping, key=lambda m: (m['row'], m['col']))
        
        # C++コード生成
        cpp_code = "// DOIO KB16 キーマッピング - 自動生成 {}\n".format(self.timestamp)
        cpp_code += "const KeyMapping kb16_key_map[] = {\n"
        
        for mapping in sorted_mapping:
            cpp_code += f"    {{ {mapping['byte_idx']}, {mapping['bit_mask']}, {mapping['row']}, {mapping['col']} }},  // {mapping['key_name']}\n"
            
        cpp_code += "};\n\n"
        
        # HIDキーコードマッピングコード（必要に応じて）
        cpp_code += "// 行列からHIDコードへのマッピング関数\n"
        cpp_code += "uint8_t getHidKeyCode(uint8_t row, uint8_t col) {\n"
        cpp_code += "    if (row >= 4 || col >= 4) return 0;\n\n"
        cpp_code += "    // 行/列の組み合わせからキーコードに変換\n"
        cpp_code += "    switch (row * 4 + col) {\n"
        
        for mapping in sorted_mapping:
            index = mapping['row'] * 4 + mapping['col']
            cpp_code += f"        case {index}: return {mapping.get('hid_keycode', '0x00')};  // {mapping['key_name']}\n"
            
        cpp_code += "        default: return 0;\n"
        cpp_code += "    }\n"
        cpp_code += "}\n"
        
        # コードを表示・保存
        print("\n=== 生成されたコード ===\n")
        print(cpp_code)
        
        filename = f"kb16_mapping_code_{self.timestamp}.cpp"
        with open(filename, 'w') as f:
            f.write(cpp_code)
            
        print(f"\nコードを保存しました: {filename}")
        
        # EspUsbHost.cpp内でのコード更新手順
        print("\n=== EspUsbHost.cppの更新手順 ===")
        print("1. EspUsbHost.cpp内のkb16_key_map配列を上記の新しいマッピングに置き換えます")
        print("2. onKeyboard関数内のキー判定部分も必要に応じて更新してください")
        
    def save_mapping(self):
        """マッピングをJSONとして保存"""
        data = {
            "timestamp": self.timestamp,
            "physical_layout": self.kb16_physical_layout,
            "key_mappings": self.current_mapping
        }
        
        filename = f"kb16_mapping_data_{self.timestamp}.json"
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            
        print(f"\nマッピングデータを保存しました: {filename}")

def main():
    parser = argparse.ArgumentParser(description="DOIO KB16 キーマッピング調整ツール")
    parser.add_argument("-f", "--file", help="既存のマッピングJSONファイル")
    args = parser.parse_args()
    
    adjuster = KB16MappingAdjuster()
    adjuster.load_existing_mapping(args.file)
    
    while True:
        print("\n=== KB16 マッピング調整ツール ===")
        print("1: 現在のマッピングを表示")
        print("2: マッピングを調整")
        print("3: C++コードを生成")
        print("4: マッピングをJSONとして保存")
        print("0: 終了")
        
        choice = input("\n選択: ").strip()
        
        if choice == "1":
            adjuster.display_current_mapping()
        elif choice == "2":
            adjuster.adjust_mapping()
        elif choice == "3":
            adjuster.generate_code()
        elif choice == "4":
            adjuster.save_mapping()
        elif choice == "0":
            break
        else:
            print("無効な選択です")

if __name__ == "__main__":
    main()
