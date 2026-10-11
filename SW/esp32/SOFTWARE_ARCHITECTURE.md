# Smart Clock ソフトウェア構成

ESP-IDF v6.1、ターゲット `esp32s3`。既存の6アプリケーションタスクを維持し、新規タスクは追加しない。

| ファイル / コンポーネント | 責務 |
| --- | --- |
| `main/main.cpp` | 省電力設定、`SMART_CLOCK::init()` / `run()`、アプリケーション寿命の保持 |
| `main/smart_clock.h/.cpp` | 初期化、タスク起動、測定結果集約、スナップショットmutex、記録呼び出し、表示通知、ネットワーク管理 |
| `components/sensor_data/sensor_data.h` | 共通型 `SensorData`。値、センサ別有効状態、測定確定時刻と有効状態、sequence、有限値判定 |
| `components/clock_display/clock_display.h/.cpp` | `CLOCK_DISPLAY`。レイアウト、LovyanGFXバッファ、電子ペーパー描画。時刻・接続・設定情報は呼び出し元が渡す |
| `components/sensor_recorder/sensor_recorder.h/.cpp` | `SENSOR_RECORDER`。記録条件、CSVヘッダー・行生成、sequenceの重複・遡り記録防止、SD追記結果 |
| `main/sensor_http.h/.cpp` | HTTP応答とCSV転送。`SensorHttpAccess` のsnapshot / begin_request / end_request / sd_availableコールバックとSD操作を使用 |
| `main/button_wifi_session.h` | ボタン・HTTP操作の5分無操作期限と転送中の延期。tickの差分で周回に対応 |
| `main/sync_schedule.h` | 起動・日次同期の5分期限と失敗後30分の再試行。`esp_timer_get_time()` の単調時刻を使用 |

新規コンポーネントはmainやSMART_CLOCKへ依存しない。sensor_dataはヘッダーのみで、タスク・mutex・ハードウェアへの依存を持たない。
clock_displayはsensor_data / display / LovyanGFX、sensor_recorderはsensor_data / sd_cardに依存する。

## 測定、記録、表示

`monitor_sensor` は約60秒周期でSCD40とBME280へ要求し、CO2は最大15秒、BME280は最大5秒の受信待ちを行う。
両方の結果が確定してから、mutex内で一つのスナップショットを更新する。
各センサの成功値を独立して保存し、失敗側の過去値は有効状態を落とす。
BME280のNaN / Infinityを無効とし、成功・失敗を問わずsequenceを一度増やす。
時刻同期済みかを確認してから確定時刻を取得する。初回同期前の時刻を有効として記録しない。

処理順序は「測定要求 → 結果確定 → スナップショット更新 → mutex解放 → CSV記録 → 表示通知」。
`update_display` は通知後に最新スナップショットをコピーし、描画だけを行う。
ボタン、Wi-Fi状態変化、IP取得、時刻同期、HTTP開始・停止による表示通知ではCSVを増やさない。
無効・未測定項目は `--` とし、正常側の値と時計表示、Wi-Fi設定案内を維持する。
時刻未同期でもセンサ欄を表示し、時計欄は `--:--` とする。
描画、SDアクセス、HTTP送信中には測定mutexを保持しない。

記録条件は「時刻が有効・SD利用可能・両センサが有効・未処理sequence」のすべて。
CSV日時はスナップショットの確定時刻を使用する。既存ヘッダー、列順、日時書式、単位を維持する。
結果は `SUCCESS` / `SKIPPED` / `FAILED`。どの結果でも表示通知へ進む。
書き込みを試したsequenceは再試行しない。対象外sequenceも消費し、後から時刻やSDが利用可能になっても遡って記録しない。
過去sequenceの再入力も無視し、32bit sequenceの周回を扱う（順序差が2^31未満の連続測定を前提とする）。
ヘッダーは空ファイルにだけ作り、再起動時に既存ログを保持する。
CSV配信は取得開始時のサイズまでとし、SDロックを送信前に解放するため転送中も追記できる。

## 初期化と開始・回収

`init()` は機器、センサ結果キュー、ボタンイベントキュー、共有開始ゲート、イベント通知を準備する。
Wi-Fi機器とプロビジョニング情報を準備するが、接続・設定サービスの開始はネットワーク管理タスクへ延期する。
アプリケーションタスクは `run()` で作成する。
SCD40、BME280、測定集約、表示、ネットワーク、ボタンの全6タスクは入口で同じEventGroupの開始ビットを待つ。
全作成成功後、表示通知を先に積み、開始ゲートを解放する。初回測定の通知もキューに保持される。

途中失敗ではゲートを開かず、作成済みタスクを回収する。
表示通知のmutexで解放済みタスクへの通知を防止し、SNTPコールバックとIPハンドラーを解除する。
送信側タスクを停止してから結果・ボタンキューを削除し、デバイス、SPI、I2C、SDマウント、描画バッファ、所有するイベントループを回収する。
回収は繰り返し呼べる。回収APIは開始前の失敗復旧用であり、稼働中の任意の強制停止用ではない。
SD不在・ヘッダー書き込み失敗は起動を中止せず、表示と最新値APIを利用できる。
電子ペーパー初期化エラーも測定・HTTPの起動を妨げない。描画バッファのメモリ不足は起動エラーとする。
ESP-IDF内部のイベント・通信タスクはこの開始ゲートの対象外。

## ネットワークと時刻同期

Wi-FiとHTTPの開始・停止、SNTP開始・停止と再試行は `clock_network` が管理する。
IP取得コールバックは要求と表示通知だけを送り、SNTP完了コールバックは同期日付と成功要求だけを保存する。
プロビジョニング終了コールバックもフラグを送り、管理タスクがマネージャーを回収する。

ボタン・HTTP操作から5分無操作で利用終了。進行中のHTTP要求は終了を延期し、終了時から再び5分を数える。
通常の起動・日次同期は接続開始から5分で期限切れとなり、失敗後30分で再試行する。
成功すると、ボタン利用がなければWi-Fiを停止する。失敗してもボタン利用中はWi-Fiを維持する。
プロビジョニング中は期限による停止をしない。設定終了後、元の期限が過ぎていれば同期をタイムアウト扱いとする。
日付判定は同期済みのローカル時刻、期限・再試行は64bit単調マイクロ秒なので時刻補正とFreeRTOS tick周回の影響を受けない。
定数は `SyncSchedule::TIMEOUT_US`（5分）、`RETRY_US`（30分）、`SMART_CLOCK::BUTTON_IDLE_MS`（5分）。

## HTTP / Android互換性

ポート8080と `/`、`/sensor_log.csv`、`/api/sensors/latest` を維持する。
全項目が有効な場合だけ従来のJSONを返す。

```json
{"valid":true,"co2_ppm":812,"temperature_c":24.50,"humidity_percent":45.00,"pressure_hpa":1018.00,"sequence":1}
```

未測定・無効・利用期限終了はHTTP 503と `{"valid":false}`。
`?since=<sequence>` は同じ有効スナップショットの間、最大約1秒待ち、更新がなければHTTP 204。
新しい結果が確定すればその結果を返す。待機・転送中も利用期限を延長する。
Androidの測定完了待ちと既存CSV読み込みを維持し、Android側の変更は不要。

## 検証と制約

`tests/run_host_regressions.py` は既存56件を移動後のHTTPと共通型へ対応。
`tests/run_architecture_tests.py` は実際の型・記録・描画・HTTP・初期化・開始・回収・ネットワーク処理をホスト用スタブで実行する。
作成直後に各タスク入口を実行して開始ゲートを検証し、全6作成位置と初期化位置で故障を注入する。
CSVの一回制限・測定日時・遡り記録禁止・片側失敗・不正値・無効表示・SD失敗後の表示・API形式・同期期限と優先関係を検証する。
SD・描画・HTTPの実行時に測定mutexが解放されていることも検査する。
runtime verifierのテストは9件。実行コマンドは `tests/README.md` を参照。

ホスト検証は実機のセンサ通信、電子ペーパーの配置とBUSY復帰、FreeRTOSの実並行実行、プロビジョニングと電源動作を代替しない。
実機で初回測定、片側故障と復帰、SD不在と書き込み失敗、設定案内、Android更新、CSV取得中の追記、同期成功・5分期限・30分再試行、ボタン再接続を確認する。
本作業では実機への書き込み・リセットを行わない。

## 変更ファイル

- `main/smart_clock.h/.cpp`、`main/CMakeLists.txt`
- 新規 `main/sensor_http.h/.cpp`、`main/sync_schedule.h`
- 新規 `components/sensor_data/`、`components/clock_display/`、`components/sensor_recorder/`（各CMakeListsとヘッダー、後二つの実装）
- `components/bme280/bme280.h/.cpp`、`components/scd40/scd40.h/.cpp`、`components/button/button.h/.cpp`
- `components/i2c/i2c_base.h/.cpp`、`components/display/e_paper.h`、`components/display/e_paper4in26.cpp`
- `components/sd_card/sd_card.h/.cpp`、`components/wifi/wifi.h/.cpp`、`components/wifi/sntp_interface.h/.cpp`
- `tests/run_host_regressions.py`、新規 `tests/run_architecture_tests.py`、`tests/README.md`
- 本書、`SENSOR_DOWNLOAD.md`
