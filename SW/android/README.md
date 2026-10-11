# Smart Clock CO₂ Androidアプリ

ESP32の最新CO₂値を同じLANから取得するネイティブAndroidアプリです。
CO₂ (ppm)、温度、湿度、気圧を表示します。Android 8.0以降に対応します。

## ビルド

Android Studioでこの `SW/android` フォルダを開き、Gradle同期後に
`app` をビルド・実行してください。SDK Platform 35、Build Tools 35.0.0、
JDK 17が必要です。Gradle Wrapperは8.11.1、Android Gradle Pluginは8.9.2です。
互換性は [Android公式資料](https://developer.android.com/build/releases/agp-8-9-0-release-notes) に合わせています。

SDKをインストール済みなら、`local.properties` にSDKの場所を指定します。
例: `sdk.dir=C:/Users/ユーザー名/AppData/Local/Android/Sdk`

```powershell
.\gradlew.bat assembleDebug
```

APK出力先: `app/build/outputs/apk/debug/app-debug.apk`

## 使い方

VS Codeではこの `SW/android` フォルダを開きます。`Ctrl+Shift+B` で
`Android: Build debug APK` を実行できます。ターミナルの `gradlew.bat` でも同じビルドができます。
USBデバッグを有効にしたAndroid端末を接続し、端末の接続許可を承認してから、
「ターミナル → タスクの実行 → Android: Install debug APK (USB)」でインストールできます。
インストール後はスマートフォン上でアプリを開いてください。

1. `/api/sensors/latest` 対応のESP32ファームウェアを用意します。
2. AndroidとESP32を同じWi-Fiに接続します。
3. 時計のSW4 (IO4) を押し、画面に表示されるIPアドレスを確認します。
4. アプリの「接続設定」を開き、時計の実際のIPアドレスを入力して「保存して接続」を押します。

アプリは画面表示中、初回に最新値を取得し、以後はESP32の測定完了後に更新します（測定は約60秒ごと）。APIのsequenceで測定結果を識別し、sinceが同じ間は最大1秒待ってHTTP 204を返します。アプリは250ms後に待機を再開します。旧ファームウェアでは1分ごとの取得に戻ります。
表示の取得成功時刻はHTTP取得時刻であり、センサ測定時刻ではありません。
接続先は保存し、次回起動時に自動接続します。初回設定後はSW4を押してアプリを開くだけで取得できます。
IPアドレスが変わった場合だけ再設定が必要です。通信失敗・無効な測定値の場合は数値を消し、理由を表示します。
SDカードが利用できない場合でも最新値APIは利用できます。

HTTPリクエストが続く間は時計のWi-Fi利用時間が延長されます。
アプリを閉じると取得が止まり、他の通信・ボタン操作がなければ約5分で時計のWi-Fiが停止します。
常時Wi-Fi接続やBluetooth通信は行いません。

この通信はLAN内のHTTPです。Androidの
[Network Security Configuration](https://developer.android.com/privacy-and-security/security-config)
でHTTPを許可しています。信頼できるLANで利用してください。

## センサ時系列グラフ

画面を下へスクロールするとセンサ履歴グラフを表示します。
CO₂・温度・湿度・気圧のチェックボックスで表示する項目を選びます。
最新値は画面上部の4枚のカードに表示し、接続設定は折りたためます。
項目と期間の選択中のボタンは色で区別します。
各項目の値に合わせて縦軸を自動調整し、小数値も表示します。
チェックボックスで表示する項目を複数選択すると、1つのグラフに色分けして重ね表示します。
選択した項目それぞれに独立した縦軸を表示します。
軸と線は同じ色で、CO₂はppm、温度は℃、湿度は%、気圧はhPaです。
縦軸を左右へ交互に配置し、選択項目が増えると外側に軸を追加します。
各軸は表示期間の実測値に合わせて自動調整します。
凡例には実際の最小〜最大と単位を表示し、タップすると選択日時の実測値を表示します。
選択なしの場合は項目選択を案内します。選択項目は次回起動時にも保持します。
時計のSDカードのCSVを接続成功後に自動取得します。履歴欄の「更新」で再取得できます。
最新値の測定完了後の更新とは別に、履歴グラフはCSV取得時点のデータを表示します。

- 「1時間」「24時間」「全記録」で期間を切り替えます。期間は最後の記録日時が基準です。
- タップすると最寄りの記録の日時とppm値を表示します。時刻は日本時間です。
- 最新10,000件までを保持します。「全記録」もこの保持範囲内です。
- 不正行・非有限値 (NaN/Infinity) を除外し、日時順に並べ、同じ日時の行は後の行を採用します。
- 3分を超える記録の間隔は線をつなぎません。
- SDカードが利用できない場合は理由を表示します。更新失敗時は前回のグラフを保持します。
- CSV転送は20MBまで、読み込み開始から約30秒までに制限しています。

CSV処理のホスト検証:

```powershell
javac -encoding UTF-8 -d build/history-tests app/src/main/java/jp/smartclock/co2/SensorHistory.java tests/SensorHistoryTest.java
java -cp build/history-tests jp.smartclock.co2.SensorHistoryTest ../esp32/build/verification/sensor_log-latest.csv
```

実機でHTTP 200、履歴1,357件の読み込み、CO₂グラフ表示を確認しました。

## 最新値API

`GET http://IP:8080/api/sensors/latest`

```json
{"valid":true,"co2_ppm":650,"temperature_c":24.50,"humidity_percent":45.20,"pressure_hpa":1013.20}
```

センサ値が無効・未取得、または利用時間終了の場合はHTTP 503と `{"valid":false}` を返します。
センサ取得に失敗した場合、以前の値を有効な最新値として返しません。

## 現時点の検証

- ESP-IDF v6.1 / ESP32-S3ビルド成功 (終了コード0)。
- ESP32既存ホスト回帰テスト53件通過。APIの実機通信はHTTP 200を確認済み。
- Android SDKの場所を `local.properties` に設定後、`assembleDebug` 成功 (終了コード0)。
  `app/build/outputs/apk/debug/app-debug.apk` を生成しました。スマホへのインストールと、アプリ再起動時の自動接続・CO₂表示を確認済みです。
- API対応ファームウェアをESP32-S3 (COM5) へ書き込み、ハッシュ検証成功を確認済みです。
