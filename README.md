# Pico W FreeRTOS Environment Logger

Raspberry Pi Pico W、FreeRTOS、BME280、AQM0802 LCDを使った環境ロガーです。
温度・湿度・気圧を取得し、LCD表示、UARTログ出力、Flashログ保存に加えて、Pico WのWi-Fi機能を使ってブラウザから環境データを確認できます。

## 概要

このプロジェクトは、Raspberry Pi Pico W、FreeRTOS、BME280、AQM0802 LCDを使った環境ロガーです。

以前作成したRaspberry Pi Pico版の環境ロガーをベースに、Pico Wへ移行し、Wi-Fi接続と簡易HTTPサーバ機能を追加しました。

Pico版のリポジトリはこちらです。

* [PicoSensorLogger](https://github.com/kazu025/PicoSensorLogger)

Pico W版では、従来のLCD表示、センサ取得、UARTログ、Flashログ保存といった機能を維持したまま、Wi-Fi経由でブラウザから環境データを確認できるようにしています。

ブラウザからPico WのIPアドレスへアクセスすると、BME280から取得した温度・湿度・気圧を確認できます。


## 主な機能

* FreeRTOSによるマルチタスク動作
* BME280による温度・湿度・気圧取得
* ADT7410による温度取得
* AQM0802 LCDへの表示
* タクトスイッチによる表示モード切り替え
* UART DMAによるログ出力
* Flash領域へのログ保存
* Pico WによるWi-Fi接続
* DHCPによるIPアドレス取得
* LCDへのIPアドレス表示
* lwIP RAW APIを使った簡易HTTPサーバ
* ブラウザから環境データを表示

## 使用ハードウェア

* Raspberry Pi Pico W
* BME280 温湿度・気圧センサ
* ADT7410 温度センサ
* AQM0802 LCD
* タクトスイッチ
* ブレッドボード、ジャンパ線など

## Clone方法

このプロジェクトは FreeRTOS-Kernel を Git submodule として使用しています。

```bash
git clone --recursive https://github.com/kazu025/PicoWEnvironmentLogger.git
## I2C構成

本プロジェクトでは、Pico WのI2C0を使用しています。

| デバイス    | I2Cアドレス | 用途         |
| ------- | ------: | ---------- |
| AQM0802 |    0x3E | LCD表示      |
| ADT7410 |    0x48 | 温度取得       |
| BME280  |    0x76 | 温度・湿度・気圧取得 |

I2Cピンは以下の設定です。

| 信号  |  GPIO |
| --- | ----: |
| SDA | GPIO4 |
| SCL | GPIO5 |

I2Cクロックは安定性を優先して 50kHz に設定しています。

## 表示機能

LCDには、センサ値やログ情報、Wi-Fi接続情報を表示します。
タクトスイッチにより表示モードを切り替えます。

Wi-Fi接続後は、取得したIPアドレスをLCDに表示できます。

例：

```text
192.168.
10.108
```

## HTTPサーバ機能

Pico WがWi-Fi接続後、簡易HTTPサーバを起動します。

PCやスマートフォンのブラウザから、LCDに表示されたIPアドレスへアクセスします。

例：

```text
http://192.168.10.108/
```

ブラウザには、BME280から取得した最新の環境データを表示します。

表示例：

```text
PicoW Environment Logger

Temperature: 26.69 C
Humidity: 70.30 %
Pressure: 1001.73 hPa
```

## ソフトウェア構成

主な構成ファイルは以下の通りです。

| ファイル                  | 内容                  |
| --------------------- | ------------------- |
| `main.cpp`            | FreeRTOSタスク生成、全体初期化 |
| `WifiTask.cpp`        | Wi-Fi初期化、接続、IP取得    |
| `HttpServer.cpp`      | 簡易HTTPサーバ           |
| `EnvironmentTask.cpp` | BME280から環境データ取得     |
| `EnvironmentData.cpp` | 最新の環境データ共有          |
| `BME280.cpp`          | BME280制御            |
| `AEADT7410.cpp`       | ADT7410制御           |
| `AQM0802.cpp`         | LCD制御               |
| `ButtonTask.cpp`      | タクトスイッチ処理           |
| `FlashLogStorage.cpp` | Flashログ保存           |
| `UartDma.cpp`         | UART DMA出力          |

## タスク構成

FreeRTOS上で複数のタスクを動作させています。

| タスク              | 役割                |
| ---------------- | ----------------- |
| Logger task      | ログ出力処理            |
| Environment task | BME280の温度・湿度・気圧取得 |
| Temperature task | ADT7410の温度取得      |
| ADC task         | ADCログ取得           |
| Command task     | UARTコマンド処理        |
| Button task      | タクトスイッチ入力処理       |
| WiFi task        | Wi-Fi接続、HTTPサーバ起動 |

## 環境データ共有の考え方

HTTPサーバのコールバック内では、直接I2Cセンサを読みません。

BME280の読み取りは `EnvironmentTask` が周期的に行い、取得した最新値を共有データとして保存します。
HTTPサーバは、ブラウザからアクセスが来たときに、その共有データを読み出してHTMLに埋め込みます。

構成イメージ：

```text
EnvironmentTask
    ↓
BME280から温度・湿度・気圧を取得
    ↓
EnvironmentDataに最新値を保存
    ↓
HttpServerが最新値を読み出す
    ↓
ブラウザへHTMLとして返す
```

この構成により、HTTP処理とI2Cアクセスを分離しています。

## Wi-Fi設定

Wi-FiのSSIDとパスワードは `wifi_config.h` に記述します。

```cpp
#pragma once

#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
```

公開用には、以下のようなサンプルファイルを用意します。

```cpp
// wifi_config_sample.h
```

## ビルド方法

Pico W向けにビルドします。

```bash
./mk.sh clean picow
```

内部的には、以下のように `PICO_BOARD=pico_w` を指定しています。

```bash
cmake -DPICO_BOARD=pico_w ..
```

通常のPico向けに戻す場合は、以下のようにします。

```bash
./mk.sh clean pico
```

## FreeRTOS Heapについて

Wi-Fi機能とHTTPサーバを追加したことで、FreeRTOSのHeap使用量が増えました。

当初は以下の設定では不足しました。

```c
#define configTOTAL_HEAP_SIZE (32 * 1024)
```

WiFiTaskの追加により、タスクスタックだけでも大きなHeapを消費します。
そのため、Heapサイズを増やして対応しました。

例：

```c
#define configTOTAL_HEAP_SIZE (128 * 1024)
```

Heap不足調査では、`xPortGetFreeHeapSize()` と `xPortGetMinimumEverFreeHeapSize()` を使って、タスク生成後の残りHeapを確認しました。

## HTTPサーバ実装について

HTTPサーバは、lwIPのRAW APIを使って実装しています。

* `tcp_new_ip_type()`
* `tcp_bind()`
* `tcp_listen()`
* `tcp_accept()`
* `tcp_recv()`
* `tcp_write()`
* `tcp_close()`

を使い、ブラウザからのGETリクエストに対してHTMLを返します。

また、Wi-Fi再接続時にHTTPサーバを二重起動しないように、起動済みフラグで保護しています。

## 現在の到達点

現在、以下の動作を確認済みです。

* Pico Wで既存FreeRTOS Loggerが動作
* LCD表示が動作
* タクトスイッチによる表示切り替えが動作
* I2CスキャンでLCD、ADT7410、BME280を検出
* BME280から温度・湿度・気圧を取得
* Wi-Fi接続成功
* DHCPでIPアドレス取得
* LCDにIPアドレス表示
* Pico W内蔵LED点滅
* HTTPサーバ起動
* ブラウザからPico Wへアクセス
* 温度・湿度・気圧をWeb表示

## 今後の予定

今後は以下の機能追加を検討しています。

* ブラウザ表示の自動更新
* JSON APIの追加
* JavaScriptによる部分更新
* ログ件数のWeb表示
* 最新ログのWeb表示
* 簡易グラフ表示
* READMEとNote記事の整理


## ライセンス

This project is licensed under the MIT License.

See the [LICENSE](LICENSE) file for details.

