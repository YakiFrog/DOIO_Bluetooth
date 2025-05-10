#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_raw_input.py
# DOIO KB16キーボード接続テスト用簡易ツール
# 低レベルのHID通信を確立し、生データのみを表示

import hid
import time
import sys
import argparse
import binascii
import os
import platform

def main():
    parser = argparse.ArgumentParser(description='DOIO KB16 生データ取得ツール')
    parser.add_argument('--vid', help='ベンダーIDを16進数で指定 (例: 0xD010)', default="0xD010")
    parser.add_argument('--pid', help='プロダクトIDを16進数で指定 (例: 0x1601)', default="0x1601")
    parser.add_argument('--all-interfaces', action='store_true', 
                        help='全てのインターフェースに接続を試みる')
    args = parser.parse_args()
    
    # VID/PIDを16進数から変換
    vendor_id = int(args.vid, 16)
    product_id = int(args.pid, 16)
    
    print(f"DOIO KB16 生データ取得ツール")
    print(f"指定デバイス: VID=0x{vendor_id:04X}, PID=0x{product_id:04X}")
    
    print("システムの情報:")
    print(f"OS: {platform.system()} {platform.release()}")
    print(f"Python: {platform.python_version()}")
    
    # 全デバイスを列挙
    all_devices = hid.enumerate()
    
    # ターゲットデバイスのインターフェースリスト
    target_interfaces = []
    paths = []
    
    print("\n接続可能なデバイス一覧:")
    for idx, device in enumerate(all_devices):
        if device['vendor_id'] == vendor_id and device['product_id'] == product_id:
            interface = device.get('interface_number', -1)
            usage_page = device.get('usage_page', 0)
            usage = device.get('usage', 0)
            path = device.get('path', b'').hex()
            manufacturer = device.get('manufacturer_string', 'Unknown')
            product = device.get('product_string', 'Unknown')
            
            print(f"デバイス #{idx+1}: {manufacturer} {product}")
            print(f"  インターフェース: {interface}")
            print(f"  Usage Page: 0x{usage_page:04X}, Usage: 0x{usage:04X}")
            print(f"  Path: {path}")
            
            target_interfaces.append({
                'interface': interface,
                'path': device['path'] if 'path' in device else None,
                'usage_page': usage_page,
                'usage': usage
            })
            
            # パスを保存
            if 'path' in device:
                paths.append(device['path'])
    
    if not target_interfaces:
        print(f"\n指定されたVID=0x{vendor_id:04X}, PID=0x{product_id:04X}のデバイスが見つかりませんでした。")
        return
    
    # インターフェース番号ごとにデバイスのオープンを試みる
    successful_connections = []
    
    # まずパスで接続を試みる
    if not args.all_interfaces and paths:
        print("\n直接パスでデバイスへの接続を試みます...")
        for path in paths:
            try:
                h = hid.device()
                h.open_path(path)
                print(f"パス '{path.hex()}' で接続に成功しました！")
                
                successful_connections.append({
                    'device': h,
                    'path': path.hex(),
                    'method': 'path',
                    'interface': None  # パス接続では特定不可
                })
            except Exception as e:
                print(f"パス '{path.hex()}' での接続に失敗: {e}")
    
    # 全インターフェースを試すか、パス接続が失敗した場合
    if args.all_interfaces or not successful_connections:
        print("\n個別のインターフェースでの接続を試みます...")
        
        # macOSではインターフェース指定がサポートされていないため、直接VID/PIDで接続
        if platform.system() == "Darwin":  # macOS
            try:
                h = hid.device()
                h.open(vendor_id, product_id)
                print(f"macOS: VID/PIDでの一般接続に成功しました！")
                
                successful_connections.append({
                    'device': h,
                    'path': None,
                    'method': 'vid_pid',
                    'interface': None  # macOSでは指定不可
                })
            except Exception as e:
                print(f"macOS: VID/PIDでの接続に失敗: {e}")
        else:
            # Windowsまたはその他のOSでは、インターフェース番号を指定して接続
            for idx, interface_info in enumerate(target_interfaces):
                interface = interface_info['interface']
                try:
                    print(f"インターフェース {interface} への接続を試行中...")
                    h = hid.device()
                    
                    # デバイスを開く (Windowsではインターフェース番号を指定可能)
                    if interface_info['path']:
                        h.open_path(interface_info['path'])
                    else:
                        # インターフェース番号の指定は一部のプラットフォームでのみ有効
                        h.open(vendor_id, product_id)
                    
                    print(f"インターフェース {interface} への接続に成功!")
                    
                    successful_connections.append({
                        'device': h,
                        'interface': interface,
                        'method': 'interface',
                        'usage_page': interface_info['usage_page'],
                        'usage': interface_info['usage']
                    })
                    
                    # 全インターフェースを試す設定でなければ、ここで終了
                    if not args.all_interfaces:
                        break
                        
                except Exception as e:
                    print(f"インターフェース {interface} への接続に失敗: {e}")
    
    if not successful_connections:
        print("\nどのインターフェースにも接続できませんでした。")
        return
    
    print(f"\n成功した接続: {len(successful_connections)} 件")
    
    # 各接続からデータの読み取りを試みる
    for conn_idx, connection in enumerate(successful_connections):
        h = connection['device']
        interface = connection.get('interface', 'unknown')
        method = connection.get('method', 'unknown')
        
        try:
            manufacturer = h.get_manufacturer_string()
            product = h.get_product_string()
            serial = h.get_serial_number_string()
            print(f"\n=== 接続 #{conn_idx+1} (インターフェース: {interface}, 方式: {method}) ===")
            print(f"メーカー: {manufacturer}")
            print(f"製品名: {product}")
            print(f"シリアル番号: {serial}")
        except Exception as e:
            print(f"デバイス情報の取得に失敗: {e}")
        
        print("\nデータの取得を開始します。10秒間または Ctrl+C で終了します。")
        
        start_time = time.time()
        received_count = 0
        
        try:
            while time.time() - start_time < 10:  # 10秒間データを取得
                try:
                    # 非ブロッキングモードでデータ取得を試みる（タイムアウト100ms）
                    data = h.read(64, timeout_ms=100)
                    
                    if data:
                        received_count += 1
                        hex_data = ' '.join([f"{b:02X}" for b in data])
                        print(f"データ受信 #{received_count}: {hex_data}")
                        
                        # ASCII表示
                        ascii_data = ''.join([chr(b) if 32 <= b <= 126 else '.' for b in data])
                        print(f"ASCII: {ascii_data}")
                except Exception as e:
                    print(f"データ読み取りエラー: {e}")
                    break
        except KeyboardInterrupt:
            print("\n中断しました")
        
        print(f"合計 {received_count} 件のデータを受信しました")
        
        # Feature Reportの取得も試みる
        print("\nFeature Reportの取得を試みます...")
        for report_id in range(5):
            try:
                report = h.get_feature_report(report_id, 64)
                if report:
                    print(f"レポートID {report_id}: {' '.join([f'{b:02X}' for b in report])}")
            except Exception as e:
                print(f"レポートID {report_id} の取得に失敗: {e}")
        
        # 入力レポートの取得を試みる（テクニック）
        print("\n特殊技: 入力レポートの取得...")
        for report_id in range(5):
            try:
                # set_nonblockingでノンブロッキングモードに設定
                h.set_nonblocking(1)
                time.sleep(0.1)  # 少し待機
                
                # 短いデータを書き込みリクエスト
                try:
                    result = h.write([report_id, 0x00, 0x00])
                    print(f"書き込み: report_id={report_id}, 結果={result}")
                except:
                    pass
                
                # 即座に読み取り
                time.sleep(0.1)
                data = h.read(64)
                if data:
                    print(f"レポート応答: {' '.join([f'{b:02X}' for b in data])}")
            except Exception as e:
                print(f"レポートID {report_id} での特殊技失敗: {e}")
        
        # 接続を閉じる
        h.close()
    
    print("\nすべての接続をテストしました。プログラムを終了します。")

if __name__ == "__main__":
    main()
