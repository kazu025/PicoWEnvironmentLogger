# PicoW FreeRTOS Environment Logger

Raspberry Pi Pico W と FreeRTOS を使用した環境ロガーです。
BME280 から取得した温度・湿度・気圧を、Pico W 内蔵の簡易 HTTP サーバでブラウザに表示します。

本プロジェクトでは、以下の機能を実装しています。

* FreeRTOS タスクによるセンサ値取得
* BME280 による温度・湿度・気圧測定
* Pico W の Wi-Fi 接続
* lwIP raw API による簡易 HTTP サーバ
* HTML ページによる環境データ表示
* `/api/env` による JSON API 出力
* JavaScript `fetch()` によるブラウザ画面の自動更新

---

## 概要

Pico W 上で HTTP サーバを動作させ、ブラウザから環境データを確認できるようにしたサンプルです。

ブラウザで Pico W の IP アドレスへアクセスすると、以下のような画面が表示されます。

```text
PicoW Environment Logger

Temperature: 28.93 °C
Humidity: 63.87 %
Pressure: 1003.84 hPa
Status: OK
```

ページ全体を再読み込みするのではなく、JavaScript が `/api/env` に定期アクセスし、温度・湿度・気圧の数値だけを更新します。

---

## システム構成

```text
BME280
  ↓ I2C
Raspberry Pi Pico W
  ↓ FreeRTOS task
EnvironmentData
  ↓
HTTP Server
  ├─ /         : HTML + JavaScript
  └─ /api/env  : JSON API
```

---

## 使用ハードウェア

* Raspberry Pi Pico W
* BME280 センサモジュール
* USB ケーブル
* Wi-Fi 環境
* 開発用 PC

---

## 主な機能

### 1. HTML 表示

ブラウザで Pico W の IP アドレスにアクセスすると、環境データ表示ページを返します。

```text
http://<PicoWのIPアドレス>/
```

表示内容は以下です。

* Temperature
* Humidity
* Pressure
* Status

---

### 2. JSON API

以下の URL にアクセスすると、環境データを JSON 形式で取得できます。

```text
http://<PicoWのIPアドレス>/api/env
```

出力例:

```json
{
  "valid": true,
  "temperature": 28.93,
  "humidity": 63.87,
  "pressure": 1003.84
}
```

センサ値がまだ準備できていない場合は、以下のように返します。

```json
{
  "valid": false
}
```

---

### 3. JavaScript による自動更新

HTML ページ内の JavaScript が、定期的に `/api/env` を読み込みます。

```javascript
fetch('/api/env')
```

取得した JSON から値を取り出し、HTML の表示部分を書き換えます。

```javascript
document.getElementById('temperature').textContent = temperature.toFixed(2);
document.getElementById('humidity').textContent = humidity.toFixed(2);
document.getElementById('pressure').textContent = pressure.toFixed(2);
```

更新周期は 5 秒です。

```javascript
setInterval(updateEnv, 5000);
```

---

## HTTP エンドポイント

| URL        | 内容                 |
| ---------- | ------------------ |
| `/`        | 環境データ表示用 HTML ページ  |
| `/api`     | JSON API           |
| `/api/env` | 温度・湿度・気圧の JSON API |

---

## 実装メモ

### HTTP サーバ

HTTP サーバは lwIP の raw API を使用しています。

主な処理は `HttpServer.cpp` にあります。

* `http_server_init()`

  * TCP PCB を作成
  * port 80 に bind
  * listen 開始
  * accept callback 登録

* `http_accept_callback()`

  * 接続受付時に receive callback を登録

* `http_recv_callback()`

  * HTTP リクエスト受信
  * `/api/env` か通常ページかを判定
  * HTML または JSON を生成
  * HTTP レスポンスを返す

---

### HTML と JSON の生成

HTML 本文は `make_html_body()` で生成します。

JSON 本文は `make_json_body()` で生成します。

HTTP レスポンスヘッダは `make_http_response()` で生成します。

```text
HTTP/1.1 200 OK
Content-Type: ...
Connection: close
Cache-Control: no-store
Content-Length: ...
```

---

### バッファ管理

JavaScript入り HTML はサイズが大きくなるため、HTTP レスポンス用バッファを用意しています。

```cpp
#define HTTP_BODY_SIZE      2048
#define HTTP_RESPONSE_SIZE  3072

static char g_http_body[HTTP_BODY_SIZE];
static char g_http_response[HTTP_RESPONSE_SIZE];
```

ローカル変数として大きな配列を持つと、FreeRTOS タスクや lwIP callback のスタックを圧迫するため、ファイルスコープの static バッファとして確保しています。

---

## ビルド方法

通常の Pico SDK / FreeRTOS 環境でビルドします。

例:

```bash
mkdir -p build
cd build
cmake ..
make
```

プロジェクト側で Makefile を用意している場合は、以下のようにビルドします。

```bash
make
```

または、

```bash
make all
```

---

## 実行方法

ビルド後に生成された UF2 ファイルを Pico W に書き込みます。

Pico W が Wi-Fi に接続すると、UART ログに IP アドレスが表示されます。

例:

```text
HTTP server started on port 80
IP address: 192.168.10.108
```

ブラウザで以下にアクセスします。

```text
http://192.168.10.108/
```

JSON API を確認する場合は、以下にアクセスします。

```text
http://192.168.10.108/api/env
```

---

## 確認した動作

* Pico W が Wi-Fi に接続する
* ブラウザから Pico W の HTTP サーバにアクセスできる
* HTML ページに温度・湿度・気圧を表示できる
* `/api/env` で JSON データを取得できる
* JavaScript によりページ全体を再読み込みせずに値だけ更新できる

---

## トラブルシュート

### ブラウザにアクセスできない

Pico W が Wi-Fi に接続できているか確認します。

```bash
ping <PicoWのIPアドレス>
```

IP アドレスが変わっている場合があります。UART ログで現在の IP アドレスを確認してください。

---

### 画面に `HTT` だけ表示される

HTTP レスポンス生成時のバッファサイズ指定が間違っている可能性があります。

`body` や `response` が `char*` の場合、以下は誤りです。

```cpp
sizeof(body)
sizeof(response)
```

ポインタサイズになってしまうため、次のように定義済みサイズを指定します。

```cpp
HTTP_BODY_SIZE
HTTP_RESPONSE_SIZE
```

---

### `Status: starting...` のまま更新されない

JavaScript がブラウザに届いていない、または途中で切れている可能性があります。

ブラウザでページのソースを表示し、以下が含まれているか確認します。

```javascript
updateEnv();
setInterval(updateEnv, 5000);
```

---

### `Temperature: NaN` と表示される

JSON のキー名と JavaScript 側の参照名が一致しているか確認します。

JSON 側:

```json
"temperature": 28.93
```

JavaScript 側:

```javascript
data.temperature
```

スペルが違うと `undefined` になり、`Number(undefined)` の結果が `NaN` になります。

---

## 学習ポイント

このプロジェクトでは、以下を確認できます。

* Pico W の Wi-Fi 使用方法
* lwIP raw API による簡易 HTTP サーバ実装
* HTTP レスポンスの基本構造
* JSON API の作成
* HTML と JavaScript の連携
* `fetch()` による非同期データ取得
* FreeRTOS タスクと Web 表示の連携
* 組込み機器を簡易 IoT デバイス化する流れ

---

## 今後の拡張案

* グラフ表示の追加
* 最終更新時刻の表示
* ログデータの CSV ダウンロード
* センサ異常時のエラー表示改善
* 複数センサ対応
* 表示デザインの改善
* WebSocket や Server-Sent Events によるリアルタイム更新

---

## ライセンス

This project is licensed under the MIT License.

See the [LICENSE](LICENSE) file for details.

