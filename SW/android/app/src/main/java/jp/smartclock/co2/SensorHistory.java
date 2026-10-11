package jp.smartclock.co2;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.time.format.ResolverStyle;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;

/** CSV timestamps are the clock's Japanese local time, not the phone's time zone. */
final class SensorHistory {
    static final class Point {
        final long time;
        final int ppm;
        final double temperature, humidity, pressure;
        Point(long time, int ppm, double temperature, double humidity, double pressure) {
            this.time = time; this.ppm = ppm; this.temperature = temperature;
            this.humidity = humidity; this.pressure = pressure;
        }
    }
    private static final DateTimeFormatter FORMAT = DateTimeFormatter.ofPattern("uuuu/MM/dd HH:mm:ss")
        .withResolverStyle(ResolverStyle.STRICT);

    static List<Point> read(Reader input) throws IOException {
        TreeMap<Long, Point> points = new TreeMap<>();
        BufferedReader reader = new BufferedReader(input);
        String line;
        while ((line = reader.readLine()) != null) {
            String[] fields = line.split(",", -1);
            if (fields.length != 7) continue;
            try {
                long time = LocalDateTime.parse(fields[0].trim() + " " + fields[2].trim(), FORMAT)
                    .toInstant(ZoneOffset.ofHours(9)).toEpochMilli();
                int ppm = Integer.parseInt(fields[3].trim());
                if (ppm <= 0 || ppm > 65535) continue;
                double temperature = Double.parseDouble(fields[4].trim());
                double humidity = Double.parseDouble(fields[5].trim());
                double pressure = Double.parseDouble(fields[6].trim());
                if (!Double.isFinite(temperature) || !Double.isFinite(humidity) || !Double.isFinite(pressure)) continue;
                points.put(time, new Point(time, ppm, temperature, humidity, pressure));
                if (points.size() > 10000) points.pollFirstEntry();
            } catch (RuntimeException ignored) { /* Headers and malformed rows are skipped. */ }
        }
        return new ArrayList<>(points.values());
    }
}
