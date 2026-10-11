package jp.smartclock.co2;

import java.io.StringReader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.List;

public final class SensorHistoryTest {
    public static void main(String[] args) throws Exception {
        List<SensorHistory.Point> points = SensorHistory.read(new StringReader(
            "YYYY/MM/DD, week, HH:MM:SS, CO2[rpm], Temperature[degree], Humidity[%], Pressure[hPa]\n"
            + "2026/10/11, Sun, 00:01:00, 700, 24, 40, 1013\n"
            + "2026/10/10, Sat, 23:59:00, 600, 24, 40, 1013\n"
            + "2026/10/11, Sun, 00:01:00, 710, 24, 40, 1013\n"
            + "2026/02/30, Mon, 00:00:00, 500, 24, 40, 1013\n"
            + "2026/10/11, Sun, 00:02:00, NaN, 24, 40, 1013\n"
            + "2026/10/11, Sun, 00:03:00, 0, 24, 40, 1013\n"
            + "truncated,row\n"));
        if (points.size()!=2 || points.get(0).ppm!=600 || points.get(1).ppm!=710)
            throw new AssertionError("Sorting, duplicate, invalid row handling");
        if (points.get(0).temperature!=24 || points.get(0).humidity!=40 || points.get(0).pressure!=1013)
            throw new AssertionError("Environmental fields");
        List<SensorHistory.Point> environmental = SensorHistory.read(new StringReader(
            "2026/10/11, Sun, 00:00:00, 600, -2.35, 45.67, 1018.24\n"
            + "2026/10/11, Sun, 00:01:00, 600, NaN, 45, 1013\n"
            + "2026/10/11, Sun, 00:02:00, 600, 25, Infinity, 1013\n"));
        if(environmental.size()!=1 || environmental.get(0).temperature!=-2.35
            || environmental.get(0).humidity!=45.67 || environmental.get(0).pressure!=1018.24)
            throw new AssertionError("Decimal, negative, non-finite environmental values");
        if (points.get(1).time-points.get(0).time!=120000 || points.get(0).time!=1791644340000L)
            throw new AssertionError("JST date boundary");
        StringBuilder many = new StringBuilder();
        LocalDateTime start = LocalDateTime.of(2026,10,1,0,0);
        DateTimeFormatter format = DateTimeFormatter.ofPattern("uuuu/MM/dd, 'X', HH:mm:ss");
        // Reverse order ensures retention uses timestamps rather than file order.
        for (int i=10004;i>=0;i--) many.append(start.plusMinutes(i).format(format)).append(", 650, 24, 40, 1013\n");
        points=SensorHistory.read(new StringReader(many.toString()));
        if(points.size()!=10000 || points.get(0).time!=start.plusMinutes(5).toInstant(ZoneOffset.ofHours(9)).toEpochMilli())
            throw new AssertionError("Newest 10000 records");
        if(args.length>0) {
            try(java.io.Reader reader=Files.newBufferedReader(Path.of(args[0]))) {
                points=SensorHistory.read(reader);
                if(points.isEmpty()) throw new AssertionError("Real CSV fixture");
                System.out.println("Real CSV: "+points.size()+" records");
            }
        }
        System.out.println("PASS: CSV parsing, JST timestamps, sorting, duplicates, invalid rows, bounded retention");
    }
}
