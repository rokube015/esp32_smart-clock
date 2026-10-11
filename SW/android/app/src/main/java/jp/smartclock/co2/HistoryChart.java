package jp.smartclock.co2;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.view.MotionEvent;
import android.view.View;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

final class HistoryChart extends View {
    static final int[] COLORS = {Color.rgb(0,112,125), Color.rgb(180,75,36), Color.rgb(45,105,175), Color.rgb(115,75,155)};
    private final String[] names = {"CO₂", "温度", "湿度", "気圧"};
    private final String[] units = {"ppm", "℃", "%", "hPa"};
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float density;
    private List<SensorHistory.Point> points = new ArrayList<>(), visible = new ArrayList<>();
    private long range = 24 * 3600000L;
    private int selected = -1, mask = 15;
    private float plotLeft, plotRight;
    HistoryChart(Context context) {
        super(context); density = getResources().getDisplayMetrics().density;
        setBackgroundColor(Color.WHITE);
        setContentDescription("複数センサ履歴グラフ。タップで実際の測定値を表示します。");
    }
    void setMetrics(int selectedMask) { mask = selectedMask & 15; selected = -1; invalidate(); }
    void setPoints(List<SensorHistory.Point> data) { points = data; filter(); }
    void setRange(long duration) { range = duration; filter(); }
    private void filter() {
        visible = new ArrayList<>();
        long cutoff = points.isEmpty() || range==0 ? Long.MIN_VALUE : points.get(points.size()-1).time-range;
        for (SensorHistory.Point point : points) if (point.time>=cutoff) visible.add(point);
        selected=-1; invalidate();
    }
    private double value(SensorHistory.Point p, int metric) {
        switch(metric) { case 1: return p.temperature; case 2: return p.humidity; case 3: return p.pressure; default: return p.ppm; }
    }
    private String formatted(double value, int metric) { return String.format(Locale.JAPAN, metric==0 ? "%.0f" : "%.1f", value); }
    private String time(long millis, String pattern) {
        SimpleDateFormat format = new SimpleDateFormat(pattern, Locale.JAPAN);
        format.setTimeZone(TimeZone.getTimeZone("Asia/Tokyo"));
        return format.format(new java.util.Date(millis));
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        paint.setStyle(Paint.Style.FILL); paint.setTextSize(12*density); paint.setColor(Color.DKGRAY);
        if (mask==0 || visible.isEmpty()) {
            canvas.drawText(mask==0 ? "表示する項目を選択してください" : "表示できる記録がありません", 8*density, 35*density, paint); return;
        }
        float top=116*density, bottom=getHeight()-44*density;
        double[] min = new double[4], max = new double[4];
        for(int metric=0;metric<4;metric++) {
            min[metric]=Double.POSITIVE_INFINITY; max[metric]=Double.NEGATIVE_INFINITY;
            for(SensorHistory.Point p:visible) { min[metric]=Math.min(min[metric],value(p,metric)); max[metric]=Math.max(max[metric],value(p,metric)); }
        }
        double[] axisMin = new double[4], axisMax = new double[4];
        float[] axisWidth = new float[4], axisX = new float[4];
        boolean[] onLeft = new boolean[4];
        float leftWidth=0, rightWidth=0;
        int axisCount=0;
        paint.setTextSize(10*density);
        for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) {
            double padding=Math.max((max[metric]-min[metric])*0.15,metric==0 ? 100 : metric==3 ? 0.5 : 0.2);
            axisMin[metric]=min[metric]-padding; axisMax[metric]=max[metric]+padding;
            if(metric==0 || metric==2) axisMin[metric]=Math.max(0,axisMin[metric]);
            float width=paint.measureText(units[metric]);
            for(int i=0;i<=4;i++) width=Math.max(width,paint.measureText(formatted(axisMin[metric]+(axisMax[metric]-axisMin[metric])*i/4,metric)));
            axisWidth[metric]=width+10*density;
            onLeft[metric]=(axisCount++ % 2)==0;
            if(onLeft[metric]) leftWidth+=axisWidth[metric]; else rightWidth+=axisWidth[metric];
        }
        float left=leftWidth+2*density, right=getWidth()-rightWidth-2*density;
        plotLeft=left; plotRight=right;
        if(right<=left || bottom<=top) return;
        float leftOffset=0, rightOffset=0;
        for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) {
            axisX[metric]=onLeft[metric] ? left-leftOffset : right+rightOffset;
            if(onLeft[metric]) leftOffset+=axisWidth[metric]; else rightOffset+=axisWidth[metric];
        }
        paint.setTextSize(12*density);
        int row=0;
        for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) {
            paint.setColor(COLORS[metric]);
            String text=names[metric]+"  ";
            text += selected>=0 ? formatted(value(visible.get(selected),metric),metric)+" "+units[metric]
                : formatted(min[metric],metric)+"〜"+formatted(max[metric],metric)+" "+units[metric];
            canvas.drawText(text,8*density,(18+18*row++)*density,paint);
        }
        paint.setColor(Color.DKGRAY);
        canvas.drawText(selected>=0 ? time(visible.get(selected).time,"MM/dd HH:mm:ss") : "タップで実測値を表示（日本時間）",8*density,94*density,paint);
        paint.setStrokeWidth(density);
        for(int i=0;i<=4;i++) {
            float y=bottom-(bottom-top)*i/4;
            paint.setColor(Color.rgb(225,230,235)); canvas.drawLine(left,y,right,y,paint);
        }
        paint.setTextSize(10*density);
        for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) {
            paint.setColor(COLORS[metric]);
            float x=axisX[metric];
            canvas.drawLine(x,top,x,bottom,paint);
            paint.setTextAlign(onLeft[metric] ? Paint.Align.RIGHT : Paint.Align.LEFT);
            float labelX=x+(onLeft[metric] ? -4 : 4)*density;
            canvas.drawText(units[metric],labelX,top-8*density,paint);
            for(int i=0;i<=4;i++) {
                float y=bottom-(bottom-top)*i/4;
                canvas.drawLine(x,y,x+(onLeft[metric] ? -3 : 3)*density,y,paint);
                canvas.drawText(formatted(axisMin[metric]+(axisMax[metric]-axisMin[metric])*i/4,metric),labelX,y+3*density,paint);
            }
        }
        paint.setTextAlign(Paint.Align.LEFT); paint.setTextSize(12*density); paint.setColor(Color.DKGRAY);
        long first=visible.get(0).time,last=visible.get(visible.size()-1).time;
        String end=time(last,"MM/dd HH:mm");
        canvas.drawText(time(first,"MM/dd HH:mm"),left,bottom+20*density,paint);
        canvas.drawText(end,Math.max(left,right-paint.measureText(end)),bottom+38*density,paint);
        for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) {
            Path path=new Path();
            paint.setColor(COLORS[metric]);
            for(int i=0;i<visible.size();i++) {
                SensorHistory.Point p=visible.get(i);
                float x=last==first ? (left+right)/2 : left+(right-left)*(p.time-first)/(last-first);
                double fraction=(value(p,metric)-axisMin[metric])/(axisMax[metric]-axisMin[metric]);
                float y=bottom-(bottom-top)*(float)fraction;
                if(i==0 || p.time-visible.get(i-1).time>180000) path.moveTo(x,y); else path.lineTo(x,y);
                if(visible.size()<100) canvas.drawCircle(x,y,2*density,paint);
            }
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(2*density); canvas.drawPath(path,paint); paint.setStyle(Paint.Style.FILL);
        }
        if(selected>=0) {
            float x=last==first ? (left+right)/2 : left+(right-left)*(visible.get(selected).time-first)/(last-first);
            paint.setColor(Color.GRAY); paint.setStrokeWidth(density); canvas.drawLine(x,top,x,bottom,paint);
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if(visible.isEmpty() || mask==0) return super.onTouchEvent(event);
        if(event.getAction()==MotionEvent.ACTION_DOWN || event.getAction()==MotionEvent.ACTION_UP) {
            float fraction=Math.max(0,Math.min(1,(event.getX()-plotLeft)/Math.max(1,plotRight-plotLeft)));
            long first=visible.get(0).time,last=visible.get(visible.size()-1).time;
            long target=first+(long)((last-first)*fraction), distance=Long.MAX_VALUE;
            for(int i=0;i<visible.size();i++) if(Math.abs(visible.get(i).time-target)<distance) { selected=i; distance=Math.abs(visible.get(i).time-target); }
            SensorHistory.Point p=visible.get(selected);
            String description=time(p.time,"MM/dd HH:mm:ss");
            for(int metric=0;metric<4;metric++) if((mask & (1<<metric))!=0) description += " "+names[metric]+" "+formatted(value(p,metric),metric)+" "+units[metric];
            setContentDescription(description); invalidate();
            if(event.getAction()==MotionEvent.ACTION_UP) performClick(); return true;
        }
        return true;
    }
    @Override public boolean performClick() { super.performClick(); return true; }
}
