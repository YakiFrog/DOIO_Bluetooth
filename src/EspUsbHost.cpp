#include "EspUsbHost.h"
#include <class/hid/hid.h>

// HIDキーコード定義（TinyUSBライブラリから必要なものを抽出）
#define HID_KEY_1               0x1E
#define HID_KEY_2               0x1F
#define HID_KEY_3               0x20
#define HID_KEY_4               0x21
#define HID_KEY_5               0x22
#define HID_KEY_6               0x23
#define HID_KEY_7               0x24
#define HID_KEY_8               0x25
#define HID_KEY_9               0x26
#define HID_KEY_0               0x27
#define HID_KEY_ENTER           0x28
#define HID_KEY_ESCAPE          0x29
#define HID_KEY_BACKSPACE       0x2A
#define HID_KEY_TAB             0x2B
#define HID_KEY_SPACE           0x2C
#define HID_KEY_RIGHT_ALT       0xE6

// DOIO KB16 キーボード用マッピング
// キーボードマトリックスマッピング構造体
struct KeyMapping {
    uint8_t byte_idx;  // レポート内のバイトインデックス
    uint8_t bit_mask;  // ビットマスク (1 << bit)
    uint8_t row;       // キーボードマトリックス行
    uint8_t col;       // キーボードマトリックス列
};

// DOIO KB16キーマッピング
const KeyMapping kb16_key_map[] = {
    { 5, 0x20, 0, 0 },  // byte5_bit5, 変化回数: 80
    { 1, 0x01, 0, 1 },  // byte1_bit0, 変化回数: 78
    { 1, 0x02, 0, 2 },  // byte1_bit1, 変化回数: 44
    { 5, 0x01, 0, 3 },  // byte5_bit0, 変化回数: 38
    { 4, 0x01, 1, 0 },  // byte4_bit0, 変化回数: 36
    { 5, 0x02, 1, 1 },  // byte5_bit1, 変化回数: 33
    { 4, 0x08, 1, 2 },  // byte4_bit3, 変化回数: 24
    { 4, 0x80, 1, 3 },  // byte4_bit7, 変化回数: 24
    { 4, 0x02, 2, 0 },  // byte4_bit1, 変化回数: 24
    { 4, 0x20, 2, 1 },  // byte4_bit5, 変化回数: 24
    { 5, 0x08, 2, 2 },  // byte5_bit3, 変化回数: 24
    { 4, 0x40, 2, 3 },  // byte4_bit6, 変化回数: 22
    { 4, 0x10, 3, 0 },  // byte4_bit4, 変化回数: 20
    { 5, 0x10, 3, 1 },  // byte5_bit4, 変化回数: 20
    { 4, 0x04, 3, 2 },  // byte4_bit2, 変化回数: 18
    { 5, 0x04, 3, 3 },  // byte5_bit2, 変化回数: 18
};

/**
 * PCAPフォーマットでUSB通信データをテキスト出力する関数
 * 
 * @param title タイトル（出力の説明）
 * @param function USB機能コード
 * @param direction データの方向（0:OUT 1:IN）
 * @param endpoint エンドポイント番号
 * @param type 転送タイプ
 * @param size データサイズ
 * @param stage 転送ステージ
 * @param data 実際のデータバッファ
 */
void EspUsbHost::_printPcapText(const char *title, uint16_t function, uint8_t direction, uint8_t endpoint, uint8_t type, uint8_t size, uint8_t stage, const uint8_t *data) {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
  uint8_t urbsize = 0x1c;
  if (stage == 0xff) {
    urbsize = 0x1b;
  }

  String data_str = "";
  for (int i = 0; i < size; i++) {
    if (data[i] < 16) {
      data_str += "0";
    }
    data_str += String(data[i], HEX) + " ";
  }

  printf("\n");
  printf("[PCAP TEXT]%s\n", title);
  printf("0000  %02x 00 00 00 00 00 00 00 00 00 00 00 00 00 %02x %02x\n", urbsize, (function & 0xff), ((function >> 8) & 0xff));
  printf("0010  %02x 01 00 01 00 %02x %02x %02x 00 00 00", direction, endpoint, type, size);
  if (stage != 0xff) {
    printf(" %02x\n", stage);
  } else {
    printf("\n");
  }
  printf("00%02x  %s\n", urbsize, data_str.c_str());
  printf("\n");
#endif
}

/**
 * USBホスト機能の初期化を行うメソッド
 * USBスタックのインストール、クライアント登録を実施
 */
void EspUsbHost::begin(void) {
  // 転送領域の初期化
  usbTransferSize = 0;

  // USBホスト設定構造体
  const usb_host_config_t config = {
    .skip_phy_setup = false,  // PHYセットアップを行う
    .intr_flags = ESP_INTR_FLAG_LEVEL1,  // 割り込み優先度
  };
  esp_err_t err = usb_host_install(&config);
  if (err != ESP_OK) {
    ESP_LOGI("EspUsbHost", "usb_host_install() err=%x", err);
  } else {
    ESP_LOGI("EspUsbHost", "usb_host_install() ESP_OK");
  }
  // USBホストクライアント設定構造体
  const usb_host_client_config_t client_config = {
    .is_synchronous = true,             // 同期モードで動作
    .max_num_event_msg = 10,            // イベントメッセージの最大数
    .async = {
      .client_event_callback = this->_clientEventCallback,  // コールバック関数の設定
      .callback_arg = this,                                 // コールバック関数の引数（このオブジェクト）
    }
  };
  err = usb_host_client_register(&client_config, &this->clientHandle);
  if (err != ESP_OK) {
    ESP_LOGI("EspUsbHost", "usb_host_client_register() err=%x", err);
  } else {
    ESP_LOGI("EspUsbHost", "usb_host_client_register() ESP_OK");
  }
}

/**
 * USBホストクライアントイベントのコールバック関数
 * デバイスの接続・切断時に呼び出される
 * 
 * @param eventMsg イベントメッセージ
 * @param arg コールバック引数（EspUsbHostオブジェクト）
 */
void EspUsbHost::_clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg) {
  // 引数からEspUsbHostオブジェクトを取得
  EspUsbHost *usbHost = (EspUsbHost *)arg;

  esp_err_t err;  switch (eventMsg->event) {
    // 新しいUSBデバイスが接続された場合
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      // 新規デバイスのアドレスをログ出力
      // アドレスは1-127の範囲の値で、USBホストによってデバイスに割り当てられる識別子
      ESP_LOGI("EspUsbHost", "USB_HOST_CLIENT_EVENT_NEW_DEV new_dev.address=%d", eventMsg->new_dev.address);
      
      // USBデバイスをオープンする
      // デバイスを操作するためのハンドルを取得
      err = usb_host_device_open(usbHost->clientHandle, eventMsg->new_dev.address, &usbHost->deviceHandle);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_device_open() err=%x", err);
      } else {
        ESP_LOGI("EspUsbHost", "usb_host_device_open() ESP_OK");
      }
        // デバイス情報を取得
      usb_device_info_t dev_info;
      err = usb_host_device_info(usbHost->deviceHandle, &dev_info);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_device_info() err=%x", err);
      } else {
        // 取得したデバイス情報をログに出力
        ESP_LOGI("EspUsbHost", "usb_host_device_info() ESP_OK\n"
                               "# speed                 = %d\n"  // デバイス速度（1=低速、2=全速、3=高速）
                               "# dev_addr              = %d\n"  // デバイスアドレス
                               "# vMaxPacketSize0       = %d\n"  // コントロールエンドポイントの最大パケットサイズ
                               "# bConfigurationValue   = %d\n"  // 現在のコンフィギュレーション値
                               "# str_desc_manufacturer = \"%s\"\n"  // 製造元名
                               "# str_desc_product      = \"%s\"\n"  // 製品名
                               "# str_desc_serial_num   = \"%s\"",   // シリアル番号
                 dev_info.speed,
                 dev_info.dev_addr,
                 dev_info.bMaxPacketSize0,
                 dev_info.bConfigurationValue,
                 getUsbDescString(dev_info.str_desc_manufacturer).c_str(),
                 getUsbDescString(dev_info.str_desc_product).c_str(),
                 getUsbDescString(dev_info.str_desc_serial_num).c_str());        // デバイス情報を子クラスに通知（派生クラスで実装するコールバック）
        usbHost->onNewDevice(dev_info);
      }      // デバイスディスクリプタ（基本情報）を取得
      // デバイスディスクリプタにはベンダーID、製品ID、USBバージョンなどの基本情報が含まれる
      const usb_device_desc_t *dev_desc;
      err = usb_host_get_device_descriptor(usbHost->deviceHandle, &dev_desc);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_get_device_descriptor() err=%x", err);
      } else {
        // 製造元と製品名を出力
        ESP_LOGI("EspUsbHost", "製造元: %s, 製品: %s", 
                getUsbDescString(dev_info.str_desc_manufacturer).c_str(), 
                getUsbDescString(dev_info.str_desc_product).c_str());
        // デバイスディスクリプタリクエストのセットアップパケット
        // 0x80: デバイスからホストへの転送（IN）
        // 0x06: GET_DESCRIPTOR リクエスト
        // 0x01: DEVICE ディスクリプタ
        // 0x12, 0x00: 要求サイズ (18バイト)
        const uint8_t setup[8] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00 };
        // PCAP形式でリクエストとレスポンスを記録（デバッグ・解析用）
        _printPcapText("GET DESCRIPTOR Request DEVICE", 0x000b, 0x00, 0x80, 0x02, sizeof(setup), 0x00, setup);
        _printPcapText("GET DESCRIPTOR Response DEVICE", 0x0008, 0x01, 0x80, 0x02, sizeof(usb_device_desc_t), 0x03, (const uint8_t *)dev_desc);        ESP_LOGI("EspUsbHost", "usb_host_get_device_descriptor() ESP_OK\n"
                               "#### DESCRIPTOR DEVICE ####\n"
                               "# bLength            = %d\n"  // ディスクリプタの長さ（通常18バイト）
                               "# bDescriptorType    = %d\n"  // ディスクリプタタイプ（1=デバイス）
                               "# bcdUSB             = 0x%x\n"  // 対応するUSB仕様のバージョン（0x0200=USB2.0）
                               "# bDeviceClass       = 0x%x\n"  // デバイスクラスコード
                               "# bDeviceSubClass    = 0x%x\n"  // デバイスサブクラスコード
                               "# bDeviceProtocol    = 0x%x\n"  // デバイスプロトコルコード
                               "# bMaxPacketSize0    = %d\n"  // エンドポイント0の最大パケットサイズ
                               "# idVendor           = 0x%x\n"  // ベンダーID
                               "# idProduct          = 0x%x\n"  // 製品ID
                               "# bcdDevice          = 0x%x\n"  // デバイスのリリースナンバー
                               "# iManufacturer      = %d\n"  // 製造元文字列のインデックス
                               "# iProduct           = %d\n"  // 製品名文字列のインデックス
                               "# iSerialNumber      = %d\n"  // シリアル番号文字列のインデックス
                               "# bNumConfigurations = %d",   // 使用可能な設定数
                 dev_desc->bLength,
                 dev_desc->bDescriptorType,
                 dev_desc->bcdUSB,
                 dev_desc->bDeviceClass,
                 dev_desc->bDeviceSubClass,
                 dev_desc->bDeviceProtocol,                 dev_desc->bMaxPacketSize0,
                 dev_desc->idVendor,
                 dev_desc->idProduct,
                 dev_desc->bcdDevice,
                 dev_desc->iManufacturer,
                 dev_desc->iProduct,
                 dev_desc->iSerialNumber,
                 dev_desc->bNumConfigurations);
                 
        // デバイスのベンダーIDと製品IDを保存
        usbHost->device_vendor_id = dev_desc->idVendor;
        usbHost->device_product_id = dev_desc->idProduct;
      }      // アクティブなコンフィグレーションディスクリプタ（設定情報）を取得
      // コンフィグレーションディスクリプタにはデバイスの電源要件やインターフェース数などの情報が含まれる
      const usb_config_desc_t *config_desc;
      err = usb_host_get_active_config_descriptor(usbHost->deviceHandle, &config_desc);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_get_active_config_descriptor() err=%x", err);
      } else {
        // コンフィグレーションディスクリプタリクエストのセットアップパケット
        // 0x80: デバイスからホストへの転送（IN）
        // 0x06: GET_DESCRIPTOR リクエスト
        // 0x02: CONFIGURATION ディスクリプタ
        // 0x09, 0x00: 要求サイズ (9バイト、コンフィグディスクリプタのヘッダ部分)
        const uint8_t setup[8] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0x09, 0x00 };
        // PCAP形式でリクエストとレスポンスを記録（デバッグ・解析用）
        _printPcapText("GET DESCRIPTOR Request CONFIGURATION", 0x000b, 0x00, 0x80, 0x02, sizeof(setup), 0x00, setup);
        _printPcapText("GET DESCRIPTOR Response CONFIGURATION", 0x0008, 0x01, 0x80, 0x02, sizeof(usb_config_desc_t), 0x03, (const uint8_t *)config_desc);

        ESP_LOGI("EspUsbHost", "usb_host_get_active_config_descriptor() ESP_OK\n"
                               "# bLength             = %d\n"
                               "# bDescriptorType     = %d\n"
                               "# wTotalLength        = %d\n"
                               "# bNumInterfaces      = %d\n"
                               "# bConfigurationValue = %d\n"
                               "# iConfiguration      = %d\n"
                               "# bmAttributes        = 0x%x\n"
                               "# bMaxPower           = %dmA",
                 config_desc->bLength,
                 config_desc->bDescriptorType,
                 config_desc->wTotalLength,
                 config_desc->bNumInterfaces,
                 config_desc->bConfigurationValue,
                 config_desc->iConfiguration,
                 config_desc->bmAttributes,
                 config_desc->bMaxPower * 2);
      }      // コンフィグレーションディスクリプタを処理するためのコールバックを呼び出す
      // このコールバックで各種ディスクリプタ（インターフェース、エンドポイントなど）を解析
      usbHost->_configCallback(config_desc);
      break;
      
    // USBデバイスが切断された場合
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      {
        ESP_LOGI("EspUsbHost", "USB_HOST_CLIENT_EVENT_DEV_GONE dev_gone.dev_hdl=%x", eventMsg->dev_gone.dev_hdl);
        
        // 登録済みの全転送を解放する処理
        // 接続解除時には全てのリソースを適切に解放する必要がある
        for (int i = 0; i < usbHost->usbTransferSize; i++) {
          // NULLポインタは処理をスキップ
          if (usbHost->usbTransfer[i] == NULL) {
            continue;
          }

          // エンドポイントのクリア（転送を中止して状態をリセット）
          err = usb_host_endpoint_clear(eventMsg->dev_gone.dev_hdl, usbHost->usbTransfer[i]->bEndpointAddress);
          if (err != ESP_OK) {
            ESP_LOGI("EspUsbHost", "usb_host_endpoint_clear() err=%x, dev_hdl=%x, bEndpointAddress=%x", err, eventMsg->dev_gone.dev_hdl, usbHost->usbTransfer[i]->bEndpointAddress);
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_endpoint_clear() ESP_OK, dev_hdl=%x, bEndpointAddress=%x", eventMsg->dev_gone.dev_hdl, usbHost->usbTransfer[i]->bEndpointAddress);
          }

          err = usb_host_transfer_free(usbHost->usbTransfer[i]);
          if (err != ESP_OK) {
            ESP_LOGI("EspUsbHost", "usb_host_transfer_free() err=%x, err, usbTransfer=%x", err, usbHost->usbTransfer[i]);
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_transfer_free() ESP_OK, usbTransfer=%x", usbHost->usbTransfer[i]);
          }

          usbHost->usbTransfer[i] = NULL;
        }
        usbHost->usbTransferSize = 0;        // 確保していたインターフェースをすべて解放する処理
        for (int i = 0; i < usbHost->usbInterfaceSize; i++) {
          // インターフェースの解放（claim_interfaceの反対操作）
          err = usb_host_interface_release(usbHost->clientHandle, usbHost->deviceHandle, usbHost->usbInterface[i]);
          if (err != ESP_OK) {
            ESP_LOGI("EspUsbHost", "usb_host_interface_release() err=%x, err, clientHandle=%x, deviceHandle=%x, Interface=%x", err, usbHost->clientHandle, usbHost->deviceHandle, usbHost->usbInterface[i]);
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_interface_release() ESP_OK, clientHandle=%x, deviceHandle=%x, Interface=%x", usbHost->clientHandle, usbHost->deviceHandle, usbHost->usbInterface[i]);
          }

          usbHost->usbInterface[i] = 0;
        }
        usbHost->usbInterfaceSize = 0;        // デバイスを閉じる
        usb_host_device_close(usbHost->clientHandle, usbHost->deviceHandle);

        // デバイスが切断されたことを子クラスに通知
        usbHost->onGone(eventMsg);
      }
      break;    // その他のイベント
    default:
      ESP_LOGI("EspUsbHost", "clientEventCallback() default %d", eventMsg->event);
      break;
  }
}

/**
 * USB構成ディスクリプタのコールバック関数
 * 構成ディスクリプタを解析し、含まれる各ディスクリプタを処理する
 * 
 * @param config_desc 構成ディスクリプタへのポインタ
 */
void EspUsbHost::_configCallback(const usb_config_desc_t *config_desc) {
  // 構成ディスクリプタの最初の値へのポインタを取得
  const uint8_t *p = &config_desc->val[0];
  uint8_t bLength;

  // PCAPフォーマットでディスクリプタリクエストとレスポンスを記録
  const uint8_t setup[8] = { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, (uint8_t)config_desc->wTotalLength, 0x00 };
  _printPcapText("GET DESCRIPTOR Request CONFIGURATION", 0x000b, 0x00, 0x80, 0x02, sizeof(setup), 0x00, setup);
  _printPcapText("GET DESCRIPTOR Response CONFIGURATION", 0x0008, 0x01, 0x80, 0x02, config_desc->wTotalLength, 0x03, (const uint8_t *)config_desc);

  // 構成ディスクリプタ内のすべてのディスクリプタを順番に処理
  for (int i = 0; i < config_desc->wTotalLength; i += bLength, p += bLength) {
    bLength = *p; // 現在のディスクリプタの長さを取得
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1); // ディスクリプタの種類を取得
      this->onConfig(bDescriptorType, p);       // 各ディスクリプタタイプに応じた処理を実行
    } else {
      return; // 長さが範囲外の場合は処理を終了
    }
  }
}

/**
 * USBホストのタスク処理関数
 * 定期的に呼び出され、USBイベントの処理と転送のチェックを行う
 */
void EspUsbHost::task(void) {
  // USBホストライブラリのイベント処理（タイムアウト=1ms）
  esp_err_t err = usb_host_lib_handle_events(1, &this->eventFlags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI("EspUsbHost", "usb_host_lib_handle_events() err=%x eventFlags=%x", err, this->eventFlags);
  }

  // USBホストクライアントのイベント処理（タイムアウト=1ms）
  err = usb_host_client_handle_events(this->clientHandle, 1);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI("EspUsbHost", "usb_host_client_handle_events() err=%x", err);
  }

  // デバイスが準備完了状態の場合、定期的に転送をチェック
  if (this->isReady) {
    unsigned long now = millis();
    if ((now - this->lastCheck) > this->interval) {
      this->lastCheck = now;

      for (int i = 0; i < this->usbTransferSize; i++) {
        if (this->usbTransfer[i] == NULL) {
          continue;
        }

        esp_err_t err = usb_host_transfer_submit(this->usbTransfer[i]);
        if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
          //ESP_LOGI("EspUsbHost", "usb_host_transfer_submit() err=%x", err);
        }
      }
    }
  }
}

/**
 * USBディスクリプタ文字列を読み取る関数
 * USB文字列ディスクリプタからASCII文字列に変換する
 * 
 * @param str_desc 文字列ディスクリプタへのポインタ
 * @return 変換されたASCII文字列
 */
String EspUsbHost::getUsbDescString(const usb_str_desc_t *str_desc) {
  String str = "";
  // NULLポインタのチェック
  if (str_desc == NULL) {
    return str;
  }

  // USB文字列ディスクリプタはUTF-16フォーマット（2バイト/文字）
  // bLength/2で文字数を取得し、各文字をASCIIとして扱う
  for (int i = 0; i < str_desc->bLength / 2; i++) {
    // ASCII範囲外の文字はスキップ
    if (str_desc->wData[i] > 0xFF) {
      continue;
    }
    str += char(str_desc->wData[i]);
  }
  return str;
}

void EspUsbHost::onConfig(const uint8_t bDescriptorType, const uint8_t *p) {
  switch (bDescriptorType) {
    case USB_DEVICE_DESC:
      {
        ESP_LOGI("EspUsbHost", "USB_DEVICE_DESC(0x01)");
      }
      break;

    case USB_CONFIGURATION_DESC:
      {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
        const usb_config_desc_t *config_desc = (const usb_config_desc_t *)p;
        ESP_LOGI("EspUsbHost", "USB_CONFIGURATION_DESC(0x02)\n"
                               "# bLength             = %d\n"
                               "# bDescriptorType     = %d\n"
                               "# wTotalLength        = %d\n"
                               "# bNumInterfaces      = %d\n"
                               "# bConfigurationValue = %d\n"
                               "# iConfiguration      = %d\n"
                               "# bmAttributes        = 0x%x\n"
                               "# bMaxPower           = %dmA",
                 config_desc->bLength,
                 config_desc->bDescriptorType,
                 config_desc->wTotalLength,
                 config_desc->bNumInterfaces,
                 config_desc->bConfigurationValue,
                 config_desc->iConfiguration,
                 config_desc->bmAttributes,
                 config_desc->bMaxPower * 2);
#endif
      }
      break;

    case USB_STRING_DESC:
      {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
        const usb_standard_desc_t *desc = (const usb_standard_desc_t *)p;
        String data_str = "";
        for (int i = 0; i < (desc->bLength - 2); i++) {
          if (desc->val[i] < 16) {
            data_str += "0";
          }
          data_str += String(desc->val[i], HEX) + " ";
        }
        ESP_LOGI("EspUsbHost", "USB_STRING_DESC(0x03) bLength=%d, bDescriptorType=0x%x, data=[%s]",
                 desc->bLength,
                 desc->bDescriptorType,
                 data_str);
#endif
      }
      break;    case USB_INTERFACE_DESC:
      {
        // インターフェースディスクリプタの処理
        // これはデバイスが提供する機能（HID、オーディオ、ストレージなど）を定義
        const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
        ESP_LOGI("EspUsbHost", "USB_INTERFACE_DESC(0x04)\n"
                               "# bLength            = %d\n"
                               "# bDescriptorType    = %d\n"
                               "# bInterfaceNumber   = %d\n"  // インターフェース識別子
                               "# bAlternateSetting  = %d\n"  // 代替設定番号
                               "# bNumEndpoints      = %d\n"  // 使用するエンドポイント数
                               "# bInterfaceClass    = 0x%x\n"  // インターフェースクラス(HID=0x03など)
                               "# bInterfaceSubClass = 0x%x\n"  // サブクラス
                               "# bInterfaceProtocol = 0x%x\n"  // プロトコル(キーボード=0x01など)
                               "# iInterface         = %d",     // インターフェース名の文字列インデックス
                 intf->bLength,
                 intf->bDescriptorType,
                 intf->bInterfaceNumber,
                 intf->bAlternateSetting,
                 intf->bNumEndpoints,
                 intf->bInterfaceClass,
                 intf->bInterfaceSubClass,
                 intf->bInterfaceProtocol,
                 intf->iInterface);

        // インターフェースの確保（この後のエンドポイント操作に必要）
        this->claim_err = usb_host_interface_claim(this->clientHandle, this->deviceHandle, intf->bInterfaceNumber, intf->bAlternateSetting);
        if (this->claim_err != ESP_OK) {
          ESP_LOGI("EspUsbHost", "usb_host_interface_claim() err=%x", claim_err);
        } else {
          ESP_LOGI("EspUsbHost", "usb_host_interface_claim() ESP_OK");
          // 確保したインターフェースを記録
          this->usbInterface[this->usbInterfaceSize] = intf->bInterfaceNumber;
          this->usbInterfaceSize++;
          // インターフェース情報を保存（後でエンドポイント設定に使用）
          _bInterfaceNumber = intf->bInterfaceNumber;
          _bInterfaceClass = intf->bInterfaceClass;
          _bInterfaceSubClass = intf->bInterfaceSubClass;
          _bInterfaceProtocol = intf->bInterfaceProtocol;
        }
      }
      break;    case USB_ENDPOINT_DESC:
      {
        const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)p;
        // エンドポイントディスクリプタの情報をログ出力
        ESP_LOGI("EspUsbHost", "USB_ENDPOINT_DESC(0x05)\n"
                               "# bLength          = %d\n"
                               "# bDescriptorType  = %d\n"
                               "# bEndpointAddress = 0x%x(EndpointID=%d, Direction=%s)\n"
                               "# bmAttributes     = 0x%x(%s)\n"
                               "# wMaxPacketSize   = %d\n"
                               "# bInterval        = %d",
                 ep_desc->bLength,
                 ep_desc->bDescriptorType,
                 ep_desc->bEndpointAddress, USB_EP_DESC_GET_EP_NUM(ep_desc), USB_EP_DESC_GET_EP_DIR(ep_desc) ? "IN" : "OUT",
                 ep_desc->bmAttributes,
                 // 転送タイプをマスクで取得し、対応する名称に変換
                 (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_CONTROL ? "CTRL" : (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC ? "ISOC"
                                                                                                                      : (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK ? "BULK"
                                                                                                                      : (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT  ? "Interrupt"
                                                                                                                                                                                                                 : "",
                 ep_desc->wMaxPacketSize,
                 ep_desc->bInterval);

        // インターフェースの確保に失敗している場合は処理をスキップ
        if (this->claim_err != ESP_OK) {
          ESP_LOGI("EspUsbHost", "claim_err skip");
          return;
        }

        // エンドポイントデータリストにインターフェース情報を保存
        // 接続されたデバイスの各エンドポイントに関連するインターフェース情報を記録
        this->endpoint_data_list[USB_EP_DESC_GET_EP_NUM(ep_desc)].bInterfaceNumber = _bInterfaceNumber;
        this->endpoint_data_list[USB_EP_DESC_GET_EP_NUM(ep_desc)].bInterfaceClass = _bInterfaceClass;
        this->endpoint_data_list[USB_EP_DESC_GET_EP_NUM(ep_desc)].bInterfaceSubClass = _bInterfaceSubClass;
        this->endpoint_data_list[USB_EP_DESC_GET_EP_NUM(ep_desc)].bInterfaceProtocol = _bInterfaceProtocol;
        this->endpoint_data_list[USB_EP_DESC_GET_EP_NUM(ep_desc)].bCountryCode = _bCountryCode;        // 割り込み転送タイプ(Interrupt)のみ処理、それ以外はスキップ
        if ((ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_INT) {
          ESP_LOGI("EspUsbHost", "err ep_desc->bmAttributes=%x", ep_desc->bmAttributes);
          return;
        }

        // INエンドポイント（デバイスからホストへのデータ転送）の場合
        if (ep_desc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
          // 転送バッファを割り当て
          // 最大パケットサイズ+1バイトのバッファを確保
          esp_err_t err = usb_host_transfer_alloc(ep_desc->wMaxPacketSize + 1, 0, &this->usbTransfer[this->usbTransferSize]);
          if (err != ESP_OK) {
            this->usbTransfer[this->usbTransferSize] = NULL;
            ESP_LOGI("EspUsbHost", "usb_host_transfer_alloc() err=%x", err);
            return;
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_transfer_alloc() ESP_OK data_buffer_size=%d", ep_desc->wMaxPacketSize + 1);
          }

          // 転送構造体を設定
          this->usbTransfer[this->usbTransferSize]->device_handle = this->deviceHandle;      // デバイスハンドル
          this->usbTransfer[this->usbTransferSize]->bEndpointAddress = ep_desc->bEndpointAddress; // エンドポイントアドレス
          this->usbTransfer[this->usbTransferSize]->callback = this->_onReceive;             // 転送完了コールバック
          this->usbTransfer[this->usbTransferSize]->context = this;                          // コールバックコンテキスト
          this->usbTransfer[this->usbTransferSize]->num_bytes = ep_desc->wMaxPacketSize;     // 転送バイト数
          interval = ep_desc->bInterval;     // ポーリング間隔を設定
          isReady = true;                   // デバイス準備完了フラグをセット
          this->usbTransferSize++;          // 転送カウントを増やす
        }
      }
      break;    case USB_INTERFACE_ASSOC_DESC:
      {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
        // インターフェース関連付けディスクリプタの処理
        // 複数のインターフェースをグループ化するために使用される（複合デバイスなど）
        const usb_iad_desc_t *iad_desc = (const usb_iad_desc_t *)p;
        ESP_LOGI("EspUsbHost", "USB_INTERFACE_ASSOC_DESC(0x0b)\n"
                               "# bLength           = %d\n"
                               "# bDescriptorType   = %d\n"
                               "# bFirstInterface   = %d\n"  // グループの最初のインターフェース番号
                               "# bInterfaceCount   = %d\n"  // グループに含まれるインターフェース数
                               "# bFunctionClass    = 0x%x\n"  // グループ全体のクラスコード
                               "# bFunctionSubClass = 0x%x\n"  // グループ全体のサブクラス
                               "# bFunctionProtocol = 0x%x\n"  // グループ全体のプロトコル
                               "# iFunction         = %d",     // グループの説明文字列インデックス
                 iad_desc->bLength,
                 iad_desc->bDescriptorType,
                 iad_desc->bFirstInterface,
                 iad_desc->bInterfaceCount,
                 iad_desc->bFunctionClass,
                 iad_desc->bFunctionSubClass,
                 iad_desc->bFunctionProtocol,
                 iad_desc->iFunction);
#endif
      }
      break;    case USB_HID_DESC:
      {
        // HID（ヒューマンインターフェースデバイス）ディスクリプタの処理
        const tusb_hid_descriptor_hid_t *hid_desc = (const tusb_hid_descriptor_hid_t *)p;
        ESP_LOGI("EspUsbHost", "USB_HID_DESC(0x21)\n"
                               "# bLength         = %d\n"
                               "# bDescriptorType = 0x%x\n"
                               "# bcdHID          = 0x%x\n"  // HIDクラス仕様のバージョン
                               "# bCountryCode    = 0x%x\n"  // 国別コード（キーボードレイアウト等）
                               "# bNumDescriptors = %d\n"    // このディスクリプタ内の下位ディスクリプタ数
                               "# bReportType     = 0x%x\n"  // レポートディスクリプタタイプ
                               "# wReportLength   = %d",     // レポートディスクリプタ長
                 hid_desc->bLength,
                 hid_desc->bDescriptorType,
                 hid_desc->bcdHID,
                 hid_desc->bCountryCode,
                 hid_desc->bNumDescriptors,
                 hid_desc->bReportType,
                 hid_desc->wReportLength);
        _bCountryCode = hid_desc->bCountryCode;  // 国別コードを保存

        // HIDレポートディスクリプタをリクエスト
        // 0x81: デバイス宛（クラス固有）、0x00: インターフェース、0x22: レポートディスクリプタ
        submitControl(0x81, 0x00, 0x22, _bInterfaceNumber, hid_desc->wReportLength);
      }
      break;    default:
      {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
        // 未知/未サポートのディスクリプタタイプの処理
        // 内容を16進数でログに出力
        const usb_standard_desc_t *desc = (const usb_standard_desc_t *)p;
        String data_str = "";
        for (int i = 0; i < (desc->bLength - 2); i++) {
          if (desc->val[i] < 16) {
            data_str += "0";  // 16未満の値は先頭に0を追加
          }
          data_str += String(desc->val[i], HEX) + " ";
        }
        ESP_LOGI("EspUsbHost", "USB_???_DESC(%02x) bLength=%d, bDescriptorType=0x%x, data=[%s]",
                 bDescriptorType,
                 desc->bLength,
                 desc->bDescriptorType,
                 data_str);
#endif
      }
  }
}

void EspUsbHost::_onReceive(usb_transfer_t *transfer) {
  EspUsbHost *usbHost = (EspUsbHost *)transfer->context;
  endpoint_data_t *endpoint_data = &usbHost->endpoint_data_list[(transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK)];

#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE)
  _printPcapText("URB_INTERRUPT in", 0x0009, 0x01, transfer->bEndpointAddress, 0x01, transfer->actual_num_bytes, 0xff, (const uint8_t *)transfer->data_buffer);

  String buffer_str = "";
  for (int i = 0; i < transfer->actual_num_bytes; i++) {
    if (transfer->data_buffer[i] < 16) {
      buffer_str += "0";
    }
    buffer_str += String(transfer->data_buffer[i], HEX) + " ";
  }
  buffer_str.trim();
  ESP_LOGV("EspUsbHost", "transfer\n"
                         "# bInterfaceClass    = 0x%x\n"
                         "# bInterfaceSubClass = 0x%x\n"
                         "# bInterfaceProtocol = 0x%x\n"
                         "# bCountryCode       = 0x%x > usb_transfer_t data_buffer=[%s]\n"
                         "# data_buffer_size   = %d\n"
                         "# num_bytes          = %d\n"
                         "# actual_num_bytes   = %d\n"
                         "# flags              = 0x%x\n"
                         "# bEndpointAddress   = 0x%x\n"
                         "# timeout_ms         = %d\n"
                         "# num_isoc_packets   = %d",
           endpoint_data->bInterfaceClass,
           endpoint_data->bInterfaceSubClass,
           endpoint_data->bInterfaceProtocol,
           endpoint_data->bCountryCode,
           buffer_str.c_str(),
           transfer->data_buffer_size,
           transfer->num_bytes,
           transfer->actual_num_bytes,
           transfer->flags,
           transfer->bEndpointAddress,
           transfer->timeout_ms,
           transfer->num_isoc_packets);
#endif
  if (endpoint_data->bInterfaceClass == USB_CLASS_HID) {    // DOIO KB16 キーボード特別処理
    bool is_doio_kb16 = (usbHost->device_vendor_id == 0xD010 && usbHost->device_product_id == 0x1601);
    
    if (is_doio_kb16) {
      ESP_LOGI("KB16", "DOIO KB16からのデータ受信: %d バイト", transfer->actual_num_bytes);
      
      // レポート内容をログに出力
      if (transfer->actual_num_bytes > 0) {
        String data_str = "データ:";
        for (int i = 0; i < transfer->actual_num_bytes && i < 16; i++) {  // 最初の16バイトまで表示
          char buf[8];
          sprintf(buf, " %02X", transfer->data_buffer[i]);
          data_str += buf;
        }
        ESP_LOGI("KB16", "%s", data_str.c_str());
      }
      
      // HIDレポートとしてデータを処理
      static hid_keyboard_report_t last_report = {};
      hid_keyboard_report_t report = {};      
      // 標準のHIDキーボードレポート要素を埋める
      report.modifier = transfer->data_buffer[0];
      
      // reservedフィールドは使っていないので、ここにポインタは入れられない
      // 代わりにキーコード配列の最初の要素にデータバイトを入れる（識別用）
      report.reserved = 0xAA; // 特殊な値を設定して、DOIO KB16であることを表す
      
      // キーコード配列にデータをコピー
      for (int i = 0; i < sizeof(report.keycode) && i < transfer->actual_num_bytes; i++) {
        report.keycode[i] = transfer->data_buffer[i + 1]; // 1バイトずらして保存（最初のバイトはmodifier）
      }
        // キーボード処理コールバックを呼び出す
      usbHost->onKeyboard(report, last_report);
      
      // 前回の状態を保存
      memcpy(&last_report, &report, sizeof(last_report));
    }
    else if (endpoint_data->bInterfaceSubClass == HID_SUBCLASS_BOOT) {
      if (endpoint_data->bInterfaceProtocol == HID_ITF_PROTOCOL_KEYBOARD) {
        static hid_keyboard_report_t last_report = {};

        if (transfer->data_buffer[2] == HID_KEY_NUM_LOCK) {
          // HID_KEY_NUM_LOCK TODO!
        } else if (memcmp(&last_report, transfer->data_buffer, sizeof(last_report))) {
          // chenge
          hid_keyboard_report_t report = {};
          report.modifier = transfer->data_buffer[0];
          report.reserved = transfer->data_buffer[1];
          report.keycode[0] = transfer->data_buffer[2];
          report.keycode[1] = transfer->data_buffer[3];
          report.keycode[2] = transfer->data_buffer[4];
          report.keycode[3] = transfer->data_buffer[5];
          report.keycode[4] = transfer->data_buffer[6];
          report.keycode[5] = transfer->data_buffer[7];          usbHost->onKeyboard(report, last_report);

          bool shift = (report.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) || (report.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT);
          for (int i = 0; i < 6; i++) {
            if (report.keycode[i] != 0 && last_report.keycode[i] == 0) {
              // Type
              usbHost->onKeyboardKey(usbHost->getKeycodeToAscii(report.keycode[i], shift), report.keycode[i], shift);
            }
          }

          memcpy(&last_report, &report, sizeof(last_report));
        }
      } else if (endpoint_data->bInterfaceProtocol == HID_ITF_PROTOCOL_MOUSE) {
        static uint8_t last_buttons = 0;
        hid_mouse_report_t report = {};
        report.buttons = transfer->data_buffer[1];
        report.x = (uint8_t)transfer->data_buffer[2];
        report.y = (uint8_t)transfer->data_buffer[4];        report.wheel = (uint8_t)transfer->data_buffer[6];
        usbHost->onMouse(report, last_buttons);
        if (report.buttons != last_buttons) {
          usbHost->onMouseButtons(report, last_buttons);
          last_buttons = report.buttons;
        }
        if (report.x != 0 || report.y != 0 || report.wheel != 0) {
          usbHost->onMouseMove(report);
        }
      }
    } else {
      //this->_onReceiveGamepad();
    }
  }

  usbHost->onReceive(transfer);
}

void EspUsbHost::onMouse(hid_mouse_report_t report, uint8_t last_buttons) {
  ESP_LOGD("EspUsbHost", "last_buttons=0x%02x(%c%c%c%c%c), buttons=0x%02x(%c%c%c%c%c), x=%d, y=%d, wheel=%d",
           last_buttons,
           (last_buttons & MOUSE_BUTTON_LEFT) ? 'L' : ' ',
           (last_buttons & MOUSE_BUTTON_RIGHT) ? 'R' : ' ',
           (last_buttons & MOUSE_BUTTON_MIDDLE) ? 'M' : ' ',
           (last_buttons & MOUSE_BUTTON_BACKWARD) ? 'B' : ' ',
           (last_buttons & MOUSE_BUTTON_FORWARD) ? 'F' : ' ',
           report.buttons,
           (report.buttons & MOUSE_BUTTON_LEFT) ? 'L' : ' ',
           (report.buttons & MOUSE_BUTTON_RIGHT) ? 'R' : ' ',
           (report.buttons & MOUSE_BUTTON_MIDDLE) ? 'M' : ' ',
           (report.buttons & MOUSE_BUTTON_BACKWARD) ? 'B' : ' ',
           (report.buttons & MOUSE_BUTTON_FORWARD) ? 'F' : ' ',
           report.x,
           report.y,
           report.wheel);
}

void EspUsbHost::onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons) {
  ESP_LOGD("EspUsbHost", "last_buttons=0x%02x(%c%c%c%c%c), buttons=0x%02x(%c%c%c%c%c), x=%d, y=%d, wheel=%d",
           last_buttons,
           (last_buttons & MOUSE_BUTTON_LEFT) ? 'L' : ' ',
           (last_buttons & MOUSE_BUTTON_RIGHT) ? 'R' : ' ',
           (last_buttons & MOUSE_BUTTON_MIDDLE) ? 'M' : ' ',
           (last_buttons & MOUSE_BUTTON_BACKWARD) ? 'B' : ' ',
           (last_buttons & MOUSE_BUTTON_FORWARD) ? 'F' : ' ',
           report.buttons,
           (report.buttons & MOUSE_BUTTON_LEFT) ? 'L' : ' ',
           (report.buttons & MOUSE_BUTTON_RIGHT) ? 'R' : ' ',
           (report.buttons & MOUSE_BUTTON_MIDDLE) ? 'M' : ' ',
           (report.buttons & MOUSE_BUTTON_BACKWARD) ? 'B' : ' ',
           (report.buttons & MOUSE_BUTTON_FORWARD) ? 'F' : ' ',
           report.x,
           report.y,
           report.wheel);

  // LEFT
  if (!(last_buttons & MOUSE_BUTTON_LEFT) && (report.buttons & MOUSE_BUTTON_LEFT)) {
    ESP_LOGI("EspUsbHost", "Mouse LEFT Click");
  }
  if ((last_buttons & MOUSE_BUTTON_LEFT) && !(report.buttons & MOUSE_BUTTON_LEFT)) {
    ESP_LOGI("EspUsbHost", "Mouse LEFT Release");
  }

  // RIGHT
  if (!(last_buttons & MOUSE_BUTTON_RIGHT) && (report.buttons & MOUSE_BUTTON_RIGHT)) {
    ESP_LOGI("EspUsbHost", "Mouse RIGHT Click");
  }
  if ((last_buttons & MOUSE_BUTTON_RIGHT) && !(report.buttons & MOUSE_BUTTON_RIGHT)) {
    ESP_LOGI("EspUsbHost", "Mouse RIGHT Release");
  }

  // MIDDLE
  if (!(last_buttons & MOUSE_BUTTON_MIDDLE) && (report.buttons & MOUSE_BUTTON_MIDDLE)) {
    ESP_LOGI("EspUsbHost", "Mouse MIDDLE Click");
  }
  if ((last_buttons & MOUSE_BUTTON_MIDDLE) && !(report.buttons & MOUSE_BUTTON_MIDDLE)) {
    ESP_LOGI("EspUsbHost", "Mouse MIDDLE Release");
  }

  // BACKWARD
  if (!(last_buttons & MOUSE_BUTTON_BACKWARD) && (report.buttons & MOUSE_BUTTON_BACKWARD)) {
    ESP_LOGI("EspUsbHost", "Mouse BACKWARD Click");
  }
  if ((last_buttons & MOUSE_BUTTON_BACKWARD) && !(report.buttons & MOUSE_BUTTON_BACKWARD)) {
    ESP_LOGI("EspUsbHost", "Mouse BACKWARD Release");
  }

  // FORWARD
  if (!(last_buttons & MOUSE_BUTTON_FORWARD) && (report.buttons & MOUSE_BUTTON_FORWARD)) {
    ESP_LOGI("EspUsbHost", "Mouse FORWARD Click");
  }
  if ((last_buttons & MOUSE_BUTTON_FORWARD) && !(report.buttons & MOUSE_BUTTON_FORWARD)) {
    ESP_LOGI("EspUsbHost", "Mouse FORWARD Release");
  }
}

void EspUsbHost::onMouseMove(hid_mouse_report_t report) {
  ESP_LOGD("EspUsbHost", "buttons=0x%02x(%c%c%c%c%c), x=%d, y=%d, wheel=%d",
           report.buttons,
           (report.buttons & MOUSE_BUTTON_LEFT) ? 'L' : ' ',
           (report.buttons & MOUSE_BUTTON_RIGHT) ? 'R' : ' ',
           (report.buttons & MOUSE_BUTTON_MIDDLE) ? 'M' : ' ',
           (report.buttons & MOUSE_BUTTON_BACKWARD) ? 'B' : ' ',
           (report.buttons & MOUSE_BUTTON_FORWARD) ? 'F' : ' ',
           report.x,
           report.y,
           report.wheel);
}

void EspUsbHost::onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {
  ESP_LOGD("EspUsbHost", "modifier=[0x%02x]->[0x%02x], Key0=[0x%02x]->[0x%02x], Key1=[0x%02x]->[0x%02x], Key2=[0x%02x]->[0x%02x], Key3=[0x%02x]->[0x%02x], Key4=[0x%02x]->[0x%02x], Key5=[0x%02x]->[0x%02x]",
           last_report.modifier,
           report.modifier,
           last_report.keycode[0],
           report.keycode[0],
           last_report.keycode[1],
           report.keycode[1],
           last_report.keycode[2],
           report.keycode[2],
           last_report.keycode[3],
           report.keycode[3],
           last_report.keycode[4],
           report.keycode[4],
           last_report.keycode[5],
           report.keycode[5]);
  
  // DOIO KB16カスタムキー解析
  if (device_product_id == 0x1601 && device_vendor_id == 0xD010) {
    ESP_LOGI("KB16", "DOIO KB16キーボード処理 (VID=0x%04X, PID=0x%04X)", device_vendor_id, device_product_id);
    // キー状態が変化したことを示すフラグ
    static bool first_report = true;
    bool key_state_changed = false;
    
    // 特殊な値(0xAA)をチェック - DOIO KB16からのデータかどうかを確認
    if (report.reserved == 0xAA) {
      // レポートの詳細をログに出力（初回のみ全データ）
      if (first_report) {
        String full_data = "KB16レポート検出: ";
        full_data += String("modifier=") + String(report.modifier, HEX) + " ";
        
        for (int i = 0; i < 6; i++) {
          char buf[16];
          sprintf(buf, "key%d=0x%02X ", i, report.keycode[i]);
          full_data += buf;
        }
        
        ESP_LOGI("KB16", "%s", full_data.c_str());
        first_report = false;
      }
        // onKeyboard関数内ではキーコードデータからキーマトリックス状態を計算
      uint8_t kb16_data[32] = {0};
      uint8_t kb16_last_data[32] = {0};
      
      // キーコードフィールドからデータを取得
      // 基本的に最初の6バイトにデータが格納されている
      for (int i = 0; i < 6 && i < 32; i++) {
        kb16_data[i] = report.keycode[i];
        kb16_last_data[i] = last_report.keycode[i];
      }
      
      // 各キーマッピングをチェック
      for (int i = 0; i < sizeof(kb16_key_map) / sizeof(KeyMapping); i++) {
        const KeyMapping& mapping = kb16_key_map[i];
        
        // マッピング情報を一度表示（デバッグ用）
        ESP_LOGD("KB16", "マッピング[%d]: バイト=%d, ビット=0x%02X, 行=%d, 列=%d", 
                i, mapping.byte_idx, mapping.bit_mask, mapping.row, mapping.col);
        
        // バイトインデックスが範囲内かチェック
        if (mapping.byte_idx < 32) {
          uint8_t current_byte = kb16_data[mapping.byte_idx];
          uint8_t last_byte = kb16_last_data[mapping.byte_idx];
          
          bool current_state = (current_byte & mapping.bit_mask) != 0;
          bool last_state = (last_byte & mapping.bit_mask) != 0;
          
          // 状態が変化した場合
          if (current_state != last_state) {
            key_state_changed = true;
            ESP_LOGI("KB16", "キー (%d,%d) %s [バイト%d:0x%02X, ビット:0x%02X]", 
                    mapping.row, mapping.col, 
                    current_state ? "押下" : "解放",
                    mapping.byte_idx, current_byte, mapping.bit_mask);
            
            // キーマトリックス状態を更新
            updateKB16KeyState(mapping.row, mapping.col, current_state);
            
            // HIDキーコードに変換
            uint8_t hid_keycode = 0;
            
            // 行と列の組み合わせに基づいて特定のキーコードに変換
            if (mapping.row == 0 && mapping.col == 0) hid_keycode = HID_KEY_1; // 1キー
            else if (mapping.row == 0 && mapping.col == 1) hid_keycode = HID_KEY_2; // 2キー
            else if (mapping.row == 0 && mapping.col == 2) hid_keycode = HID_KEY_3; // 3キー
            else if (mapping.row == 0 && mapping.col == 3) hid_keycode = HID_KEY_4; // 4キー
            else if (mapping.row == 1 && mapping.col == 0) hid_keycode = HID_KEY_5; // 5キー
            else if (mapping.row == 1 && mapping.col == 1) hid_keycode = HID_KEY_6; // 6キー
            else if (mapping.row == 1 && mapping.col == 2) hid_keycode = HID_KEY_7; // 7キー
            else if (mapping.row == 1 && mapping.col == 3) hid_keycode = HID_KEY_8; // 8キー
            else if (mapping.row == 2 && mapping.col == 0) hid_keycode = HID_KEY_9; // 9キー
            else if (mapping.row == 2 && mapping.col == 1) hid_keycode = HID_KEY_0; // 0キー
            else if (mapping.row == 2 && mapping.col == 2) hid_keycode = HID_KEY_ENTER; // Enterキー
            else if (mapping.row == 2 && mapping.col == 3) hid_keycode = HID_KEY_ESCAPE; // Escキー
            else if (mapping.row == 3 && mapping.col == 0) hid_keycode = HID_KEY_BACKSPACE; // Backspaceキー
            else if (mapping.row == 3 && mapping.col == 1) hid_keycode = HID_KEY_TAB; // Tabキー
            else if (mapping.row == 3 && mapping.col == 2) hid_keycode = HID_KEY_SPACE; // Spaceキー
            else if (mapping.row == 3 && mapping.col == 3) hid_keycode = HID_KEY_RIGHT_ALT; // 右Altキー
            
            // 仮想的なキー入力を生成
            if (current_state) {
              // キーが押された場合の処理
              ESP_LOGI("KB16", "キー押下: HIDコード=0x%02X", hid_keycode);
              // ここで外部システムに通知する処理を追加
              // 例: BLEキーボードの場合はBLEで送信する
            } else {
              // キーが離された場合の処理
              ESP_LOGI("KB16", "キー解放: HIDコード=0x%02X", hid_keycode);
              // ここで外部システムに通知する処理を追加
            }
              // 派生クラスに通知
            onKB16KeyStateChanged(mapping.row, mapping.col, current_state);
          }
        } else {
          ESP_LOGW("KB16", "バイトインデックス範囲外: %d", mapping.byte_idx);
        }
      }
    } else {
      ESP_LOGW("KB16", "KB16データが無効です");
    }
    
    if (key_state_changed) {
      ESP_LOGI("KB16", "キー状態が変化しました");
      // キー状態が変化した場合の全体的な処理
      // 例: LEDの更新など
    }
  }
}

uint8_t EspUsbHost::getKeycodeToAscii(uint8_t keycode, uint8_t shift) {
  static uint8_t const keyboard_conv_table[128][2] = { HID_KEYCODE_TO_ASCII };
  static uint8_t const keyboard_conv_table_ja[128][2] = { HID_KEYCODE_TO_ASCII_JA };

  if (shift > 1) {
    shift = 1;
  }

  if (hidLocal == HID_LOCAL_Japan_Katakana) {
    // Japan
    return keyboard_conv_table_ja[keycode][shift];
  } else {
    // US
    return keyboard_conv_table[keycode][shift];
  }
}

void EspUsbHost::onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) {
  if (' ' <= ascii && ascii <= '~') {
    // printable
    ESP_LOGV("EspUsbHost", "Keyboard Type=0x%02x(%c), keycode=0x%02x, modifier=0x%02x",
             ascii,
             ascii,
             keycode,
             modifier);
  } else {
    ESP_LOGV("EspUsbHost", "Keyboard Type=0x%02x, keycode=0x%02x, modifier=0x%02x",
             ascii,
             keycode,
             modifier);
  }
}

void EspUsbHost::setHIDLocal(hid_local_enum_t code) {
  hidLocal = code;
}

esp_err_t EspUsbHost::submitControl(const uint8_t bmRequestType, const uint8_t bDescriptorIndex, const uint8_t bDescriptorType, const uint16_t wInterfaceNumber, const uint16_t wDescriptorLength) {
  usb_transfer_t *transfer;
  usb_host_transfer_alloc(wDescriptorLength + 8 + 1, 0, &transfer);

  transfer->num_bytes = wDescriptorLength + 8;
  transfer->data_buffer[0] = bmRequestType;
  transfer->data_buffer[1] = 0x06;
  transfer->data_buffer[2] = bDescriptorIndex;
  transfer->data_buffer[3] = bDescriptorType;
  transfer->data_buffer[4] = wInterfaceNumber & 0xff;
  transfer->data_buffer[5] = wInterfaceNumber >> 8;
  transfer->data_buffer[6] = wDescriptorLength & 0xff;
  transfer->data_buffer[7] = wDescriptorLength >> 8;

  transfer->device_handle = deviceHandle;
  transfer->bEndpointAddress = 0x00;
  transfer->callback = _onReceiveControl;
  transfer->context = this;

  //submitControl(0x81, 0x00, 0x22, 0x0000, 136);
  if (bmRequestType == 0x81 && bDescriptorIndex == 0x00 && bDescriptorType == 0x22) {
    _printPcapText("GET DESCRIPTOR Request HID Report", 0x0028, 0x00, 0x80, 0x02, 8, 0, transfer->data_buffer);
  }

  esp_err_t err = usb_host_transfer_submit_control(clientHandle, transfer);
  if (err != ESP_OK) {
    ESP_LOGI("EspUsbHost", "usb_host_transfer_submit_control() err=%x", err);
  }
  return err;
}

void EspUsbHost::_onReceiveControl(usb_transfer_t *transfer) {
  _printPcapText("GET DESCRIPTOR Response HID Report", 0x0008, 0x01, 0x80, 0x02, transfer->actual_num_bytes - 8, 0x03, &transfer->data_buffer[8]);

  ESP_LOGV("EspUsbHost", "_onReceiveControl()\n"
                         "# data_buffer_size   = %d\n"
                         "# num_bytes          = %d\n"
                         "# actual_num_bytes   = %d\n"
                         "# flags              = 0x%x\n"
                         "# bEndpointAddress   = 0x%x\n"
                         "# timeout_ms         = %d\n"
                         "# num_isoc_packets   = %d",
           transfer->data_buffer_size,
           transfer->num_bytes,
           transfer->actual_num_bytes,
           transfer->flags,
           transfer->bEndpointAddress,
           transfer->timeout_ms,
           transfer->num_isoc_packets);

#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
  printf("=====================================================\n");
  uint16_t page = 0;
  uint8_t level = 0;
  uint8_t size = 0;
  uint8_t reportId = 0;
  uint8_t collection = 0;
  uint8_t usage = 0;
  uint8_t *p = &transfer->data_buffer[8];
  for (int i = 0; i < (transfer->actual_num_bytes - 8); i += size) {
    size = (p[i] & 3) + 1;

    // RAW data
    printf("[");
    for (int j = 0; j < size; j++) {
      // Hex
      printf(" %02x", p[i + j]);
    }
    for (int j = 0; j < (3 - size); j++) {
      // Padding
      printf("   ");
    }
    printf(" ] ");

    // pre
    if (p[i] == 0xc0) {
      // END_COLLECTION
      level--;
    }

    // level padding
    for (int j = 0; j < level; j++) {
      printf("  ");
    }

    // item
    uint8_t item = getItem(p[i]);
    int16_t val = (int8_t)p[i + 1];
    if (size == 3) {
      val = *(int16_t *)&p[i + 1];
    }
    if (item == getItem(0x05)) {
      printf("USAGE_PAGE ");
      page = p[i + 1];
      if (page == 0x01) {
        printf("(Generic Desktop)");
      } else if (page == 0x02) {
        printf("(Simulation Controls)");
      } else if (page == 0x03) {
        printf("(VR Controls)");
      } else if (page == 0x04) {
        printf("(Sport Controls)");
      } else if (page == 0x05) {
        printf("(Game Controls)");
      } else if (page == 0x06) {
        printf("(Generic Device Controls)");
      } else if (page == 0x07) {
        printf("(Keyboard/Keypad)");
      } else if (page == 0x08) {
        printf("(LED)");
      } else if (page == 0x09) {
        printf("(Button)");
      } else if (page == 0x0a) {
        printf("(Ordinal)");
      } else if (page == 0x0b) {
        printf("(Telephony Device)");
      } else if (page == 0x0c) {
        printf("(Consumer)");
      } else if (page == 0x0d) {
        printf("(Digitizers)");
      } else if (page == 0x0e) {
        printf("(Haptics)");
      } else if (page == 0x0f) {
        printf("(Physical Input Device)");
      } else if (page == 0x10) {
        printf("(Unicode)");
      } else if (page == 0x11) {
        printf("(SoC)");
      } else if (page == 0x12) {
        printf("(Eye and Head Trackers)");
      } else if (page == 0x14) {
        printf("(Auxiliary Display)");
      } else if (page == 0x20) {
        printf("(Sensors)");
      } else if (page == 0x40) {
        printf("(Medical Instrument)");
      } else if (page == 0x41) {
        printf("(Braille Display)");
      } else if (page == 0x59) {
        printf("(Lighting And Illumination)");
      } else if (page == 0x80) {
        printf("(Monitor)");
      } else if (page == 0x81) {
        printf("(Monitor Enumerated)");
      } else if (page == 0x82) {
        printf("(VESA Virtual Controls)");
      } else if (page == 0x84) {
        printf("(Power)");
      } else if (page == 0x85) {
        printf("(Battery System)");
      } else if (page == 0x8c) {
        printf("(Barcode Scanner)");
      } else if (page == 0x8d) {
        printf("(Scales)");
      } else if (page == 0x8e) {
        printf("(Magnetic Stripe Reader)");
      } else if (page == 0x90) {
        printf("(Camera Control)");
      } else if (page == 0x91) {
        printf("(Arcade)");
      } else if (page == 0x92) {
        printf("(Gaming Device)");
      } else {
        if (size == 2) {
          printf("(Vendor 0x%02x)", p[i + 1]);
        } else {
          printf("(Vendor 0x%02x%02x)", p[i + 2], p[i + 1]);
        }
      }
    } else if (item == getItem(0x09)) {
      printf("USAGE ");
      usage = p[i + 1];
      if (page == 0x01) {
        // Generic Desktop Page (0x01)
        if (usage == 0x00) {
          printf("(Undefined)");
        } else if (usage == 0x01) {
          printf("(Pointer)");
        } else if (usage == 0x02) {
          printf("(Mouse)");
        } else if (usage == 0x04) {
          printf("(Joystick)");
        } else if (usage == 0x05) {
          printf("(Gamepad)");
        } else if (usage == 0x06) {
          printf("(Keyboard)");
        } else if (usage == 0x07) {
          printf("(Keypad)");
        } else if (usage == 0x30) {
          printf("(X)");
        } else if (usage == 0x31) {
          printf("(Y)");
        } else if (usage == 0x32) {
          printf("(Z)");
        } else if (usage == 0x33) {
          printf("(Rx)");
        } else if (usage == 0x34) {
          printf("(Ry)");
        } else if (usage == 0x35) {
          printf("(Rz)");
        } else if (usage == 0x36) {
          printf("(Slider)");
        } else if (usage == 0x37) {
          printf("(Dial)");
        } else if (usage == 0x38) {
          printf("(Wheel)");
        } else if (usage == 0x39) {
          printf("(Hat Switch)");
        } else if (usage == 0x39) {
          printf("(Hat Switch)");
        } else if (usage == 0x3A) {
          printf("(Counted Buffer)");
        } else if (usage == 0x3B) {
          printf("(Byte Count)");
        } else if (usage == 0x3C) {
          printf("(Motion Wakeup)");
        } else if (usage == 0x3D) {
          printf("(Start)");
        } else if (usage == 0x3E) {
          printf("(Select)");
        } else if (usage == 0x40) {
          printf("(Vx)");
        } else if (usage == 0x41) {
          printf("(Vy)");
        } else if (usage == 0x42) {
          printf("(Vz)");
        } else if (usage == 0x43) {
          printf("(Vbrx)");
        } else if (usage == 0x44) {
          printf("(Vbry)");
        } else if (usage == 0x45) {
          printf("(Vbrz)");
        } else if (usage == 0x46) {
          printf("(Vno)");
        } else if (usage == 0x47) {
          printf("(Feature Notification)");
        } else if (usage == 0x48) {
          printf("(Resolution Multiplier)");
        } else if (usage == 0x49) {
          printf("(Qx)");
        } else if (usage == 0x4A) {
          printf("(Qy)");
        } else if (usage == 0x4B) {
          printf("(Qz)");
        } else if (usage == 0x4C) {
          printf("(Qw)");
        } else if (usage == 0x80) {
          printf("(System Control)");
        } else if (usage == 0x81) {
          printf("(System Power Down)");
        } else if (usage == 0x82) {
          printf("(System Sleep)");
        } else if (usage == 0x83) {
          printf("(System Wake Up)");
        } else if (usage == 0x84) {
          printf("(System Context Menu)");
        } else if (usage == 0x85) {
          printf("(System Main Menu)");
        } else if (usage == 0x86) {
          printf("(System App Menu)");
        } else if (usage == 0x87) {
          printf("(System Menu Help)");
        } else if (usage == 0x88) {
          printf("(System Menu Exit)");
        } else if (usage == 0x89) {
          printf("(System Menu Select)");
        } else if (usage == 0x8A) {
          printf("(System Menu Right)");
        } else if (usage == 0x8B) {
          printf("(System Menu Left)");
        } else if (usage == 0x8C) {
          printf("(System Menu Up)");
        } else if (usage == 0x8D) {
          printf("(System Menu Down)");
        } else if (usage == 0x8E) {
          printf("(System Cold Restart)");
        } else if (usage == 0x8F) {
          printf("(System Warm Restart)");
        } else if (usage == 0x90) {
          printf("(D-pad Up)");
        } else if (usage == 0x91) {
          printf("(D-pad Down)");
        } else if (usage == 0x92) {
          printf("(D-pad Right)");
        } else if (usage == 0x93) {
          printf("(D-pad Left)");
        } else if (usage == 0x94) {
          printf("(Index Trigger)");
        } else if (usage == 0x95) {
          printf("(Palm Trigger)");
        } else if (usage == 0x96) {
          printf("(Thumbstick)");
        } else if (usage == 0x97) {
          printf("(System Function Shift)");
        } else if (usage == 0x98) {
          printf("(System Function Shift Lock)");
        } else if (usage == 0x99) {
          printf("(System Function Shift Lock Indicator)");
        } else if (usage == 0x9A) {
          printf("(System Dismiss Notification)");
        } else if (usage == 0x9B) {
          printf("(System Do Not Disturb)");
        } else if (usage == 0xA0) {
          printf("(System Dock)");
        } else if (usage == 0xA1) {
          printf("(System Undock)");
        } else if (usage == 0xA2) {
          printf("(System Setup)");
        } else if (usage == 0xA3) {
          printf("(System Break)");
        } else if (usage == 0xA4) {
          printf("(System Debugger Break)");
        } else if (usage == 0xA5) {
          printf("(Application Break)");
        } else if (usage == 0xA6) {
          printf("(Application Debugger Break)");
        } else if (usage == 0xA7) {
          printf("(System Speaker Mute)");
        } else if (usage == 0xA8) {
          printf("(System Hibernate)");
        } else if (usage == 0xA9) {
          printf("(System Microphone Mute)");
        } else if (usage == 0xB0) {
          printf("(System Display Invert)");
        } else if (usage == 0xB1) {
          printf("(System Display Internal)");
        } else if (usage == 0xB2) {
          printf("(System Display External)");
        } else if (usage == 0xB3) {
          printf("(System Display Both)");
        } else if (usage == 0xB4) {
          printf("(System Display Dual)");
        } else if (usage == 0xB5) {
          printf("(System Display Toggle Int/Ext Mode)");
        } else if (usage == 0xB6) {
          printf("(System Display Swap Primary/Secondary)");
        } else if (usage == 0xB7) {
          printf("(System Display Toggle LCD Autoscale)");
        } else if (usage == 0xC0) {
          printf("(Sensor Zone)");
        } else if (usage == 0xC1) {
          printf("(RPM)");
        } else if (usage == 0xC2) {
          printf("(Coolant Level)");
        } else if (usage == 0xC3) {
          printf("(Coolant Critical Level)");
        } else if (usage == 0xC4) {
          printf("(Coolant Pump)");
        } else if (usage == 0xC5) {
          printf("(Chassis Enclosure)");
        } else if (usage == 0xC6) {
          printf("(Wireless Radio Button)");
        } else if (usage == 0xC7) {
          printf("(Wireless Radio LED)");
        } else if (usage == 0xC8) {
          printf("(Wireless Radio Slider Switch)");
        } else if (usage == 0xC9) {
          printf("(System Display Rotation Lock Button)");
        } else if (usage == 0xCA) {
          printf("(System Display Rotation Lock Slider Switch)");
        } else if (usage == 0xCB) {
          printf("(Control Enable)");
        } else if (usage == 0xD0) {
          printf("(Dockable Device Unique ID)");
        } else if (usage == 0xD1) {
          printf("(Dockable Device Vendor ID)");
        } else if (usage == 0xD2) {
          printf("(Dockable Device Primary Usage Page)");
        } else if (usage == 0xD3) {
          printf("(Dockable Device Primary Usage ID)");
        } else if (usage == 0xD4) {
          printf("(Dockable Device Docking State)");
        } else if (usage == 0xD5) {
          printf("(Dockable Device Display Occlusion)");
        } else if (usage == 0xD6) {
          printf("(Dockable Device Object Type)");
        } else if (usage == 0xE0) {
          printf("(Call Active LED)");
        } else if (usage == 0xE1) {
          printf("(Call Mute Toggle)");
        } else if (usage == 0xE2) {
          printf("(Call Mute LED)");
        } else {
          printf("(? ? ? ?)");
        }
      } else {
        printf("(0x%02x)", usage);
      }
    } else if (item == getItem(0x15)) {
      printf("LOGICAL_MINIMUM ");
      printf("(%d)", val);
    } else if (item == getItem(0x19)) {
      printf("USAGE_MINIMUM ");
      if (size == 2) {
        printf("(0x%02x)", p[i + 1]);
      } else {
        printf("(0x%02x%02x)", p[i + 2], p[i + 1]);
      }
    } else if (item == getItem(0x25)) {
      printf("LOGICAL_MAXIMUM ");
      printf("(%d)", val);
    } else if (item == getItem(0x29)) {
      printf("USAGE_MAXIMUM ");
      if (size == 2) {
        printf("(0x%02x)", p[i + 1]);
      } else {
        printf("(0x%02x%02x)", p[i + 2], p[i + 1]);
      }
    } else if (item == getItem(0x35)) {
      printf("PHYSIAL_MINIMUM ");
      printf("(%d)", val);
    } else if (item == getItem(0x45)) {
      printf("PHYSIAL_MAXIMUM ");
      printf("(%d)", val);
    } else if (item == getItem(0x55)) {
      printf("UNIT_EXPONENT ");
      if (size == 2) {
        printf("(0x%02x)", p[i + 1]);
      } else {
        printf("(0x%02x%02x)", p[i + 2], p[i + 1]);
      }
    } else if (item == getItem(0x65)) {
      printf("UNIT ");
      if (size == 2) {
        printf("(0x%02x)", p[i + 1]);
      } else {
        printf("(0x%02x%02x)", p[i + 2], p[i + 1]);
      }
    } else if (item == getItem(0x75)) {
      printf("REPORT_SIZE ");
      printf("(%d)", val);
    } else if (item == getItem(0x81)) {
      printf("INPUT ");
      uint8_t val = p[i + 1];
      printf("(");
      if (val & (1 << 0)) {
        // 1
        printf("Cnst,");
      } else {
        // 0
        printf("Data,");
      }
      if (val & (1 << 1)) {
        // 1
        printf("Var,");
      } else {
        // 0
        printf("Ary,");
      }
      if (val & (1 << 2)) {
        // 1
        printf("Rel");
      } else {
        // 0
        printf("Abs");
      }
      printf(")");
    } else if (item == getItem(0x85)) {
      printf("REPORT_ID ");
      reportId = p[i + 1];
      printf("(%d)", reportId);
    } else if (item == getItem(0x91)) {
      printf("OUTPUT ");
      uint8_t val = p[i + 1];
      printf("(");
      if (val & (1 << 0)) {
        // 1
        printf("Cnst,");
      } else {
        // 0
        printf("Data,");
      }
      if (val & (1 << 1)) {
        // 1
        printf("Var,");
      } else {
        // 0
        printf("Ary,");
      }
      if (val & (1 << 2)) {
        // 1
        printf("Rel");
      } else {
        // 0
        printf("Abs");
      }
      printf(")");
    } else if (item == getItem(0x95)) {
      printf("REPORT_COUNT ");
      printf("(%d)", val);
    } else if (item == getItem(0xa1)) {
      printf("COLLECTION ");
      level++;
      collection = p[i + 1];
      if (collection == 0x00) {
        printf("(Physical)");
      } else if (collection == 0x01) {
        printf("(Application)");
      } else if (collection == 0x02) {
        printf("(Logical)");
      } else {
        printf("(? ? ? ?)");
      }
    } else if (item == getItem(0xa4)) {
      printf("PUSH");
    } else if (item == getItem(0xa9)) {
      printf("DELIMITER ");
      if (p[i + 1] == 0x01) {
        printf("(Open)");
      } else {
        printf("(Close)");
      }
    } else if (item == getItem(0xb1)) {
      printf("FEATURE ");
      uint8_t val = p[i + 1];
      printf("(");
      if (val & (1 << 0)) {
        // 1
        printf("Cnst,");
      } else {
        // 0
        printf("Data,");
      }
      if (val & (1 << 1)) {
        // 1
        printf("Var,");
      } else {
        // 0
        printf("Ary,");
      }
      if (val & (1 << 2)) {
        // 1
        printf("Rel");
      } else {
        // 0
        printf("Abs");
      }
      printf(")");
    } else if (item == getItem(0xb4)) {
      printf("POP");
    } else if (item == getItem(0xc0)) {
      printf("END_COLLECTION");
    } else {
      printf("? ? ? ?");
    }

    printf("\n");
  }

  printf("-----------------------------------------------------\n");
#endif

  usb_host_transfer_free(transfer);
}