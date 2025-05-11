#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_visualizer/kb16_keyboard_manager.py
"""
DOIO KB16 キーボードマネージャー
キーボードの検出と接続、HIDレポートの処理を管理するクラス
"""

import sys
import os
import time
import threading
import platform
from collections import defaultdict
import datetime

# 親ディレクトリをPythonパスに追加して、既存のモジュールをインポートできるようにする
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import hid
except ImportError:
    print("hidapiがインストールされていません。pip install hidapiでインストールしてください。")
    sys.exit(1)

# DOIOのデフォルト定数
DEFAULT_VENDOR_ID = 0xD010
DEFAULT_PRODUCT_ID = 0x1601


class KeyboardNotFoundError(Exception):
    """キーボードが見つからなかった場合のエラー"""
    pass


class KeyboardManager:
    """DOIO KB16 キーボード管理クラス"""
    
    def __init__(self, vendor_id=DEFAULT_VENDOR_ID, product_id=DEFAULT_PRODUCT_ID, 
                 callback=None, auto_connect=False):
        """
        初期化
        
        Parameters:
        -----------
        vendor_id : int
            ベンダーID（デフォルト: 0xD010）
        product_id : int
            プロダクトID（デフォルト: 0x1601）
        callback : function
            HIDレポート受信時のコールバック関数
        auto_connect : bool
            初期化時に自動的にキーボードに接続するかどうか
        """
        self.vendor_id = vendor_id
        self.product_id = product_id
        self.callback = callback
        self.device = None
        self.running = False
        self.capture_thread = None
        self.key_states = {}
        self.encoder_states = {}
        self.encoder_button_states = {}
        self.debug = False
        self.last_report_time = None
        self.report_counter = 0
        
        # システム情報
        self.system_info = {
            'os': platform.system(),
            'release': platform.release(),
            'python': platform.python_version()
        }
        
        # 自動接続
        if auto_connect:
            try:
                self.connect()
            except KeyboardNotFoundError:
                print(f"キーボード (VID=0x{vendor_id:04X}, PID=0x{product_id:04X}) が見つかりませんでした")
    
    def set_callback(self, callback):
        """コールバック関数を設定"""
        self.callback = callback
    
    def list_devices(self):
        """利用可能なHIDデバイスをリストアップ"""
        devices = []
        
        try:
            all_devices = hid.enumerate()
            for device in all_devices:
                device_info = {
                    'vendor_id': device['vendor_id'],
                    'product_id': device['product_id'],
                    'path': device.get('path', b'').hex(),
                    'manufacturer': device.get('manufacturer_string', 'Unknown'),
                    'product': device.get('product_string', 'Unknown'),
                    'interface_number': device.get('interface_number', -1),
                    'usage_page': device.get('usage_page', 0),
                    'usage': device.get('usage', 0)
                }
                devices.append(device_info)
                
                # デバッグ出力
                if self.debug:
                    vid = device_info['vendor_id']
                    pid = device_info['product_id']
                    print(f"デバイス: VID=0x{vid:04X}, PID=0x{pid:04X}, "
                          f"製造元={device_info['manufacturer']}, 製品={device_info['product']}")
            
            return devices
        
        except Exception as e:
            print(f"デバイス列挙中にエラー: {e}")
            return []
    
    def find_device(self):
        """指定されたVID/PIDのデバイスを検索"""
        try:
            devices = hid.enumerate(self.vendor_id, self.product_id)
            if devices:
                return devices[0]
            return None
        except Exception as e:
            print(f"デバイス検索中にエラー: {e}")
            return None
    
    def connect(self):
        """キーボードに接続"""
        device_info = self.find_device()
        
        if not device_info:
            raise KeyboardNotFoundError(
                f"キーボード (VID=0x{self.vendor_id:04X}, PID=0x{self.product_id:04X}) が見つかりませんでした")
        
        try:
            self.device = hid.device()
            self.device.open(self.vendor_id, self.product_id)
            self.device.set_nonblocking(True)
            
            manufacturer = device_info.get('manufacturer_string', 'Unknown')
            product = device_info.get('product_string', 'Unknown')
            
            if self.debug:
                print(f"接続成功: {manufacturer} {product}")
            
            return True
        
        except Exception as e:
            print(f"接続エラー: {e}")
            self.device = None
            return False
    
    def start_capture(self):
        """キャプチャスレッドを開始"""
        if not self.device:
            try:
                self.connect()
            except Exception as e:
                print(f"キャプチャ開始エラー: {e}")
                return False
        
        if self.capture_thread and self.capture_thread.is_alive():
            print("キャプチャはすでに実行中です")
            return True
        
        self.running = True
        self.capture_thread = threading.Thread(target=self._capture_loop)
        self.capture_thread.daemon = True
        self.capture_thread.start()
        
        return True
    
    def stop_capture(self):
        """キャプチャを停止"""
        self.running = False
        if self.capture_thread and self.capture_thread.is_alive():
            self.capture_thread.join(1.0)
    
    def close(self):
        """デバイスを閉じる"""
        self.stop_capture()
        if self.device:
            try:
                self.device.close()
            except:
                pass
            self.device = None
    
    def _capture_loop(self):
        """キャプチャループ"""
        try:
            while self.running:
                report = self.device.read(64, timeout_ms=100)
                if report:
                    self.report_counter += 1
                    now = datetime.datetime.now()
                    
                    if self.debug and (not self.last_report_time or 
                                     (now - self.last_report_time).total_seconds() >= 1.0):
                        print(f"レポート数: {self.report_counter}")
                        self.report_counter = 0
                        self.last_report_time = now
                    
                    # レポートを処理
                    self._process_report(report)
                
                time.sleep(0.001)  # 短いスリープで負荷を軽減
        
        except Exception as e:
            print(f"キャプチャループエラー: {e}")
        finally:
            if self.debug:
                print("キャプチャループ終了")
    
    def _process_report(self, report):
        """HIDレポートを処理"""
        # レポートの長さが十分であることを確認
        if len(report) >= 8:
            updated_keys = {}
            updated_encoders = {}
            updated_encoder_buttons = {}
            
            # キーマトリックスの状態を処理
            for key_id in range(1, 17):  # 16キー
                key_byte = (key_id - 1) // 8
                key_bit = (key_id - 1) % 8
                
                if key_byte < len(report):
                    key_state = bool(report[key_byte] & (1 << key_bit))
                    
                    # 状態が変わった場合のみ更新
                    if self.key_states.get(key_id) != key_state:
                        self.key_states[key_id] = key_state
                        updated_keys[key_id] = key_state
            
            # エンコーダー（ノブ）の状態を処理
            if len(report) >= 10:
                # エンコーダー1
                if report[8] != self.encoder_states.get(1):
                    self.encoder_states[1] = report[8]
                    updated_encoders[1] = min(100, self.encoder_states[1] * 100 // 255)
                
                # エンコーダー2
                if len(report) >= 11 and report[9] != self.encoder_states.get(2):
                    self.encoder_states[2] = report[9]
                    updated_encoders[2] = min(100, self.encoder_states[2] * 100 // 255)
                
                # エンコーダー3
                if len(report) >= 12 and report[10] != self.encoder_states.get(3):
                    self.encoder_states[3] = report[10]
                    updated_encoders[3] = min(100, self.encoder_states[3] * 100 // 255)
                
                # エンコーダーのプッシュ状態
                if len(report) >= 13:
                    for encoder_id in range(1, 4):
                        encoder_bit = encoder_id - 1
                        state = bool(report[11] & (1 << encoder_bit))
                        if self.encoder_button_states.get(encoder_id) != state:
                            self.encoder_button_states[encoder_id] = state
                            updated_encoder_buttons[encoder_id] = state
            
            # コールバック関数がある場合は呼び出す
            if self.callback:
                self.callback(report, updated_keys, updated_encoders, updated_encoder_buttons)
    
    def get_key_states(self):
        """現在のキー状態の辞書を取得"""
        return self.key_states.copy()
    
    def get_encoder_states(self):
        """現在のエンコーダー状態の辞書を取得"""
        return self.encoder_states.copy()
    
    def get_encoder_button_states(self):
        """現在のエンコーダーボタン状態の辞書を取得"""
        return self.encoder_button_states.copy()


if __name__ == "__main__":
    # コマンドラインでのシンプルなテスト
    def report_callback(report, keys, encoders, encoder_buttons):
        if keys:
            key_info = ", ".join([f"キー{k}={'押' if s else '離'}" for k, s in keys.items()])
            print(f"キー状態変化: {key_info}")
        
        if encoders:
            enc_info = ", ".join([f"ノブ{e}={v}%" for e, v in encoders.items()])
            print(f"ノブ回転: {enc_info}")
        
        if encoder_buttons:
            btn_info = ", ".join([f"ノブ{b}ボタン={'押' if s else '離'}" for b, s in encoder_buttons.items()])
            print(f"ノブボタン: {btn_info}")
    
    print("DOIO KB16 キーボードマネージャーテスト")
    print("Ctrl+Cで終了します")
    
    try:
        manager = KeyboardManager(callback=report_callback, debug=True)
        devices = manager.list_devices()
        print(f"検出されたHIDデバイス: {len(devices)}")
        
        if manager.connect():
            print("キーボードに接続しました。キャプチャを開始します...")
            manager.start_capture()
            
            # メインスレッドを継続させるためのループ
            while True:
                time.sleep(1)
        
    except KeyboardInterrupt:
        print("\nプログラムを終了します")
    except KeyboardNotFoundError as e:
        print(f"エラー: {e}")
    finally:
        if 'manager' in locals():
            manager.close()
