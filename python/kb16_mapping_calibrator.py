#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DOIO KB16 キーボードのマッピングキャリブレーションツール

このツールは、KB16キーボードの各キーを順番に押下し、
対応するビットパターンを分析して正確なマッピングテーブルを生成するためのものです。
"""

import sys
import json
import time
import os
import struct
from datetime import datetime
import argparse

# macOS特有の設定 - PyUSBがlibusb-1.0を見つけるために必要
try:
    # Homebrewでインストールした場合のライブラリパスを追加
    brew_lib_paths = [
        "/opt/homebrew/lib",
        "/usr/local/lib",
        "/opt/local/lib"
    ]
    
    # libusb-1.0.dylibのパスを追加
    for path in brew_lib_paths:
        if os.path.exists(path):
            # メッセージを表示
            print(f"ライブラリパスを追加: {path}")
            # 環境変数にパスを追加
            os.environ['DYLD_LIBRARY_PATH'] = f"{path}:{os.environ.get('DYLD_LIBRARY_PATH', '')}"
except Exception as e:
    print(f"警告: ライブラリパスの設定中にエラーが発生しました: {e}")

# PyUSBを最後にインポート（パスの設定後）
import usb.core
import usb.util

# DOIO KB16のVID/PID
VENDOR_ID = 0xD010
PRODUCT_ID = 0x1601

# 収集データディレクトリ
DATA_DIR = "kb16_mapping_data"

# KB16物理キー配列（4x4マトリックス）
KB16_PHYSICAL_LAYOUT = [
    ["1", "2", "3", "4"],
    ["5", "6", "7", "8"],
    ["9", "0", "Enter", "Esc"],
    ["Backspace", "Tab", "Space", "Alt"]
]

# HIDキーコード定義
HID_KEY_CODES = {
    "1": 0x1E,        # 1
    "2": 0x1F,        # 2
    "3": 0x20,        # 3
    "4": 0x21,        # 4
    "5": 0x22,        # 5
    "6": 0x23,        # 6
    "7": 0x24,        # 7
    "8": 0x25,        # 8
    "9": 0x26,        # 9
    "0": 0x27,        # 0
    "Enter": 0x28,    # Enter
    "Esc": 0x29,      # Escape
    "Backspace": 0x2A, # Backspace
    "Tab": 0x2B,      # Tab
    "Space": 0x2C,    # Space
    "Alt": 0xE6       # 右Alt
}

class KB16Calibrator:
    def __init__(self):
        self.device = None
        self.endpoint = None
        self.interface_number = None
        self.collected_data = []
        self.key_bit_patterns = {}
        self.timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # コマンドライン引数で指定される可能性のあるオプション
        self.target_interface = None
        self.target_endpoint = None
        
        # データ保存ディレクトリの確保
        if not os.path.exists(DATA_DIR):
            os.makedirs(DATA_DIR)
    
    def find_device(self):
        """DOIO KB16デバイスを検索"""
        # すべてのUSBデバイスを表示（デバッグ用）
        print("\n--- すべての接続されたUSBデバイス ---")
        devices = list(usb.core.find(find_all=True))
        for idx, dev in enumerate(devices):
            try:
                print(f"デバイス {idx}: ID={dev.idVendor:04x}:{dev.idProduct:04x}, 製造元={dev.manufacturer if hasattr(dev, 'manufacturer') else '不明'}")
            except:
                print(f"デバイス {idx}: ID={dev.idVendor:04x}:{dev.idProduct:04x}, 製造元=<取得失敗>")
        print("------------------------------\n")
        
        # KB16デバイスを検索
        self.device = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
        
        if self.device is None:
            print(f"エラー: VID=0x{VENDOR_ID:04X}, PID=0x{PRODUCT_ID:04X}のデバイスが見つかりません")
            return False
            
        # デバイス情報を表示
        print(f"デバイス検出: {self.device}")
        print(f"製造元ID: 0x{self.device.idVendor:04X}")
        print(f"製品ID: 0x{self.device.idProduct:04X}")
        
        # すべてのインターフェースについてカーネルドライバをデタッチ
        for interface in range(3):  # インターフェース0,1,2をデタッチ
            try:
                if self.device.is_kernel_driver_active(interface):
                    try:
                        self.device.detach_kernel_driver(interface)
                        print(f"インターフェース {interface} のカーネルドライバをデタッチしました")
                    except usb.core.USBError as e:
                        print(f"インターフェース {interface} のカーネルドライバのデタッチに失敗: {e}")
                else:
                    print(f"インターフェース {interface} はカーネルドライバがアクティブではありません")
            except:
                # ドライバがない場合（Windowsなど）はスキップ
                pass
                
        # デバイスをリセット
        try:
            self.device.reset()
            print("デバイスをリセットしました")
        except usb.core.USBError as e:
            print(f"警告: デバイスリセット失敗: {e}")
        
        # デバイス設定
        try:
            self.device.set_configuration()
            cfg = self.device.get_active_configuration()
            
            # すべてのインターフェースとエンドポイントを表示（デバッグ用）
            print("\n--- 利用可能なインターフェース/エンドポイント ---")
            for intf_idx, intf in enumerate(cfg):
                print(f"インターフェース #{intf.bInterfaceNumber}: クラス={intf.bInterfaceClass}, サブクラス={intf.bInterfaceSubClass}, プロトコル={intf.bInterfaceProtocol}")
                for ep_idx, ep in enumerate(intf):
                    direction = "IN" if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else "OUT"
                    print(f"  エンドポイント {ep_idx}: アドレス=0x{ep.bEndpointAddress:02X}, 種類={ep.bmAttributes:02X}, 方向={direction}")
            print("-------------------------------------------\n")
            
            # キーボードHIDインターフェースを探す - インターフェース1を優先（一般的なキーボードインターフェース）
            target_interfaces = []
            
            # まずすべてのHIDインターフェースをリストアップ
            for intf in cfg:
                if intf.bInterfaceClass == 3:  # HIDクラス
                    target_interfaces.append(intf)
            
            # コマンドラインで指定されていればそれを使用
            if self.target_interface is not None:
                for intf in target_interfaces:
                    if intf.bInterfaceNumber == self.target_interface:
                        self.interface_number = intf.bInterfaceNumber
                        print(f"指定されたHIDインターフェース #{self.interface_number} を使用")
                        
                        # エンドポイントを探す
                        for ep in intf:
                            # 指定されたエンドポイントがあれば使用
                            if self.target_endpoint is not None and ep.bEndpointAddress == self.target_endpoint:
                                self.endpoint = ep
                                print(f"指定されたエンドポイント: 0x{ep.bEndpointAddress:02X}, ポーリング間隔: {ep.bInterval}ms")
                                return True
                            # なければIN方向のエンドポイントを使用
                            elif self.target_endpoint is None and usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN:
                                self.endpoint = ep
                                print(f"INエンドポイント: 0x{ep.bEndpointAddress:02X}, ポーリング間隔: {ep.bInterval}ms")
                                return True
            
            # インターフェース1が存在すれば優先的に使用
            for intf in target_interfaces:
                if intf.bInterfaceNumber == 1:
                    self.interface_number = intf.bInterfaceNumber
                    print(f"HIDインターフェース #{self.interface_number} を優先的に使用")
                    
                    # エンドポイントを探す
                    for ep in intf:
                        if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN:
                            self.endpoint = ep
                            print(f"INエンドポイント: 0x{ep.bEndpointAddress:02X}, ポーリング間隔: {ep.bInterval}ms")
                            return True
            
            # インターフェース1がなければ他のHIDインターフェースを使用
            for intf in target_interfaces:
                self.interface_number = intf.bInterfaceNumber
                print(f"HIDインターフェース #{self.interface_number} を検出")
                
                # エンドポイントを探す
                for ep in intf:
                    if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN:
                        self.endpoint = ep
                        print(f"INエンドポイント: 0x{ep.bEndpointAddress:02X}, ポーリング間隔: {ep.bInterval}ms")
                        return True
            
            if self.endpoint is None:
                print("適切なエンドポイントが見つかりません")
                return False
                
        except usb.core.USBError as e:
            print(f"USB設定エラー: {e}")
            return False
        
        return False
    
    def collect_data(self):
        """各キーのデータ収集を実行"""
        if not self.find_device():
            print("KB16キーボードが見つかりません。接続を確認してください。")
            return
            
        print("\n===== DOIO KB16 キーマッピングキャリブレーション =====\n")
        print("このツールは各キーを押すたびにデータを収集し、正確なマッピングを生成します。")
        print("指示に従って各キーを押し、その間は他のキーに触れないでください。")
        
        try:
            # 基準データを収集（何も押していない状態）
            input("\nまず、すべてのキーから指を離し、Enterを押してください...")
            base_data = self.read_report()
            if base_data is None or len(base_data) == 0:
                print("警告: 基準データの読み取りに失敗しました。再試行します...")
                time.sleep(1)
                base_data = self.read_report()
                
            print("基準データを記録しました。")
            
            # 各キーごとにデータを収集
            collected_keys = {}
            
            for row_idx, row in enumerate(KB16_PHYSICAL_LAYOUT):
                for col_idx, key_name in enumerate(row):
                    # キー押下指示
                    input(f"\n「{key_name}」キー (行:{row_idx}, 列:{col_idx}) を押し、押したままEnterを押してください...")
                    
                    # データ読み取り（複数回試行して安定したデータを得る）
                    samples = []
                    valid_samples = 0
                    max_attempts = 8  # 最大試行回数を増やす
                    
                    for attempt in range(max_attempts):
                        sample = self.read_report()
                        if sample is not None and len(sample) > 0:
                            samples.append(sample)
                            valid_samples += 1
                            if valid_samples >= 5:  # 有効なサンプルが5つあれば十分
                                break
                        time.sleep(0.2)  # 待機時間を増やす
                    
                    if not samples:
                        print(f"警告: 「{key_name}」キーのデータ取得に失敗しました。ダミーデータを使用します。")
                        # ダミーデータを作成（基準データの1バイト目を変更）
                        dummy_data = bytearray(base_data)
                        if len(dummy_data) > 0:
                            dummy_data[0] ^= 0x01  # 最初のバイトの最下位ビットを反転
                        key_data = dummy_data
                    else:
                        # 最もよく現れたパターンを採用
                        key_data = max(samples, key=samples.count)
                    
                    # キーごとの差分を計算
                    diff_data = self.calculate_diff(base_data, key_data)
                    
                    # ビットパターンを調査
                    bit_pattern = self.analyze_bit_pattern(base_data, key_data)
                    
                    # 結果を保存
                    key_info = {
                        "key_name": key_name,
                        "row": row_idx,
                        "col": col_idx,
                        "hid_keycode": HID_KEY_CODES[key_name],
                        "base_data": [f"0x{b:02X}" for b in base_data],
                        "key_data": [f"0x{b:02X}" for b in key_data],
                        "diff_data": diff_data,
                        "bit_pattern": bit_pattern
                    }
                    
                    self.collected_data.append(key_info)
                    collected_keys[f"{row_idx},{col_idx}"] = key_info
                    
                    print(f"「{key_name}」キーのデータを記録しました: {bit_pattern}")
                    
                    # キー解放指示
                    input("キーを離し、Enterを押してください...")
                    time.sleep(0.5)
            
            # 結果を保存
            self.save_results()
            
            # マッピング生成
            self.generate_mapping()
            
        except KeyboardInterrupt:
            print("\n中断されました。")
        except Exception as e:
            print(f"\nエラーが発生しました: {e}")
        finally:
            self.cleanup()
    
    def read_report(self):
        """USBレポートを読み取る"""
        max_retries = 5  # リトライ回数を増やす
        timeout_ms = 500  # タイムアウトを短くする
        
        for retry in range(max_retries):
            try:
                # HID Getレポート要求を送信してデータを取得
                # 通常のreadだとタイムアウトする場合があるため、
                # ctrl_transfer（コントロール転送）を使用してデータを取得する手法も試す
                try:
                    # 標準的なHIDレポート要求
                    data = self.device.read(self.endpoint.bEndpointAddress, self.endpoint.wMaxPacketSize, timeout_ms)
                    if data and len(data) > 0:
                        print(f"データ取得成功 (方法1): {[hex(x) for x in data]}")
                        return data
                except usb.core.USBError as e:
                    if retry == 0:  # 最初のリトライでのみメッセージを表示
                        print(f"方法1失敗: {e}")
                    
                    # 方法1が失敗したら代替方法を試す
                    try:
                        # HIDレポート要求（コントロール転送）
                        # bmRequestType (0xA1) = 10100001b [方向=IN, タイプ=クラス固有, 受信者=インタフェース]
                        # bRequest = 0x01 (GET_REPORT)
                        # wValue = 0x0100 (レポートタイプ=Input, レポートID=0)
                        # wIndex = インタフェース番号
                        data = self.device.ctrl_transfer(
                            bmRequestType=0xA1,
                            bRequest=0x01,  # GET_REPORT
                            wValue=0x0100,  # レポートタイプ=Input(1), レポートID=0
                            wIndex=self.interface_number,
                            data_or_wLength=self.endpoint.wMaxPacketSize,
                            timeout=timeout_ms
                        )
                        if data and len(data) > 0:
                            print(f"データ取得成功 (方法2): {[hex(x) for x in data]}")
                            return data
                    except usb.core.USBError as e2:
                        if retry == 0:  # 最初のリトライでのみメッセージを表示
                            print(f"方法2失敗: {e2}")
                
                # 次のリトライ前に少し長めに待機
                time.sleep(0.8)
                    
            except Exception as e:
                print(f"読み取り例外: {e} (リトライ {retry+1}/{max_retries})")
                time.sleep(0.8)
        
        # すべてのリトライが失敗した場合、ダミーデータを返す
        print("すべてのデータ取得方法が失敗しました。ダミーデータを返します")
        return bytearray(8)  # 8バイトの空データ（標準的なHIDレポートサイズ）
    
    def calculate_diff(self, base_data, key_data):
        """ベースデータとキーデータの差分を計算"""
        # データが None でないことを確認
        if base_data is None or key_data is None:
            print("警告: 有効なデータがありません")
            return []
            
        diff = []
        for i in range(min(len(base_data), len(key_data))):
            if base_data[i] != key_data[i]:
                diff.append({
                    "byte_idx": i,
                    "base_value": f"0x{base_data[i]:02X}",
                    "key_value": f"0x{key_data[i]:02X}",
                    "diff_mask": f"0x{(base_data[i] ^ key_data[i]):02X}"
                })
        return diff
    
    def analyze_bit_pattern(self, base_data, key_data):
        """ビットパターンの変化を分析"""
        # データが None でないことを確認
        if base_data is None or key_data is None:
            print("警告: ビットパターン分析に有効なデータがありません")
            return []
            
        patterns = []
        
        for i in range(min(len(base_data), len(key_data))):
            if base_data[i] != key_data[i]:
                # 変化したビットを特定
                changed_bits = base_data[i] ^ key_data[i]
                for bit in range(8):
                    if (changed_bits >> bit) & 1:
                        bit_mask = 1 << bit
                        is_set = (key_data[i] & bit_mask) != 0
                        patterns.append({
                            "byte_idx": i,
                            "bit": bit,
                            "bit_mask": f"0x{bit_mask:02X}",
                            "state": "押下" if is_set else "解放"
                        })
        
        return patterns
    
    def save_results(self):
        """収集したデータを保存"""
        filename = f"{DATA_DIR}/kb16_mapping_data_{self.timestamp}.json"
        
        data = {
            "device_info": {
                "vendor_id": f"0x{VENDOR_ID:04X}",
                "product_id": f"0x{PRODUCT_ID:04X}",
                "timestamp": self.timestamp
            },
            "physical_layout": KB16_PHYSICAL_LAYOUT,
            "key_data": self.collected_data
        }
        
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        
        print(f"\nマッピングデータを保存しました: {filename}")
    
    def generate_mapping(self):
        """最適なマッピングを生成"""
        # 各キーについて最も安定したビットパターンを特定
        key_mappings = []
        
        for key_info in self.collected_data:
            if not key_info['bit_pattern']:
                print(f"警告: {key_info['key_name']}キーのビットパターンが検出できませんでした")
                continue
                
            # 最も信頼性の高いビットパターンを選択
            best_pattern = key_info['bit_pattern'][0]  # 単純化のため最初のパターン
            
            mapping = {
                "byte_idx": best_pattern['byte_idx'],
                "bit_mask": best_pattern['bit_mask'],
                "row": key_info['row'],
                "col": key_info['col'],
                "key_name": key_info['key_name'],
                "hid_keycode": f"0x{key_info['hid_keycode']:02X}"
            }
            
            key_mappings.append(mapping)
        
        # C++コード生成
        cpp_code = "// DOIO KB16 キーマッピング - 自動生成 {}\n".format(self.timestamp)
        cpp_code += "const KeyMapping kb16_key_map[] = {\n"
        
        for mapping in key_mappings:
            cpp_code += f"    {{ {mapping['byte_idx']}, {mapping['bit_mask']}, {mapping['row']}, {mapping['col']} }},  // {mapping['key_name']}\n"
            
        cpp_code += "};\n\n"
        
        # HIDキーコードマッピングコード
        cpp_code += "// キー行列からHIDコードへのマッピング\n"
        cpp_code += "uint8_t getHidKeyCode(uint8_t row, uint8_t col) {\n"
        cpp_code += "    if (row >= 4 || col >= 4) return 0;\n"
        cpp_code += "    \n"
        cpp_code += "    switch (row * 4 + col) {\n"
        
        for mapping in key_mappings:
            index = mapping['row'] * 4 + mapping['col']
            cpp_code += f"        case {index}: return {mapping['hid_keycode']};  // {mapping['key_name']}\n"
            
        cpp_code += "        default: return 0;\n"
        cpp_code += "    }\n"
        cpp_code += "}\n"
        
        # マッピングコードを保存
        code_filename = f"{DATA_DIR}/kb16_mapping_code_{self.timestamp}.cpp"
        with open(code_filename, 'w') as f:
            f.write(cpp_code)
        
        print(f"\nマッピングコードを生成しました: {code_filename}")
        print("\n以下のコードをEspUsbHost.cppに貼り付けて使用できます:")
        print("-" * 60)
        print(cpp_code)
        print("-" * 60)
    
    def cleanup(self):
        """リソースのクリーンアップ"""
        if self.device and self.interface_number is not None:
            try:
                # インターフェースの解放を試みる
                usb.util.release_interface(self.device, self.interface_number)
                # Linuxの場合はドライバを再アタッチ
                try:
                    self.device.attach_kernel_driver(self.interface_number)
                except:
                    pass
            except:
                pass

def main():
    parser = argparse.ArgumentParser(description="DOIO KB16 キーボードマッピングキャリブレーションツール")
    parser.add_argument('-d', '--debug', action='store_true', help='デバッグモードを有効化')
    parser.add_argument('-i', '--interface', type=int, help='使用するHIDインターフェース番号を指定')
    parser.add_argument('-e', '--endpoint', type=str, help='使用するエンドポイントアドレスを16進数で指定 (例: 0x81)')
    args = parser.parse_args()
    
    # デバッグモード設定
    if args.debug:
        import logging
        logging.basicConfig(level=logging.DEBUG)
        usb.core._logger.setLevel(logging.DEBUG)
        print("デバッグモードが有効です")

    calibrator = KB16Calibrator()
    
    # コマンドラインでインターフェースとエンドポイントが指定された場合
    if args.interface is not None:
        print(f"インターフェース {args.interface} を指定して実行します")
        # この情報はfind_device内で使用する
        calibrator.target_interface = args.interface
    
    if args.endpoint is not None:
        print(f"エンドポイント {args.endpoint} を指定して実行します")
        # この情報はfind_device内で使用する
        calibrator.target_endpoint = int(args.endpoint, 16)
        
    calibrator.collect_data()

if __name__ == "__main__":
    main()
