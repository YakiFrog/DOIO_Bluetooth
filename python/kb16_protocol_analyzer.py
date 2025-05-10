#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_protocol_analyzer.py
# DOIO KB16 プロトコル解析ツール
# 複数のアプローチでキーボードとの通信を確立し、
# プロトコルの詳細を解析するためのツール

import hid
import time
import sys
import argparse
import binascii
import json
import os
import datetime
import csv
import struct
from collections import defaultdict
import platform
import signal

# HIDレポートをキャプチャするクラス
class HIDReportCapture:
    def __init__(self, output_prefix=None):
        self.reports = []
        self.output_prefix = output_prefix or f"kb16_capture_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}"
        self.start_time = datetime.datetime.now()
        self.key_state = {}
        self.last_report = None
        self.keycode_map = self._create_keycode_map()
        self.pressed_keys = set()
        
    def _create_keycode_map(self):
        """キーコードマップを作成"""
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
            0x28: ('Enter', 'Enter'), 0x29: ('Esc', 'Esc'), 0x2A: ('Backspace', 'Backspace'),
            0x2B: ('Tab', 'Tab'), 0x2C: ('Space', 'Space'), 0x2D: ('-', '='), 0x2E: ('^', '~'),
            0x2F: ('@', '`'), 0x30: ('[', '{'), 0x31: (None, None), 0x32: (']', '}'),
            0x33: (';', '+'), 0x34: (':', '*'), 0x36: (',', '<'), 0x37: ('.', '>'),
            0x38: ('/', '?'), 0x54: ('/', '/'), 0x55: ('*', '*'), 0x56: ('-', '-'),
            0x57: ('+', '+'), 0x58: ('KP_Enter', 'KP_Enter'), 0x59: ('1', None), 0x5A: ('2', None),
            0x5B: ('3', None), 0x5C: ('4', None), 0x5D: ('5', '5'), 0x5E: ('6', None),
            0x5F: ('7', None), 0x60: ('8', None), 0x61: ('9', None), 0x62: ('0', None),
            0x63: ('.', None), 0x67: ('=', '='),
            # ファンクションキー
            0x3A: ('F1', 'F1'), 0x3B: ('F2', 'F2'), 0x3C: ('F3', 'F3'),
            0x3D: ('F4', 'F4'), 0x3E: ('F5', 'F5'), 0x3F: ('F6', 'F6'),
            0x40: ('F7', 'F7'), 0x41: ('F8', 'F8'), 0x42: ('F9', 'F9'),
            0x43: ('F10', 'F10'), 0x44: ('F11', 'F11'), 0x45: ('F12', 'F12'),
            # 特殊キー
            0x46: ('PrintScreen', 'PrintScreen'), 0x47: ('ScrollLock', 'ScrollLock'), 
            0x48: ('Pause', 'Pause'), 0x49: ('Insert', 'Insert'),
            0x4A: ('Home', 'Home'), 0x4B: ('PageUp', 'PageUp'), 0x4C: ('Delete', 'Delete'), 
            0x4D: ('End', 'End'), 0x4E: ('PageDown', 'PageDown'),
            0x4F: ('Right', 'Right'), 0x50: ('Left', 'Left'), 0x51: ('Down', 'Down'), 
            0x52: ('Up', 'Up'), 0x53: ('NumLock', 'NumLock')
        }
        return keycode_table
    
    def capture(self, data, timestamp=None):
        """レポートをキャプチャして解析"""
        if not timestamp:
            timestamp = datetime.datetime.now()
        
        delta = (timestamp - self.start_time).total_seconds()
        
        # レポートを保存
        report_data = {
            'timestamp': timestamp.isoformat(),
            'delta_seconds': delta,
            'data': list(data),
            'hex': ' '.join([f"{b:02X}" for b in data])
        }
        
        # キーボードレポートの解析を追加
        if len(data) >= 8:  # 標準HIDレポート形式を想定
            modifier = data[0]
            keycodes = data[2:8]
            
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
            
            report_data['modifier'] = modifier
            report_data['modifier_keys'] = mod_keys
            
            # アクティブなキーの取得
            active_keys = []
            shift_pressed = (modifier & 0x22) != 0  # 左右どちらかのShiftが押されているか
            
            for keycode in keycodes:
                if keycode != 0:
                    key_info = {'keycode': keycode}
                    
                    # キーコードをキー名に変換
                    if keycode in self.keycode_map:
                        key_name = self.keycode_map[keycode][1 if shift_pressed else 0]
                        if key_name:
                            key_info['name'] = key_name
                    
                    active_keys.append(key_info)
            
            report_data['active_keys'] = active_keys
            
            # 押されたキーと離されたキーを検出
            current_keycodes = set(k for k in keycodes if k != 0)
            
            if self.pressed_keys:
                newly_pressed = current_keycodes - self.pressed_keys
                released = self.pressed_keys - current_keycodes
                
                if newly_pressed:
                    report_data['newly_pressed'] = list(newly_pressed)
                if released:
                    report_data['released'] = list(released)
            
            self.pressed_keys = current_keycodes
        
        self.reports.append(report_data)
        return report_data
    
    def get_report_difference(self, report1, report2):
        """2つのレポートの差分を取得"""
        if not report1 or not report2:
            return None
        
        diff = {'changed_bytes': {}}
        
        # 同じ長さのデータのみ比較
        min_len = min(len(report1['data']), len(report2['data']))
        
        for i in range(min_len):
            if report1['data'][i] != report2['data'][i]:
                byte_diff = {
                    'old': report1['data'][i],
                    'new': report2['data'][i],
                    'changed_bits': []
                }
                
                # ビットごとの変化を検出
                for bit in range(8):
                    old_bit = (report1['data'][i] >> bit) & 1
                    new_bit = (report2['data'][i] >> bit) & 1
                    if old_bit != new_bit:
                        byte_diff['changed_bits'].append({
                            'bit': bit,
                            'old': old_bit,
                            'new': new_bit
                        })
                
                diff['changed_bytes'][i] = byte_diff
        
        return diff
    
    def save_to_json(self):
        """キャプチャしたレポートをJSONファイルに保存"""
        # 分析情報を追加
        analysis = self.analyze_reports()
        
        output_data = {
            'start_time': self.start_time.isoformat(),
            'end_time': datetime.datetime.now().isoformat(),
            'report_count': len(self.reports),
            'reports': self.reports,
            'analysis': analysis
        }
        
        filename = f"{self.output_prefix}.json"
        with open(filename, "w") as f:
            json.dump(output_data, f, indent=2)
        
        print(f"キャプチャしたデータを {filename} に保存しました")
        return filename
    
    def save_to_csv(self):
        """キャプチャしたレポートをCSVファイルに保存"""
        filename = f"{self.output_prefix}.csv"
        with open(filename, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp", "Delta (s)", "Raw Data (Hex)", "Modifier", "Modifier Keys", "Active Keys"])
            
            for report in self.reports:
                timestamp = report['timestamp']
                delta = report['delta_seconds']
                hex_data = report['hex']
                
                modifier = report.get('modifier', '')
                mod_keys = ', '.join(report.get('modifier_keys', []))
                
                active_keys = []
                for key in report.get('active_keys', []):
                    if 'name' in key:
                        active_keys.append(f"{key['name']}(0x{key['keycode']:02X})")
                    else:
                        active_keys.append(f"0x{key['keycode']:02X}")
                
                active_keys_str = ', '.join(active_keys)
                
                writer.writerow([timestamp, delta, hex_data, modifier, mod_keys, active_keys_str])
        
        print(f"キャプチャしたデータを {filename} に保存しました")
        return filename
    
    def analyze_reports(self):
        """レポートの分析を行う"""
        if not self.reports:
            return {"error": "レポートがありません"}
        
        analysis = {}
        
        # レポート長の分布
        report_lengths = {}
        for report in self.reports:
            length = len(report['data'])
            report_lengths[length] = report_lengths.get(length, 0) + 1
        
        analysis['report_lengths'] = report_lengths
        
        # レポートパターンの分析
        patterns = defaultdict(int)
        for report in self.reports:
            pattern = tuple(report['data'])
            patterns[pattern] += 1
        
        # 上位5つのパターン
        top_patterns = sorted(patterns.items(), key=lambda x: x[1], reverse=True)[:5]
        analysis['top_patterns'] = []
        
        for pattern, count in top_patterns:
            analysis['top_patterns'].append({
                'pattern': list(pattern),
                'hex': ' '.join([f"{b:02X}" for b in pattern]),
                'count': count,
                'percentage': count / len(self.reports) * 100
            })
        
        # 修飾キーの使用頻度
        if any('modifier' in report for report in self.reports):
            modifier_usage = defaultdict(int)
            for report in self.reports:
                if 'modifier' in report:
                    modifier = report['modifier']
                    modifier_usage[modifier] = modifier_usage.get(modifier, 0) + 1
            
            analysis['modifier_usage'] = dict(modifier_usage)
        
        # キーコードの使用頻度
        keycode_usage = defaultdict(int)
        for report in self.reports:
            for key in report.get('active_keys', []):
                keycode = key['keycode']
                keycode_usage[keycode] = keycode_usage.get(keycode, 0) + 1
        
        analysis['keycode_usage'] = dict(keycode_usage)
        
        return analysis

class KB16DeviceManager:
    """DOIO KB16キーボードデバイスを管理するクラス"""
    
    def __init__(self, vendor_id=0xD010, product_id=0x1601, interface=None):
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.target_interface = interface
        self.devices = []
        self.active_device = None
        self.report_capture = HIDReportCapture()
    
    def enumerate_devices(self):
        """すべてのHIDデバイスを列挙し、目的のデバイスを特定"""
        all_devices = hid.enumerate()
        matched_devices = []
        
        for device in all_devices:
            if device['vendor_id'] == self.vendor_id and device['product_id'] == self.product_id:
                interface_number = device.get('interface_number', -1)
                
                device_info = {
                    'path': device.get('path'),
                    'interface': interface_number,
                    'usage_page': device.get('usage_page', 0),
                    'usage': device.get('usage', 0),
                    'manufacturer': device.get('manufacturer_string', 'Unknown'),
                    'product': device.get('product_string', 'Unknown'),
                    'release_number': device.get('release_number', 0),
                }
                
                matched_devices.append(device_info)
        
        self.devices = matched_devices
        return matched_devices
    
    def find_best_device(self):
        """接続に最適なデバイスを特定"""
        if not self.devices:
            return None
        
        # 特定のインターフェースが指定された場合
        if self.target_interface is not None:
            for device in self.devices:
                if device['interface'] == self.target_interface:
                    return device
        
        # キーボード/マウス関連のUsage Pageを優先
        for device in self.devices:
            usage_page = device.get('usage_page', 0)
            if usage_page == 1:  # Generic Desktop Controls
                return device
        
        # 何も見つからなければ最初のデバイスを返す
        return self.devices[0]
    
    def open_device(self, device_info=None):
        """デバイスを開く"""
        if device_info is None:
            device_info = self.find_best_device()
            if not device_info:
                raise Exception("開くべきデバイスが見つかりません")
        
        h = hid.device()
        
        if 'path' in device_info and device_info['path']:
            try:
                h.open_path(device_info['path'])
                print(f"パスを使用してデバイスを開きました")
            except Exception as e:
                print(f"パスでの接続に失敗: {e}")
                h.open(self.vendor_id, self.product_id)
                print("VID/PIDで接続しました")
        else:
            h.open(self.vendor_id, self.product_id)
            print("VID/PIDで接続しました")
        
        self.active_device = {
            'device': h,
            'info': device_info
        }
        
        return h
    
    def read_reports(self, duration=60, callback=None):
        """指定された時間だけレポートを読み取る"""
        if not self.active_device:
            raise Exception("先にデバイスを開く必要があります")
        
        h = self.active_device['device']
        start_time = time.time()
        report_count = 0
        
        print(f"\n{duration}秒間のレポート読み取りを開始します...")
        print("キーを押して、キーボードの動作をテストしてください。Ctrl+Cで中止できます。")
        
        try:
            while time.time() - start_time < duration:
                try:
                    data = h.read(64, timeout_ms=100)
                    if data:
                        report_count += 1
                        timestamp = datetime.datetime.now()
                        
                        # レポートのキャプチャと解析
                        report_info = self.report_capture.capture(data, timestamp)
                        
                        # 16進数表示
                        hex_data = ' '.join([f"{byte:02X}" for byte in data])
                        print(f"\nレポート #{report_count} ({len(data)} bytes): {hex_data}")
                        
                        # アクティブキーの表示
                        if 'active_keys' in report_info and report_info['active_keys']:
                            key_infos = []
                            for key in report_info['active_keys']:
                                if 'name' in key:
                                    key_infos.append(f"{key['name']}(0x{key['keycode']:02X})")
                                else:
                                    key_infos.append(f"0x{key['keycode']:02X}")
                            
                            if key_infos:
                                print(f"アクティブなキー: {', '.join(key_infos)}")
                        
                        # コールバック関数があれば呼び出し
                        if callback:
                            callback(report_info)
                        
                except KeyboardInterrupt:
                    print("\n中断されました")
                    break
        finally:
            pass
        
        print(f"\n合計 {report_count} 件のレポートを受信しました")
        return report_count
    
    def close(self):
        """接続を閉じる"""
        if self.active_device:
            try:
                self.active_device['device'].close()
                self.active_device = None
                print("デバイス接続を閉じました")
            except Exception as e:
                print(f"デバイスを閉じる際にエラーが発生: {e}")
    
    def save_reports(self):
        """キャプチャしたレポートを保存"""
        json_file = self.report_capture.save_to_json()
        csv_file = self.report_capture.save_to_csv()
        return json_file, csv_file

def main():
    parser = argparse.ArgumentParser(description='DOIO KB16 プロトコル解析ツール')
    parser.add_argument('--vid', help='ベンダーIDを16進数で指定 (例: 0xD010)', default="0xD010")
    parser.add_argument('--pid', help='プロダクトIDを16進数で指定 (例: 0x1601)', default="0x1601")
    parser.add_argument('--interface', type=int, help='インターフェース番号を指定', default=None)
    parser.add_argument('--duration', type=int, help='キャプチャ時間（秒）', default=60)
    parser.add_argument('--output', help='出力ファイル名のプレフィックス', default=None)
    args = parser.parse_args()
    
    # VID/PIDを16進数から変換
    vendor_id = int(args.vid, 16)
    product_id = int(args.pid, 16)
    
    # 出力ファイル名プレフィックス
    output_prefix = args.output or f"kb16_capture_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}"
    
    print("\nDOIO KB16 プロトコル解析ツール")
    print(f"対象デバイス: VID=0x{vendor_id:04X}, PID=0x{product_id:04X}")
    
    if args.interface is not None:
        print(f"インターフェース: {args.interface}")
    
    # デバイスマネージャーの初期化
    device_manager = KB16DeviceManager(
        vendor_id=vendor_id,
        product_id=product_id,
        interface=args.interface
    )
    
    # レポートキャプチャーの初期化
    device_manager.report_capture = HIDReportCapture(output_prefix)
    
    try:
        # 利用可能なデバイスを列挙
        devices = device_manager.enumerate_devices()
        
        if not devices:
            print(f"\nVID=0x{vendor_id:04X}, PID=0x{product_id:04X} のデバイスが見つかりません。")
            return
        
        print(f"\n{len(devices)}個の対象デバイスを検出:")
        
        for idx, device in enumerate(devices):
            interface = device['interface']
            usage_page = device['usage_page']
            usage = device['usage']
            
            print(f"デバイス #{idx+1}:")
            print(f"  インターフェース: {interface}")
            print(f"  Usage Page: 0x{usage_page:04X}, Usage: 0x{usage:04X}")
            print(f"  メーカー: {device['manufacturer']}")
            print(f"  製品名: {device['product']}")
        
        # 最適なデバイスを特定
        best_device = device_manager.find_best_device()
        if not best_device:
            print("適切なデバイスが見つかりません。")
            return
        
        print(f"\n接続に最適なデバイス: インターフェース {best_device['interface']}, Usage Page 0x{best_device['usage_page']:04X}")
        
        # デバイスを開く
        try:
            h = device_manager.open_device(best_device)
            
            # デバイス情報の取得
            try:
                manufacturer = h.get_manufacturer_string()
                product = h.get_product_string()
                serial = h.get_serial_number_string()
                
                print("\nデバイス情報:")
                print(f"  メーカー: {manufacturer}")
                print(f"  製品名: {product}")
                print(f"  シリアル番号: {serial}")
            except Exception as e:
                print(f"デバイス情報の取得に失敗: {e}")
            
            # レポート読み取り
            device_manager.read_reports(args.duration)
            
            # レポートの保存
            json_file, csv_file = device_manager.save_reports()
            
            print("\n解析のためのコマンド:")
            print(f"  python -m json.tool {json_file} | less")
            print(f"  csvファイルはExcelなどのスプレッドシートで開いてください: {csv_file}")
            
        finally:
            # 接続を閉じる
            device_manager.close()
    
    except KeyboardInterrupt:
        print("\n中断されました")
    except Exception as e:
        print(f"エラーが発生しました: {e}")

if __name__ == "__main__":
    main()
