import hid
import time
import sys
import argparse
import binascii

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

def main():
    # コマンドライン引数の処理を追加
    parser = argparse.ArgumentParser(description='DOIOキーボードのデータアナライザー')
    parser.add_argument('--vid', help='ベンダーIDを16進数で指定 (例: 0xD010)', default=None)
    parser.add_argument('--pid', help='プロダクトIDを16進数で指定 (例: 0x1601)', default=None)
    parser.add_argument('--interface', type=int, help='インターフェース番号を指定 (例: 2)', default=None)
    parser.add_argument('--raw', action='store_true', help='生のバイトデータをダンプ')
    args = parser.parse_args()
    
    # コマンドライン引数からVID/PIDが指定された場合は変換
    target_vid = int(args.vid, 16) if args.vid else None
    target_pid = int(args.pid, 16) if args.pid else None
    target_interface = args.interface
    
    print("DOIOキーボードのデータアナライザー v1.1を開始します")
    print("接続されているHIDデバイスを検索中...")
    
    # システム上のすべてのHIDデバイスを列挙
    all_devices = hid.enumerate()
    
    # 検出されたデバイス数をカウント
    device_count = 0
    target_device = None
    
    for device in all_devices:
        vendor_id = device['vendor_id']
        product_id = device['product_id']
        
        # VID/PIDとManufacturerとProduct名を表示
        manufacturer = device.get('manufacturer_string', 'Unknown')
        product = device.get('product_string', 'Unknown')
        
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
        
        # 実験的：特定レポートIDの特徴調査
        print("\nReport Descriptorの取得を試みます（サポートされていないデバイスもあります）...")
        try:
            # こちらはWindowsではサポートされないがmacOSでは動作する場合がある
            descriptor_data = h.get_feature_report(0, 64)
            if descriptor_data:
                print(f"Report Descriptor: {binascii.hexlify(bytes(descriptor_data)).decode()}")
        except Exception as e:
            print(f"Report Descriptorの取得に失敗: {str(e)}")
            
        print("\nデータの取得を開始します。Ctrl+Cで終了します。")
        print("キー入力イベントを待機中...\n")
        
        last_data = None
        
        while True:
            try:
                # データを待機（ブロッキング読み取り、最大1秒待機）
                data = h.read(64, timeout_ms=1000)
                
                if data:
                    # データが取得できた場合
                    if args.raw:
                        # 生データをバイトとして表示
                        binary_data = bytes(data)
                        print(f"Raw data ({len(binary_data)} bytes): {binascii.hexlify(binary_data).decode()}")
                    
                    # 16進数表示
                    hex_data = ' '.join([f"{byte:02X}" for byte in data])
                    print(f"受信データ ({len(data)} bytes): [{hex_data}]")
                    
                    # ASCII表示（印刷可能文字のみ）
                    ascii_data = ''.join([chr(byte) if 32 <= byte <= 126 else '.' for byte in data])
                    print(f"ASCII: {ascii_data}")
                    
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
                        print()  # 空行を挿入
                    
                    last_data = data
            except KeyboardInterrupt:
                print("\nプログラムを終了します")
                break
                
    except Exception as e:
        print(f"エラーが発生しました: {str(e)}")
    
    finally:
        try:
            if 'h' in locals() and h:
                h.close()
        except:
            pass

if __name__ == "__main__":
    main()