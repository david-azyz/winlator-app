package com.winlator.serial;

import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.Date;
import java.util.Locale;

/**
 * Recent COM port traffic and events of all bridges, shown by the
 * "Gerenciador de Portas COM" (Trafego button) to diagnose communication.
 * Each line starts with a sequence number so the viewer can hide old lines.
 */
public abstract class TrafficLog {
    private static final int MAX_LINES = 300;
    private static final int MAX_BYTES_PER_LINE = 32;
    private static final ArrayDeque<String> lines = new ArrayDeque<>();
    private static long sequence = 0;
    private static final SimpleDateFormat timeFormat = new SimpleDateFormat("HH:mm:ss.SSS", Locale.ENGLISH);

    public static synchronized void clear() {
        lines.clear();
        sequence = 0;
    }

    /** Monotonic counter: changes whenever a line is added. */
    public static synchronized long getSequence() {
        return sequence;
    }

    /** Logs data; direction is "TX" (program to device) or "RX" (device to program). */
    public static void data(String port, String direction, byte[] data, int offset, int length) {
        if (length <= 0) return;
        StringBuilder sb = new StringBuilder();
        sb.append(direction).append(' ').append(length).append(length == 1 ? " byte:" : " bytes:");
        int shown = Math.min(length, MAX_BYTES_PER_LINE);
        for (int i = 0; i < shown; i++) sb.append(String.format(Locale.ENGLISH, " %02X", data[offset + i] & 0xFF));
        if (length > shown) sb.append(" ...");
        add(port, sb.toString());
    }

    public static void event(String port, String text) {
        add(port, "-- "+text);
    }

    private static synchronized void add(String port, String text) {
        sequence++;
        String time;
        synchronized (timeFormat) {
            time = timeFormat.format(new Date());
        }
        lines.addLast("#"+sequence+" "+time+" "+port+" "+text);
        while (lines.size() > MAX_LINES) lines.removeFirst();
    }

    public static synchronized String getText() {
        StringBuilder sb = new StringBuilder();
        for (String line : lines) sb.append(line).append("\r\n");
        return sb.toString();
    }
}
