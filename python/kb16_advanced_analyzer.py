#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_advanced_analyzer.py
# DOIO KB16キーボード詳細アナライザー
# 内部データ構造とHIDレポートの解析に特化したツール

import hid
import time
import sys
import argparse
import binascii
import json
import os
import struct
import datetime
import csv
from collections import defaultdict, deque

# キーコードからASCIIへの変換テーブル (日本語キーボード用)
def keycode_to_ascii(keycode, shift=False):
    keycode_table = {
        0x04: ('a', 'A'), 0x05: ('b', 'B'), 0x06: ('c', 'C'), 0x07: ('d', 'D'),
        0x08: ('e', 'E'), 0x09: ('f', 'F'), 0x0A: ('g', 'G'), 0x0B: ('h', 'H'),
        0x0C: ('i', 'I'), 0x0D: ('j', 'J'), 0x0E: ('k', 'K'), 0x0F: ('l', 'L'),
        0x10: ('m', 'M'), 0x11: ('n', 'N'), 0x12: ('o', 'O'), 0x13: ('p', 'P'),
        0x14: ('q', 'Q'), 0x15: ('r', 'R'), 0x16: ('s', 'S'), 0x17: ('t', 'T'),
        0x18: ('u', 'U'), 0x19: ('v', 'V'), 0x1A: ('w', 'W'), 0x1B: ('x', 'X'),
        0x1C: ('y', 'Y'), 0x1D: ('z', 'Z'), 0x1E: ('1', '!'), 0x1F: ('2', '"'),
        0x20: ('3', '#'), 0x21: ('4', '$'), 0x22: ('5', '%'), 0x23: ('6', '&'),
        0x24: ('7', "'"), 0x25: ('8', '('), 0x26: ('9', ')'), 0x27: ('0', ''),
        0x28: ('\r', '\r'), 0x29: ('\x1b', '\x1b'), 0x2A: ('\b', '\b'), 0x2B: ('\t', '\t'),
        0x2C: (' ', ' '), 0x2D: ('-', '='), 0x2E: ('^', '~'), 0x2F: ('@', '`'),
        0x30: ('[', '{'), 0x31: (None, None), 0x32: (']', '}'), 0x33: (';', '+'),
        0x34: (':', '*'), 0x36: (',', '<'), 0x37: ('.', '>'), 0x38: ('/', '?'),
        0x54: ('/', '/'), 0x55: ('*', '*'), 0x56: ('-', '-'), 0x57: ('+', '+'),
        0x58: ('\r', '\r'), 0x59: ('1', None), 0x5A: ('2', None), 0x5B: ('3', None),
        0x5C: ('4', None), 0x5D: ('5', '5'), 0x5E: ('6', None), 0x5F: ('7', None),
        0x60: ('8', None), 0x61: ('9', None), 0x62: ('0', None), 0x63: ('.', None),
        0x67: ('=', '=')
    }
    
    if keycode in keycode_table:
        return keycode_table[keycode][1 if shift else 0]
    return None

# HID Usage names
HID_USAGE_NAMES = {
    0x01: "Generic Desktop Controls",
    0x02: "Simulation Controls",
    0x03: "VR Controls",
    0x04: "Sport Controls",
    0x05: "Game Controls",
    0x06: "Generic Device Controls",
    0x07: "Keyboard/Keypad",
    0x08: "LED",
    0x09: "Button",
    0x0A: "Ordinal",
    0x0B: "Telephony Device",
    0x0C: "Consumer",
    0x0D: "Digitizer",
    0x14: "Auxiliary Display",
    0x40: "Medical Instrument"
}

# キーボード出力位置をマトリックス形式で割り当て (KB16レイアウトに基づいて)
KB16_MATRIX_LAYOUT = {
    # 行0 (上段)
    (0, 0): (0, 0),  # 左上のキー
    (0, 1): (0, 1),
    (0, 2): (0, 2),
    (0, 3): (0, 3),
    (0, 4): "e0",    # 特殊キー 1
    # 行1
    (1, 0): (1, 0),
    (1, 1): (1, 1),
    (1, 2): (1, 2),
    (1, 3): (1, 3),
    (1, 4): "e1",    # 特殊キー 2
    # 行2
    (2, 0): (2, 0),
    (2, 1): (2, 1),
    (2, 2): (2, 2),
    (2, 3): (2, 3),
    (2, 4): "e2",    # 特殊キー 3 (2倍サイズ)
    # 行3 (下段)
    (3, 0): (3, 0),
    (3, 1): (3, 1),
    (3, 2): (3, 2),
    (3, 3): (3, 3),
    # e2キーは行2と行3の2つのセルにまたがっている
}

class ReportStructureAnalyzer:
    """HIDレポート構造を解析するクラス"""
    
    def __init__(self):
        self.patterns = defaultdict(int)
        self.key_states = {}  # キーの状態を追跡
        self.report_history = deque(maxlen=100)  # 最近のレポート履歴
        self.bitmap_analysis = {}  # ビットマップ解析結果
        self.matrix_analysis = {}  # マトリックス解析結果
        self.byte_changes = defaultdict(lambda: defaultdict(int))  # バイト変化追跡
        self.last_report = None
    
    def analyze_report(self, report):
        """受信したHIDレポートを解析"""
        # レポートパターンの追跡
        report_pattern = tuple(report)
        self.patterns[report_pattern] += 1
        
        # 履歴に追加
        timestamp = datetime.datetime.now()
        self.report_history.append((timestamp, list(report)))
        
        # 前回のレポートとの差分を分析
        if self.last_report:
            self._analyze_changes(self.last_report, report)
        
        self.last_report = list(report)
        
        # キー状態の解析 (標準HIDキーボードレポート形式を想定)
        if len(report) >= 8:
            self._analyze_key_states(report)
    
    def _analyze_key_states(self, report):
        """HIDキーボードレポートのキー状態解析"""
        modifier = report[0]
        keycodes = report[2:8]
        
        # キーコードをセットに変換して比較
        active_keys = set(keycode for keycode in keycodes if keycode != 0)
        
        # 前回との差分を計算
        old_active_keys = set(self.key_states.get("active_keys", []))
        pressed_keys = active_keys - old_active_keys
        released_keys = old_active_keys - active_keys
        
        # キー状態の更新
        self.key_states["modifier"] = modifier
        self.key_states["active_keys"] = active_keys
        
        # 新しく押されたキー/離されたキーの記録
        if pressed_keys:
            self.key_states["pressed"] = pressed_keys
        else:
            self.key_states.pop("pressed", None)
            
        if released_keys:
            self.key_states["released"] = released_keys
        else:
            self.key_states.pop("released", None)
    
    def _analyze_changes(self, last_report, current_report):
        """レポート間の変化を分析"""
        for i, (last_byte, current_byte) in enumerate(zip(last_report, current_report)):
            if last_byte != current_byte:
                # どのビットが変化したか記録
                changed_bits = last_byte ^ current_byte
                for bit_pos in range(8):
                    if changed_bits & (1 << bit_pos):
                        self.byte_changes[i][bit_pos] += 1
    
    def analyze_bitmap_structure(self):
        """レポートのビットマップ構造を解析"""
        # 変化したビットの頻度からキーマトリックスの推測を試みる
        for byte_pos, bit_changes in self.byte_changes.items():
            if bit_changes:
                self.bitmap_analysis[byte_pos] = dict(bit_changes)
    
    def get_matrix_analysis(self):
        """キーマトリックスを解析し、結果を返す"""
        # 4行5列のマトリックスを想定
        # レポートの特定バイト/ビットがどのキーに対応するかの推測を返す
        
        analysis_result = {}
        # ビットマップ解析の結果を使用してマトリックスにマッピング
        
        for byte_idx, bit_data in self.bitmap_analysis.items():
            for bit_pos, change_count in bit_data.items():
                # 変化頻度が高いビットが重要なキーに対応している可能性が高い
                if change_count > 2:  # 一定以上の変化があったもののみ考慮
                    key_id = f"byte{byte_idx}_bit{bit_pos}"
                    # マトリックスポジションを推測 (ここでは単純な例)
                    row = (byte_idx * 8 + bit_pos) // 5  # 5は列数
                    col = (byte_idx * 8 + bit_pos) % 5
                    
                    if row < 4 and col < 5:  # 4行5列の範囲内
                        matrix_pos = KB16_MATRIX_LAYOUT.get((row, col), "unknown")
                        analysis_result[key_id] = {
                            "position": f"({row},{col})",
                            "matrix_label": matrix_pos,
                            "change_count": change_count
                        }
        
        return analysis_result

    def get_report_statistics(self):
        """レポートの統計情報を取得"""
        stats = {
            "total_reports": sum(self.patterns.values()),
            "unique_patterns": len(self.patterns),
            "top_patterns": sorted(self.patterns.items(), key=lambda x: x[1], reverse=True)[:5],
            "byte_changes": dict(self.byte_changes)
        }
        return stats
    
    def save_to_file(self, filename_prefix):
        """解析結果をファイルに保存"""
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # JSONで解析結果を保存
        json_data = {
            "timestamp": timestamp,
            "statistics": self.get_report_statistics(),
            "matrix_analysis": self.get_matrix_analysis(),
            "bitmap_analysis": self.bitmap_analysis,
        }
        
        json_filename = f"{filename_prefix}_analysis_{timestamp}.json"
        with open(json_filename, "w") as f:
            json.dump(json_data, f, indent=2)
            
        # CSVで履歴データを保存
        csv_filename = f"{filename_prefix}_history_{timestamp}.csv"
        with open(csv_filename, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp", "Report Data (HEX)"])
            for timestamp, report in self.report_history:
                hex_data = " ".join([f"{byte:02X}" for byte in report])
                writer.writerow([timestamp.strftime("%H:%M:%S.%f"), hex_data])
        
        return json_filename, csv_filename

def load_keyboard_layout(json_file):
    """JSONファイルからキーボードレイアウト情報を読み込む"""
    try:
        with open(json_file, 'r') as f:
            keyboard_data = json.load(f)
        return keyboard_data
    except Exception as e:
        print(f"キーボードレイアウトファイルの読み込みに失敗しました: {str(e)}")
        return None

def visualize_key_press(keyboard_data, active_keys, shift=False):
    """キーボードレイアウトを可視化し、押されたキーを表示"""
    if not keyboard_data:
        return "キーボードレイアウト情報がありません"
    
    # キー配列情報を取得
    rows = keyboard_data.get('matrix', {}).get('rows', 4)
    cols = keyboard_data.get('matrix', {}).get('cols', 5)
    
    # シンプルなテキストベースの可視化
    layout = []
    for r in range(rows):
        row = []
        for c in range(cols):
            key_pos = f"{r},{c}"
            # キーコードからASCII文字に変換
            key_char = " "
            for keycode in active_keys:
                # 実際のキーマップに基づいて変換処理を追加
                # 現在はプレースホルダ
                pass
            row.append(key_char)
        layout.append(row)
    
    # 可視化したレイアウトを文字列として返す
    result = "\n"
    for row in layout:
        result += "[" + "][".join(row) + "]\n"
    
    return result

def send_custom_report(h, report_type, data):
    """カスタムHIDレポートを送信する実験的機能"""
    try:
        # 送信用データの準備
        report_data = bytes([report_type]) + bytes(data)
        
        # Feature Report送信の試み
        result = h.send_feature_report(report_data)
        print(f"カスタムレポート送信結果: {result} バイト送信")
        return True
    except Exception as e:
        print(f"カスタムレポート送信失敗: {str(e)}")
        return False

def capture_and_analyze_report(data, analyzer, keyboard_data=None):
    """HIDレポートをキャプチャして解析"""
    # レポートデータを解析器に渡す
    analyzer.analyze_report(data)
    
    # 2回に1回の頻度でビットマップ構造を解析 (処理負荷軽減)
    if sum(analyzer.patterns.values()) % 2 == 0:
        analyzer.analyze_bitmap_structure()

def analyze_report_descriptor(descriptor_data):
    """HIDレポートディスクリプタを解析"""
    if not descriptor_data:
        return "レポートディスクリプタなし"
    
    results = []
    i = 0
    
    while i < len(descriptor_data):
        prefix = descriptor_data[i]
        
        # HIDディスクリプタの構造解析
        item_size = prefix & 0x03
        item_type = (prefix & 0x0C) >> 2
        item_tag = (prefix & 0xF0) >> 4
        
        item_name = "Unknown"
        if item_type == 0:  # Main item
            main_tags = ["Input", "Output", "Feature", "Collection", "End Collection"]
            if item_tag < len(main_tags):
                item_name = main_tags[item_tag]
        elif item_type == 1:  # Global item
            global_tags = ["Usage Page", "Logical Min", "Logical Max", "Physical Min", 
                          "Physical Max", "Unit Exponent", "Unit", "Report Size", 
                          "Report ID", "Report Count"]
            if item_tag < len(global_tags):
                item_name = global_tags[item_tag]
        elif item_type == 2:  # Local item
            local_tags = ["Usage", "Usage Min", "Usage Max"]
            if item_tag < len(local_tags):
                item_name = local_tags[item_tag]
        
        value = 0
        i += 1  # Move past prefix
        
        # データバイトの読み取り
        if item_size > 0 and i < len(descriptor_data):
            bytes_data = descriptor_data[i:i+item_size]
            # リトルエンディアンで値を解釈
            for j, byte in enumerate(bytes_data):
                value |= byte << (j * 8)
            i += item_size
        
        # 特殊な処理（Usage Pageなど）
        if item_name == "Usage Page" and value in HID_USAGE_NAMES:
            value_str = f"0x{value:04X} ({HID_USAGE_NAMES[value]})"
        else:
            value_str = f"0x{value:X}" if value > 9 else str(value)
        
        results.append(f"{item_name}: {value_str}")
    
    return "\n".join(results)

def main():
    # コマンドライン引数の処理
    parser = argparse.ArgumentParser(description='DOIO KB16 高度アナライザー')
    parser.add_argument('--vid', help='ベンダーIDを16進数で指定 (例: 0xD010)', default=None)
    parser.add_argument('--pid', help='プロダクトIDを16進数で指定 (例: 0x1601)', default=None)
    parser.add_argument('--interface', type=int, help='インターフェース番号を指定 (例: 2)', default=None)
    parser.add_argument('--raw', action='store_true', help='生のバイトデータをダンプ')
    parser.add_argument('--layout', help='キーボードレイアウトJSONファイルのパス', default=None)
    parser.add_argument('--log', help='ログファイル出力を有効化', action='store_true')
    parser.add_argument('--deep-analyze', help='詳細な解析モード', action='store_true')
    parser.add_argument('--monitor-time', type=int, help='モニタリング時間（秒）。指定すると自動的に終了', default=0)
    
    args = parser.parse_args()
    
    # レポート解析エンジンの初期化
    analyzer = ReportStructureAnalyzer()
    
    # キーボードレイアウトの読み込み
    keyboard_data = None
    if args.layout:
        keyboard_data = load_keyboard_layout(args.layout)
    elif os.path.exists("kb16-01.json"):
        keyboard_data = load_keyboard_layout("kb16-01.json")
    
    # コマンドライン引数からVID/PIDが指定された場合は変換
    target_vid = int(args.vid, 16) if args.vid else None
    target_pid = int(args.pid, 16) if args.pid else None
    target_interface = args.interface
    
    print("DOIO KB16 高度アナライザー v1.0を開始します")
    print("内部データ構造とHIDレポートの詳細解析に特化したツール")
    print("接続されているHIDデバイスを検索中...")
    
    # システム上のすべてのHIDデバイスを列挙
    all_devices = hid.enumerate()
    
    # 検出されたデバイス数をカウント
    device_count = 0
    target_device = None
    
    # 詳細なデバイス情報の収集
    device_details = []
    
    for device in all_devices:
        vendor_id = device['vendor_id']
        product_id = device['product_id']
        
        # VID/PIDとManufacturerとProduct名を表示
        manufacturer = device.get('manufacturer_string', 'Unknown')
        product = device.get('product_string', 'Unknown')
        
        # 詳細情報を収集
        device_info = {
            'index': device_count + 1,
            'vendor_id': vendor_id,
            'product_id': product_id,
            'manufacturer': manufacturer,
            'product': product,
            'interface': device.get('interface_number', -1),
            'path': device.get('path', b'').hex(),
            'usage_page': device.get('usage_page', 0),
            'usage': device.get('usage', 0)
        }
        device_details.append(device_info)
        
        # 接続情報の表示（すべてのデバイスについて）
        device_count += 1
        print(f"検出 #{device_count}: VID=0x{vendor_id:04X}, PID=0x{product_id:04X}, メーカー: {manufacturer}, 製品: {product}")
        
        # インターフェース情報を表示
        interface_number = device.get('interface_number', -1)
        if interface_number >= 0:
            print(f"  Interface: {interface_number}")
            
        # その他のパス情報
        path = device.get('path', b'').hex()
        if path:
            print(f"  Path: {path}")
            
        usage_page = device.get('usage_page', 0)
        usage = device.get('usage', 0)
        if usage_page != 0 or usage != 0:
            print(f"  Usage Page: 0x{usage_page:04X}, Usage: 0x{usage:04X}")
        
        # 特定のデバイスを検出
        if target_vid and target_pid:
            # コマンドラインで指定されたVID/PIDに一致するか
            if vendor_id == target_vid and product_id == target_pid:
                # インターフェースが指定されていればそれも確認
                if target_interface is None or interface_number == target_interface:
                    print(f"指定されたVID/PIDのデバイスを検出しました: 0x{vendor_id:04X}/0x{product_id:04X}")
                    target_device = device
                    break
        else:
            # 既知のDOIO KB16デバイスを検出
            if (vendor_id == 0x1EAF and product_id == 0x0003) or \
            (vendor_id == 0xD010 and product_id == 0x1601):
                print(f"DOIO KB16キーボードを検出しました! VID=0x{vendor_id:04X}, PID=0x{product_id:04X}")
                target_device = device
                break
    
    if target_device is None:
        print("\nDOIO KB16キーボードが見つかりませんでした。")
        print("以下のいずれかの方法を試してください:")
        print("1. キーボードが正しく接続されているか確認する")
        print("2. 手動でVID/PIDを指定する（例: --vid 0xD010 --pid 0x1601）")
        print("3. 他の利用可能なデバイスを手動で選択する")
        
        # 手動でデバイスを選択可能にする
        if device_count > 0 and input("\nリスト内のデバイスを選択しますか? (y/n): ").lower() == 'y':
            try:
                selection = int(input(f"デバイス番号を入力してください (1-{device_count}): "))
                if 1 <= selection <= device_count:
                    target_device = all_devices[selection-1]
                else:
                    print("無効な選択です。プログラムを終了します。")
                    return
            except ValueError:
                print("無効な入力です。プログラムを終了します。")
                return
        else:
            return
    
    # JSONでデバイス情報をエクスポート
    with open("detected_devices.json", "w") as f:
        json.dump(device_details, f, indent=2)
    print(f"検出したすべてのデバイス情報を detected_devices.json に保存しました")
    
    # デバイス情報を表示して操作を開始
    try:
        print(f"\nデバイスを開いています: VID=0x{target_device['vendor_id']:04X}, PID=0x{target_device['product_id']:04X}")
        
        # インターフェース番号の取得
        interface_number = target_device.get('interface_number', None)
        if interface_number is not None:
            print(f"インターフェース番号: {interface_number}")
        
        # HIDデバイスを開く
        h = hid.device()
        
        # パスを使用して開く（より確実）
        if 'path' in target_device:
            try:
                # まずパスで開いてみる
                h.open_path(target_device['path'])
                print("パスを使用してデバイスを開きました")
            except Exception as e:
                print(f"パスでの接続に失敗しました: {str(e)}")
                print("VID/PIDで接続を試みます")
                h.open(target_device['vendor_id'], target_device['product_id'])
        else:
            # 通常のVID/PIDで開く
            h.open(target_device['vendor_id'], target_device['product_id'])
            
        print("接続成功!")
        
        # デバイス情報表示
        try:
            print("\nデバイス情報:")
            manufacturer = h.get_manufacturer_string()
            product = h.get_product_string()
            serial = h.get_serial_number_string()
            print(f"  メーカー: {manufacturer}")
            print(f"  製品名: {product}")
            print(f"  シリアル番号: {serial}")
        except Exception as e:
            print(f"デバイス情報の取得に失敗しました: {str(e)}")
        
        # 詳細解析モードの場合、追加のログ情報表示
        if args.deep_analyze:
            print("\n詳細解析モードを有効化しました")
            print("このモードでは以下の追加機能が有効です:")
            print("- バイトレベルでのデータ変更追跡")
            print("- キーマトリックス構造の推測")
            print("- レポート頻度と統計分析")
            print("- ビットマップパターン検出")
        
        # レポートディスクリプタの取得と解析を試みる
        print("\nReport Descriptorの取得と解析を試みます...")
        try:
            # 様々なレポートIDで試みる
            for report_id in range(5):
                try:
                    descriptor_data = h.get_feature_report(report_id, 256)
                    if descriptor_data and len(descriptor_data) > 1:
                        print(f"レポートID {report_id} で成功:")
                        print(f"Raw Descriptor: {binascii.hexlify(bytes(descriptor_data)).decode()}")
                        
                        # ディスクリプタの解析を試みる
                        analysis = analyze_report_descriptor(descriptor_data)
                        print("\nレポートディスクリプタ解析結果:")
                        print(analysis)
                        
                        # 成功したらJSONとして保存
                        desc_filename = f"report_descriptor_id{report_id}.json"
                        with open(desc_filename, "w") as f:
                            json.dump({
                                "report_id": report_id,
                                "raw_data": binascii.hexlify(bytes(descriptor_data)).decode(),
                                "analysis": analysis.split("\n")
                            }, f, indent=2)
                        print(f"解析結果を {desc_filename} に保存しました")
                        break
                except Exception as e:
                    print(f"レポートID {report_id} での取得に失敗: {str(e)}")
        except Exception as e:
            print(f"Report Descriptorの取得・解析に失敗: {str(e)}")
        
        # メインのモニタリング開始
        print("\nデータの高度解析を開始します。")
        print("記録を停止するには Ctrl+C を押してください。")
        if args.monitor_time > 0:
            print(f"{args.monitor_time}秒後に自動的に解析を終了します。")
        print("キー入力またはデバイスからの通信を待機中...\n")
        
        # モニタリング開始時間
        start_time = time.time()
        last_data = None
        report_count = 0
        
        # 30秒ごとに中間状況を報告する
        last_status_time = start_time
        
        # ログファイル準備
        log_file = None
        if args.log:
            log_filename = f"kb16_analysis_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
            log_file = open(log_filename, "w")
            print(f"ログファイル '{log_filename}' に記録を開始します")
        
        try:
            while True:
                # 指定時間経過したら終了
                if args.monitor_time > 0 and time.time() - start_time > args.monitor_time:
                    print(f"\n指定された {args.monitor_time} 秒が経過しました。解析を終了します。")
                    break
                
                # 30秒ごとに中間状況表示
                current_time = time.time()
                if current_time - last_status_time > 30:
                    print(f"\n--- 中間状況 ({int(current_time - start_time)}秒経過) ---")
                    print(f"収集したレポート数: {report_count}")
                    if args.deep_analyze:
                        stats = analyzer.get_report_statistics()
                        print(f"ユニークなレポートパターン: {stats['unique_patterns']}")
                        print("最も頻繁なパターン:")
                        for pattern, count in stats['top_patterns'][:3]:
                            hex_pattern = ' '.join([f"{b:02X}" for b in pattern])
                            print(f"  {hex_pattern} ({count}回)")
                    last_status_time = current_time
                
                # データを待機（ブロッキング読み取り、最大1秒待機）
                data = h.read(64, timeout_ms=1000)
                
                if data:
                    report_count += 1
                    
                    # データ解析
                    capture_and_analyze_report(data, analyzer, keyboard_data)
                    
                    # データが取得できた場合
                    if args.raw:
                        # 生データをバイトとして表示
                        binary_data = bytes(data)
                        print(f"Raw data ({len(binary_data)} bytes): {binascii.hexlify(binary_data).decode()}")
                    
                    # 16進数表示
                    hex_data = ' '.join([f"{byte:02X}" for byte in data])
                    print(f"受信データ #{report_count} ({len(data)} bytes): [{hex_data}]")
                    
                    # ログファイルに記録
                    if log_file:
                        timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        log_file.write(f"{timestamp} - レポート #{report_count}: {hex_data}\n")
                        log_file.flush()
                    
                    # ASCII表示（印刷可能文字のみ）
                    ascii_data = ''.join([chr(byte) if 32 <= byte <= 126 else '.' for byte in data])
                    print(f"ASCII: {ascii_data}")
                    
                    # バイト列の差分を表示（前回のデータとの比較）
                    if last_data:
                        diff_info = []
                        for i, (prev, curr) in enumerate(zip(last_data, data)):
                            if prev != curr:
                                # ビット単位の変化を検出
                                changed_bits = []
                                for bit in range(8):
                                    prev_bit = (prev >> bit) & 1
                                    curr_bit = (curr >> bit) & 1
                                    if prev_bit != curr_bit:
                                        changed_bits.append(bit)
                                
                                bit_info = ", ".join([f"bit{bit}" for bit in changed_bits])
                                diff_info.append(f"バイト{i}: 0x{prev:02X}→0x{curr:02X} (変化: {bit_info})")
                        
                        if diff_info:
                            print("前回との差分: " + " | ".join(diff_info))
                    
                    # HIDキーボードレポートとして解析を試みる
                    if len(data) >= 8:
                        # 標準HIDキーボードレポート形式を想定
                        modifier = data[0]
                        reserved = data[1]
                        keycodes = data[2:8]
                        
                        print(f"修飾キー: 0x{modifier:02X} ", end="")
                        
                        # 修飾キーの解析
                        mod_keys = []
                        if modifier & 0x01: mod_keys.append("左Ctrl")
                        if modifier & 0x02: mod_keys.append("左Shift")
                        if modifier & 0x04: mod_keys.append("左Alt")
                        if modifier & 0x08: mod_keys.append("左GUI")
                        if modifier & 0x10: mod_keys.append("右Ctrl")
                        if modifier & 0x20: mod_keys.append("右Shift")
                        if modifier & 0x40: mod_keys.append("右Alt")
                        if modifier & 0x80: mod_keys.append("右GUI")
                        
                        if mod_keys:
                            print(f"({', '.join(mod_keys)})")
                        else:
                            print("(なし)")
                        
                        # キーコードの解析
                        active_keys = []
                        for i, keycode in enumerate(keycodes):
                            if keycode != 0:
                                key_info = f"0x{keycode:02X}"
                                # キーコードをASCIIに変換
                                shift = (modifier & 0x22) != 0  # 左右どちらかのShiftが押されているか
                                ascii_char = keycode_to_ascii(keycode, shift)
                                if ascii_char:
                                    key_info += f" ('{ascii_char}')"
                                active_keys.append(key_info)
                                
                        if active_keys:
                            print(f"キーコード: {', '.join(active_keys)}")
                        else:
                            print("キーコード: なし（キー押下なし）")
                        
                        # 詳細解析モードの場合の追加情報
                        if args.deep_analyze:
                            # キー状態変化の詳細追跡
                            if "pressed" in analyzer.key_states:
                                pressed = analyzer.key_states["pressed"]
                                press_info = ", ".join([f"0x{k:02X}" for k in pressed])
                                print(f"新たに押されたキー: {press_info}")
                            
                            if "released" in analyzer.key_states:
                                released = analyzer.key_states["released"]
                                release_info = ", ".join([f"0x{k:02X}" for k in released])
                                print(f"離されたキー: {release_info}")
                        
                        print()  # 空行を挿入
                    
                    last_data = data
        
        except KeyboardInterrupt:
            print("\n\nキー入力を検出しました。解析を終了します。")
        
        finally:
            # ログファイルを閉じる
            if log_file:
                log_file.close()
            
            # 解析結果の保存
            print("\n解析結果を保存しています...")
            json_file, csv_file = analyzer.save_to_file("kb16")
            print(f"解析結果を {json_file} および {csv_file} に保存しました")
            
            # 解析結果のサマリーを表示
            print("\n解析サマリー:")
            stats = analyzer.get_report_statistics()
            print(f"総レポート数: {stats['total_reports']}")
            print(f"ユニークなパターン数: {stats['unique_patterns']}")
            print("\n最も一般的なレポートパターン:")
            for i, (pattern, count) in enumerate(stats['top_patterns']):
                if i >= 3:  # 上位3つのみ表示
                    break
                hex_pattern = ' '.join([f"{b:02X}" for b in pattern])
                print(f"  パターン {i+1}: {hex_pattern} ({count}回, {count/stats['total_reports']*100:.1f}%)")
            
            # マトリックス解析結果
            matrix_analysis = analyzer.get_matrix_analysis()
            if matrix_analysis:
                print("\nキーマトリックス解析結果:")
                for key_id, info in matrix_analysis.items():
                    print(f"  {key_id}: マトリックス位置 {info['position']} ({info['matrix_label']}), 変化回数: {info['change_count']}")
            
            print("\nヒント:")
            print("1. 生成されたCSVファイルはExcelで開いて詳細な分析が可能です")
            print("2. JSONファイルにはビットマップ解析とマトリックス推測結果が含まれています")
            print("3. より詳細な解析には --deep-analyze オプションを使用してください")
            
            try:
                if 'h' in locals() and h:
                    h.close()
                    print("\nHID接続を閉じました。プログラムを終了します。")
            except:
                pass
    
    except Exception as e:
        print(f"エラーが発生しました: {str(e)}")
        if 'h' in locals() and h:
            h.close()
            print("HID接続を閉じました。プログラムを終了します。")
            return
    finally:
        # HIDデバイスを閉じる
        if 'h' in locals() and h:
            h.close()
            print("HID接続を閉じました。プログラムを終了します。")
            return
        print("プログラムを終了します。")

if __name__ == "__main__":
    main()
