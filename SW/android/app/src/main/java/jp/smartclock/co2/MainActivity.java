package jp.smartclock.co2;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.graphics.Color;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;
import android.util.Log;
import android.widget.*;
import org.json.JSONObject;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final ExecutorService historyWorker = Executors.newSingleThreadExecutor();
    private HistoryChart historyChart;
    private TextView historyStatus;
    private boolean historyBusy, historyAttempted;
    private EditText address;
    private TextView co2, temperature, humidity, pressure, status;
    private LinearLayout connectionPanel;
    private final int ink = Color.rgb(24, 43, 56), accent = Color.rgb(0, 112, 125);
    private boolean active, busy;
    private int generation;
    private String endpoint;
    private long lastSequence = -1;
    private final Runnable refresh = this::fetch;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(Color.rgb(242, 246, 249));
        getWindow().setNavigationBarColor(Color.rgb(242, 246, 249));
        getWindow().getDecorView().setSystemUiVisibility(android.view.View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
            | android.view.View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        LinearLayout layout = column();
        layout.setPadding(dp(16), dp(16), dp(16), dp(24));
        layout.setBackgroundColor(Color.rgb(242, 246, 249));
        LinearLayout header = new LinearLayout(this);
        TextView title = label("Smart Clock", 24);
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        header.addView(title, new LinearLayout.LayoutParams(0, -2, 1));
        Button settings = button("接続設定");
        header.addView(settings);
        layout.addView(header);
        status = label("時計のSW4を押すと接続できます", 13);
        layout.addView(status);
        connectionPanel = column();
        connectionPanel.setPadding(dp(12), dp(8), dp(12), dp(12));
        connectionPanel.setBackground(surface(Color.WHITE));
        address = new EditText(this);
        address.setSingleLine(true);
        address.setTextSize(16);
        address.setHint("時計のIP（例: 192.168.11.14）");
        address.setText(getPreferences(MODE_PRIVATE).getString("address", ""));
        connectionPanel.addView(address);
        Button connect = button("保存して接続");
        connectionPanel.addView(connect);
        connectionPanel.addView(label("同じWi-Fiに接続し、時計のSW4を押してください。", 13));
        connectionPanel.setVisibility(address.getText().toString().trim().isEmpty()
            ? android.view.View.VISIBLE : android.view.View.GONE);
        settings.setOnClickListener(v -> connectionPanel.setVisibility(connectionPanel.getVisibility()
            == android.view.View.VISIBLE ? android.view.View.GONE : android.view.View.VISIBLE));
        layout.addView(connectionPanel);
        co2 = label("— ppm", 28); temperature = label("— ℃", 28);
        humidity = label("— %", 28); pressure = label("— hPa", 25);
        addCardRow(layout, "CO₂", co2, "温度", temperature);
        addCardRow(layout, "湿度", humidity, "気圧", pressure);
        layout.addView(label("最新値 · ESP32の測定完了後に更新（約60秒ごと）", 12));
        LinearLayout history = column();
        history.setPadding(dp(12), dp(8), dp(12), dp(12));
        history.setBackground(surface(Color.WHITE));
        LinearLayout historyHeader = new LinearLayout(this);
        TextView historyTitle = label("履歴", 20);
        historyTitle.setTypeface(null, android.graphics.Typeface.BOLD);
        historyHeader.addView(historyTitle, new LinearLayout.LayoutParams(0, -2, 1));
        Button historyRefresh = button("更新");
        historyRefresh.setContentDescription("履歴を更新");
        historyRefresh.setOnClickListener(v -> fetchHistory());
        historyHeader.addView(historyRefresh);
        history.addView(historyHeader);
        historyChart = new HistoryChart(this);
        LinearLayout metrics = new LinearLayout(this);
        String[] metricNames = {"CO₂", "温度", "湿度", "気圧"};
        CheckBox[] toggles = new CheckBox[4];
        int savedMask = getPreferences(MODE_PRIVATE).getInt("plotMetrics", 15) & 15;
        historyChart.setMetrics(savedMask);
        for (int i=0; i<4; i++) {
            final int metric = i;
            CheckBox toggle = new CheckBox(this);
            toggle.setText(metricNames[i]); toggle.setTextSize(12); toggle.setMinHeight(dp(48));
            toggle.setTextColor(HistoryChart.COLORS[i]);
            toggle.setButtonTintList(android.content.res.ColorStateList.valueOf(HistoryChart.COLORS[i]));
            toggle.setChecked((savedMask & (1<<i)) != 0);
            toggles[i] = toggle;
            toggle.setOnCheckedChangeListener((button, checked) -> {
                int mask = 0;
                for (int j=0; j<4; j++) if (toggles[j]!=null && toggles[j].isChecked()) mask |= 1<<j;
                historyChart.setMetrics(mask);
                getPreferences(MODE_PRIVATE).edit().putInt("plotMetrics", mask).apply();
            });
            metrics.addView(toggle, new LinearLayout.LayoutParams(0, dp(48), 1));
        }
        history.addView(metrics);
        history.addView(label("複数選択で重ね表示 · 線と同じ色の軸で実測値を確認", 12));
        LinearLayout periods = new LinearLayout(this);
        String[] ranges = {"1時間", "24時間", "全記録"};
        long[] durations = {3600000L, 24*3600000L, 0};
        Button[] periodButtons = new Button[3];
        for (int i=0; i<3; i++) {
            final int period = i;
            Button tab = button(ranges[i]); periodButtons[i] = tab;
            tab.setOnClickListener(v -> { historyChart.setRange(durations[period]); select(periodButtons, period); });
            periods.addView(tab, new LinearLayout.LayoutParams(0, dp(48), 1));
        }
        select(periodButtons, 1);
        history.addView(periods);
        history.addView(historyChart, new LinearLayout.LayoutParams(-1, dp(350)));
        historyStatus = label("接続後に履歴を取得します", 12);
        history.addView(historyStatus);
        history.addView(label("タップで測定値を表示 · 日本時間\n期間は最後の記録が基準 / 最新10,000件まで", 12));
        layout.addView(history);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setOnApplyWindowInsetsListener((view, insets) -> {
            view.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets.consumeSystemWindowInsets();
        });
        scroll.addView(layout);
        setContentView(scroll);
        connect.setOnClickListener(v -> connectToAddress(true));
        if (!address.getText().toString().trim().isEmpty()) connectToAddress(false);
    }

    private void setHistoryPoints(java.util.List<SensorHistory.Point> points) {
        historyChart.setPoints(points);
    }

    private int dp(int size) { return Math.round(size * getResources().getDisplayMetrics().density); }
    private LinearLayout column() {
        LinearLayout view = new LinearLayout(this); view.setOrientation(LinearLayout.VERTICAL); return view;
    }
    private android.graphics.drawable.GradientDrawable surface(int color) {
        android.graphics.drawable.GradientDrawable background = new android.graphics.drawable.GradientDrawable();
        background.setColor(color); background.setCornerRadius(dp(14)); return background;
    }
    private Button button(String text) {
        Button button = new Button(this); button.setText(text); button.setTextSize(13);
        button.setAllCaps(false); button.setMinWidth(0); button.setMinimumWidth(0);
        button.setMinHeight(dp(48)); button.setPadding(dp(8), 0, dp(8), 0);
        button.setTextColor(accent); button.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.rgb(234, 243, 245)));
        return button;
    }
    private void select(Button[] buttons, int selected) {
        for (int i=0; i<buttons.length; i++) {
            buttons[i].setSelected(i==selected);
            buttons[i].setTextColor(i==selected ? Color.WHITE : ink);
            buttons[i].setBackgroundTintList(android.content.res.ColorStateList.valueOf(i==selected ? accent : Color.rgb(240,244,246)));
        }
    }
    private void addCardRow(LinearLayout parent, String first, TextView firstValue, String second, TextView secondValue) {
        LinearLayout row = new LinearLayout(this);
        String[] names = {first, second}; TextView[] values = {firstValue, secondValue};
        for(int i=0;i<2;i++) {
            LinearLayout card = column(); card.setPadding(dp(14), dp(8), dp(12), dp(10));
            card.setBackground(surface(Color.WHITE));
            TextView name = label(names[i], 13); name.setTextColor(accent); card.addView(name);
            values[i].setTypeface(null, android.graphics.Typeface.BOLD);
            values[i].setMaxLines(1);
            values[i].setAutoSizeTextTypeUniformWithConfiguration(16, 28, 1, android.util.TypedValue.COMPLEX_UNIT_SP);
            card.addView(values[i], new LinearLayout.LayoutParams(-1, dp(44)));
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -2, 1);
            if(i==0) params.rightMargin=dp(8);
            row.addView(card, params);
        }
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2); params.topMargin=dp(8);
        parent.addView(row, params);
    }
    private void clearValues() {
        co2.setText("— ppm"); temperature.setText("— ℃"); humidity.setText("— %"); pressure.setText("— hPa");
    }

    private void connectToAddress(boolean save) {
            try {
                String input = address.getText().toString().trim();
                if (!input.contains("://")) input = "http://" + input;
                URI uri = new URI(input);
                if (!"http".equals(uri.getScheme()) || uri.getHost() == null || "IP".equalsIgnoreCase(uri.getHost()) || uri.getUserInfo() != null
                    || uri.getQuery() != null || uri.getFragment() != null
                    || !(uri.getPath().isEmpty() || uri.getPath().equals("/")))
                    throw new IllegalArgumentException();
                int port = uri.getPort() == -1 ? 8080 : uri.getPort();
                endpoint = new URI("http", null, uri.getHost(), port, "/api/sensors/latest", null, null).toString();
                Log.i("SmartClock", "Connecting to " + endpoint);
                generation++;
                lastSequence = -1;
                historyAttempted = false;
                setHistoryPoints(new java.util.ArrayList<>());
                historyStatus.setText("時計の記録を接続後に読み込みます。");
                handler.removeCallbacks(refresh);
                if (save) {
                    connectionPanel.setVisibility(android.view.View.GONE);
                    getPreferences(MODE_PRIVATE).edit().putString("address", address.getText().toString().trim()).apply();
                    ((InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(address.getWindowToken(), 0);
                }
                clearValues();
                fetch();
            } catch (Exception e) {
                Log.w("SmartClock", "Invalid connection address", e);
                connectionPanel.setVisibility(android.view.View.VISIBLE);
                status.setText("時計の実際のIPを入力してください（例: 192.168.11.14）。");
            }
    }

    private TextView label(String text, int size) {
        TextView view = new TextView(this);
        view.setText(text); view.setTextSize(size);
        view.setTextColor(ink);
        view.setPadding(0, dp(4), 0, dp(4));
        return view;
    }

    private static class NoNewMeasurement extends Exception {}

    private void fetch() {
        if (!active || endpoint == null || busy) return;
        busy = true;
        final String url = endpoint + (lastSequence < 0 ? "" : "?since=" + lastSequence);
        final int requestGeneration = generation;
        if (lastSequence < 0) status.setText("取得中…");
        worker.execute(() -> {
            JSONObject data = null;
            String error = null;
            HttpURLConnection connection = null;
            try {
                connection = (HttpURLConnection)new URI(url).toURL().openConnection();
                connection.setConnectTimeout(4000); connection.setReadTimeout(4000);
                connection.setInstanceFollowRedirects(false);
                connection.setUseCaches(false);
                int code = connection.getResponseCode();
                Log.i("SmartClock", "GET " + url + " -> HTTP " + code);
                if (code == 204) throw new NoNewMeasurement();
                if (code == 404) throw new Exception("時計のAPIが見つかりません (HTTP 404)。API対応ファームウェアの書き込みを確認してください。");
                if (code == 503) throw new Exception("測定値が未取得、または利用時間が終了しています。SW4を押して再試行してください。");
                if (code != 200) throw new Exception("取得失敗 (HTTP " + code + ")。対応ファームウェアを確認してください。");
                try (java.io.InputStream stream = connection.getInputStream()) {
                    java.io.ByteArrayOutputStream body = new java.io.ByteArrayOutputStream();
                    byte[] buffer = new byte[512];
                    int count;
                    while ((count = stream.read(buffer)) != -1) {
                        if (body.size() + count > 2048) throw new Exception("応答が大きすぎます。");
                        body.write(buffer, 0, count);
                    }
                    data = new JSONObject(new String(body.toByteArray(), StandardCharsets.UTF_8));
                }
                if (!data.getBoolean("valid")) throw new Exception("有効な測定値がありません。");
                data.getInt("co2_ppm"); data.getDouble("temperature_c");
                data.getDouble("humidity_percent"); data.getDouble("pressure_hpa");
            } catch (NoNewMeasurement e) {
                // Retain values until the next completed measurement.
            } catch (Exception e) {
                Log.e("SmartClock", "Sensor request failed: " + url, e);
                error = e.getMessage();
                if (!(e.getClass() == Exception.class)) error = "接続できません。同じWi-Fi、IPアドレス、時計のSW4を確認してください。";
            } finally {
                if (connection != null) connection.disconnect();
            }
            final JSONObject result = data;
            final String failure = error;
            handler.post(() -> {
                busy = false;
                if (!active) return;
                if (requestGeneration != generation) { fetch(); return; }
                if (failure != null) {
                    clearValues(); status.setText(failure);
                } else if (result != null) {
                    lastSequence = result.optLong("sequence", -1);
                    co2.setText(result.optInt("co2_ppm") + " ppm");
                    temperature.setText(String.format(Locale.JAPAN, "%.1f ℃", result.optDouble("temperature_c")));
                    humidity.setText(String.format(Locale.JAPAN, "%.1f %%", result.optDouble("humidity_percent")));
                    pressure.setText(String.format(Locale.JAPAN, "%.1f hPa", result.optDouble("pressure_hpa")));
                    status.setText("接続中 · 最新値取得 " + java.text.DateFormat.getTimeInstance().format(new java.util.Date()));
                    if (!historyAttempted) fetchHistory();
                }
                handler.postDelayed(refresh, failure != null ? 5_000 : (lastSequence < 0 ? 60_000 : 250));
            });
        });
    }

    private void fetchHistory() {
        if (!active || endpoint == null) { historyStatus.setText("時計に接続してから履歴を取得してください。"); return; }
        if (historyBusy) return;
        historyBusy = true;
        historyAttempted = true;
        final int requestGeneration = generation;
        final String url = endpoint.substring(0, endpoint.indexOf("/api/sensors/latest")) + "/sensor_log.csv";
        historyStatus.setText("履歴を取得中…");
        historyWorker.execute(() -> {
            java.util.List<SensorHistory.Point> points = null;
            String error = null;
            HttpURLConnection connection = null;
            try {
                connection = (HttpURLConnection)new URI(url).toURL().openConnection();
                connection.setConnectTimeout(4000); connection.setReadTimeout(5000);
                connection.setInstanceFollowRedirects(false); connection.setUseCaches(false);
                int code = connection.getResponseCode();
                Log.i("SmartClock", "History GET " + url + " -> HTTP " + code);
                if (code == 404) throw new Exception("履歴ファイルがありません。");
                if (code == 503) throw new Exception("履歴を取得できません。SDカードとSW4を確認してください。");
                if (code != 200) throw new Exception("履歴取得失敗 (HTTP " + code + ")");
                final long deadline = System.nanoTime() + 30_000_000_000L;
                try (java.io.InputStream stream = new java.io.FilterInputStream(connection.getInputStream()) {
                    private long received;
                    @Override public int read(byte[] b, int off, int len) throws java.io.IOException {
                        if (System.nanoTime() > deadline) throw new java.io.IOException("履歴の取得が30秒を超えました。");
                        int count = super.read(b, off, len);
                        if (count > 0) received += count;
                        if (received > 20*1024*1024) throw new java.io.IOException("履歴が20MBを超えています。");
                        return count;
                    }
                }) {
                    points = SensorHistory.read(new java.io.InputStreamReader(stream, StandardCharsets.UTF_8));
                }
                Log.i("SmartClock", "History parsed: " + points.size() + " records");
            } catch (Exception e) {
                Log.e("SmartClock", "History request failed", e);
                error = e.getMessage();
                if (error == null) error = "履歴を取得できません。接続を確認してください。";
            } finally { if (connection != null) connection.disconnect(); }
            final java.util.List<SensorHistory.Point> result = points;
            final String failure = error;
            handler.post(() -> {
                historyBusy = false;
                if (!active || requestGeneration != generation) { historyAttempted = false; return; }
                if (failure != null) {
                    historyStatus.setText("履歴更新失敗（グラフは前回取得分）: " + failure);
                } else {
                    setHistoryPoints(result);
                    historyStatus.setText(result.size() + "件 / 履歴取得 " + java.text.DateFormat.getTimeInstance().format(new java.util.Date()));
                }
            });
        });
    }

    @Override protected void onStart() { super.onStart(); active = true; fetch(); }
    @Override protected void onStop() {
        active = false; generation++; lastSequence = -1; handler.removeCallbacks(refresh); super.onStop();
    }
    @Override protected void onDestroy() { worker.shutdownNow(); historyWorker.shutdownNow(); super.onDestroy(); }
}
