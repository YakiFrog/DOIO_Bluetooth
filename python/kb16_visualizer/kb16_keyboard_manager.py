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
                 callback=None, auto_connect=False, debug=False):
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
        debug : bool
            デバッグ情報を表示するかどうか
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
        self.debug = debug
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
            
            # QMK Firmwareのinfo.jsonに基づくマッピング
            # キーマトリックスマッピング（以下のレイアウトを想定）
            # [0,0] [0,1] [0,2] [0,3]
            # [1,0] [1,1] [1,2] [1,3]
            # [2,0] [2,1] [2,2] [2,3]
            # [3,0] [3,1] [3,2] [3,3]
            
            # キーのマトリクス位置は次のように割り当てられています
            # a b c d
            # e f g h
            # i j k l
            # m n o p
            
            # QMKのマトリクス位置に基づいて、解析結果の頻度の高いビットを手動でマッピング
            key_mapping = [
                # QMKマトリクス位置, キーID
                (0, 0, 1),   # 左上
                (0, 1, 2),
                (0, 2, 3),
                (0, 3, 4),   # 右上
                
                (1, 0, 5),   # 2段目左
                (1, 1, 6),
                (1, 2, 7),
                (1, 3, 8),   # 2段目右
                
                (2, 0, 9),   # 3段目左
                (2, 1, 10),
                (2, 2, 11),
                (2, 3, 12),  # 3段目右
                
                (3, 0, 13),  # 左下
                (3, 1, 14),
                (3, 2, 15),
                (3, 3, 16),  # 右下
            ]
            
            # キーマトリックスを確認
            if len(report) >= 6:  # レポートの長さを確認（少なくとも6バイト必要）
                # キーマトリクス位置からHIDレポートのバイト/ビットを手動マッピング
                # キーは a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p という順番で割り振られている
                matrix_to_bytes = {
                    # 最上段の修正 - QMKマトリクスの[0,0]～[0,3] (a,b,c,d)
                    (0, 0): (3, 0),  # マトリクス[0,0] -> byte3_bit0に変更 (キーa)
                    (0, 1): (3, 1),  # マトリクス[0,1] -> byte3_bit1に変更 (キーb)
                    (0, 2): (3, 2),  # マトリクス[0,2] -> byte3_bit2に変更 (キーc)
                    (0, 3): (3, 3),  # マトリクス[0,3] -> byte3_bit3に変更 (キーd)
                    
                    # 2段目の修正 - QMKマトリクスの[1,0]～[1,3]
                    (1, 0): (3, 4),  # マトリクス[1,0] -> byte3_bit4に変更
                    (1, 1): (3, 5),  # マトリクス[1,1] -> byte3_bit5に変更
                    (1, 2): (3, 6),  # マトリクス[1,2] -> byte3_bit6に変更
                    (1, 3): (3, 7),  # マトリクス[1,3] -> byte3_bit7に変更
                    
                    (2, 0): (4, 1),  # マトリクス[2,0] -> byte4_bit1
                    (2, 1): (4, 5),  # マトリクス[2,1] -> byte4_bit5
                    (2, 2): (5, 3),  # マトリクス[2,2] -> byte5_bit3
                    (2, 3): (4, 6),  # マトリクス[2,3] -> byte4_bit6
                    
                    # 最下段の修正 - QMKマトリクスの[3,0]～[3,3] (m,n,o,p)
                    (3, 0): (0, 4),  # マトリクス[3,0] -> byte0_bit4に変更 (キーm)
                    (3, 1): (0, 5),  # マトリクス[3,1] -> byte0_bit5に変更 (キーn)
                    (3, 2): (0, 6),  # マトリクス[3,2] -> byte0_bit6に変更 (キーo)
                    (3, 3): (0, 7),  # マトリクス[3,3] -> byte0_bit7に変更 (キーp)
                }
                
                # マトリクス位置を使って状態を更新
                # デバッグ情報のために生のレポートデータを出力
                if self.debug and len(report) >= 10:
                    debug_report = " ".join([f"{b:02x}" for b in report[:12]])
                    bit_info = []
                    # 重要なバイトのビット表示
                    # バイト0が4行目(mnop)の状態を持っている可能性がある
                    if len(report) >= 1:
                        bits0 = format(report[0], '08b')
                        bit_info.append(f"byte0(mnop)={bits0}")
                    # バイト2の情報も参考として表示
                    if len(report) >= 3:
                        bits2 = format(report[2], '08b')
                        bit_info.append(f"byte2={bits2}")
                    # バイト3が1行目と2行目の状態を持っている
                    if len(report) >= 4:
                        bits3 = format(report[3], '08b')
                        bit_info.append(f"byte3(abcd/efgh)={bits3}")
                    # バイト4/5が3行目の状態の一部を持っている
                    if len(report) >= 6:
                        bits4 = format(report[4], '08b')
                        bits5 = format(report[5], '08b')
                        bit_info.append(f"byte4/5(ijkl)={bits4}/{bits5}")
                    
                    if self.report_counter % 20 == 0:  # 20回に1回のみ表示（少し間引く）
                        print(f"Raw report: {debug_report}")
                        print(f"Bit info: {' | '.join(bit_info)}")
                
                for matrix_pos, key_id in [(pos[:2], pos[2]) for pos in key_mapping]:
                    if matrix_pos in matrix_to_bytes:
                        byte_idx, bit_pos = matrix_to_bytes[matrix_pos]
                        if byte_idx < len(report):
                            key_state = bool(report[byte_idx] & (1 << bit_pos))
                            
                            # キー状態の更新（変更がある場合のみ）
                            if self.key_states.get(key_id) != key_state:
                                # デバッグ情報（キー状態が変化したときのみ）
                                if self.debug:
                                    print(f"Key {key_id} (マトリクス[{matrix_pos[0]},{matrix_pos[1]}]): byte{byte_idx}_bit{bit_pos}={key_state}")
                                
                                # 状態を更新
                                self.key_states[key_id] = key_state
                                updated_keys[key_id] = key_state
            
            # エンコーダー（ノブ）の状態を処理
            # QMKファームウェアから、3つのエンコーダーが存在することを確認
            if len(report) >= 12:
                # QMK info.jsonを参考に、ノブの配置は次の通り:
                # Encoder 1 (0, 4): Play
                # Encoder 2 (1, 4): TO1
                # Encoder 3 (2, 4): Mute
                
                # エンコーダーの値変化を修正
                encoder_value_mapping = {
                    1: 7,   # エンコーダー1の値はreport[7]に変更
                    2: 8,   # エンコーダー2の値はreport[8]に変更
                    3: 9,   # エンコーダー3の値はreport[9]に変更
                }
                
                # ノブの回転状態を処理
                for encoder_id, byte_idx in encoder_value_mapping.items():
                    if byte_idx < len(report) and report[byte_idx] != self.encoder_states.get(encoder_id):
                        old_value = self.encoder_states.get(encoder_id, 0)
                        self.encoder_states[encoder_id] = report[byte_idx]
                        # 0-255の値を0-100%にスケーリング
                        updated_encoders[encoder_id] = min(100, self.encoder_states[encoder_id] * 100 // 255)
                        
                        # デバッグ情報（エンコーダー値が変化したときのみ）
                        if self.debug:
                            print(f"Encoder {encoder_id} rotation: {old_value} -> {report[byte_idx]} ({updated_encoders[encoder_id]}%)")
                
                # エンコーダーのボタン状態を処理
                if len(report) >= 12:
                    # ボタンのビットマッピングをさらに調整
                    # QMKファイルからの情報に基づく
                    encoder_button_mapping = [
                        (1, 0, 1),  # ノブ1ボタン - byte1_bit0に変更
                        (1, 1, 2),  # ノブ2ボタン - byte1_bit1に変更
                        (1, 2, 3),  # ノブ3ボタン - byte1_bit2に変更
                    ]
                    
                    for byte_idx, bit_pos, encoder_id in encoder_button_mapping:
                        if byte_idx < len(report):
                            state = bool(report[byte_idx] & (1 << bit_pos))
                            if self.encoder_button_states.get(encoder_id) != state:
                                # デバッグ情報（エンコーダーボタン状態が変化したときのみ）
                                if self.debug:
                                    print(f"Encoder {encoder_id} Button: byte{byte_idx}_bit{bit_pos}={state}")
                                
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
