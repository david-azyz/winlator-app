package com.winlator.serial;

import android.hardware.usb.UsbDevice;

import java.util.ArrayList;
import java.util.Locale;

/**
 * Maps a Windows COM port number to a USB serial adapter.
 *
 * Stored in the container config as "comPorts", e.g. "COM1=1a86:7523:0:8N,COM3=0403:6001:1:7E"
 * (COMn=vendorId:productId:portIndex:format, ids in hex).
 *
 * The format (data bits + parity) is configured by the user because the Linux
 * pty driver always reports CS8 without parity, so the values set by the
 * Windows program are lost. Baud rate, stop bits and RTS/CTS do come from Wine.
 */
public class ComPortMapping {
    public static final int MAX_COM_PORTS = 4;
    public static final String DEFAULT_FORMAT = "8N";
    /** Virtual device that sends back everything it receives, for testing without hardware. */
    public static final String LOOPBACK_ID = "loopback";
    /** Data bits + parity (N none, E even, O odd, M mark, S space). */
    public static final String[] FORMATS = {"8N", "8E", "8O", "7E", "7O", "7N", "8M", "8S"};

    public final int comNumber;
    public final int vendorId;
    public final int productId;
    public final int portIndex;
    public final String format;

    public ComPortMapping(int comNumber, int vendorId, int productId, int portIndex, String format) {
        this.comNumber = comNumber;
        this.vendorId = vendorId;
        this.productId = productId;
        this.portIndex = portIndex;
        this.format = isValidFormat(format) ? format : DEFAULT_FORMAT;
    }

    public static boolean isValidFormat(String format) {
        if (format == null) return false;
        for (String item : FORMATS) if (item.equals(format)) return true;
        return false;
    }

    public int getDataBits() {
        return format.charAt(0) == '7' ? 7 : 8;
    }

    /** Parity in usb-serial-for-android values: 0 none, 1 odd, 2 even, 3 mark, 4 space. */
    public int getParity() {
        switch (format.charAt(1)) {
            case 'O': return 1;
            case 'E': return 2;
            case 'M': return 3;
            case 'S': return 4;
            default: return 0;
        }
    }

    public String getComName() {
        return "COM"+comNumber;
    }

    public boolean isLoopback() {
        return vendorId == -1;
    }

    public String getDeviceId() {
        return isLoopback() ? LOOPBACK_ID : makeDeviceId(vendorId, productId, portIndex);
    }

    public boolean matches(UsbDevice device) {
        return !isLoopback() && device.getVendorId() == vendorId && device.getProductId() == productId;
    }

    public static String makeDeviceId(int vendorId, int productId, int portIndex) {
        return String.format(Locale.ENGLISH, "%04x:%04x:%d", vendorId, productId, portIndex);
    }

    @Override
    public String toString() {
        return getComName()+"="+getDeviceId()+":"+format;
    }

    public static ArrayList<ComPortMapping> parse(String value) {
        ArrayList<ComPortMapping> result = new ArrayList<>();
        if (value == null || value.isEmpty()) return result;

        for (String item : value.split(",")) {
            try {
                String[] parts = item.trim().split("=");
                if (parts.length != 2 || !parts[0].toUpperCase(Locale.ENGLISH).startsWith("COM")) continue;

                int comNumber = Integer.parseInt(parts[0].substring(3));
                String[] ids = parts[1].split(":");
                if (comNumber < 1 || comNumber > MAX_COM_PORTS) continue;

                if (ids[0].equalsIgnoreCase(LOOPBACK_ID)) {
                    String format = ids.length > 1 ? ids[1].toUpperCase(Locale.ENGLISH) : DEFAULT_FORMAT;
                    result.add(new ComPortMapping(comNumber, -1, -1, 0, format));
                    continue;
                }

                if (ids.length < 2) continue;

                int vendorId = Integer.parseInt(ids[0], 16);
                int productId = Integer.parseInt(ids[1], 16);
                int portIndex = ids.length > 2 ? Integer.parseInt(ids[2]) : 0;
                String format = ids.length > 3 ? ids[3].toUpperCase(Locale.ENGLISH) : DEFAULT_FORMAT;
                result.add(new ComPortMapping(comNumber, vendorId, productId, portIndex, format));
            }
            catch (NumberFormatException e) {}
        }
        return result;
    }

    public static String toString(Iterable<ComPortMapping> mappings) {
        StringBuilder sb = new StringBuilder();
        for (ComPortMapping mapping : mappings) {
            if (sb.length() > 0) sb.append(",");
            sb.append(mapping);
        }
        return sb.toString();
    }
}
