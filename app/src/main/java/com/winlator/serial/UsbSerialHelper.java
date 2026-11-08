package com.winlator.serial;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public abstract class UsbSerialHelper {
    public static final String ACTION_USB_PERMISSION = "com.winlator.USB_SERIAL_PERMISSION";

    public static class PortInfo {
        public final String deviceId;
        public final String label;

        public PortInfo(String deviceId, String label) {
            this.deviceId = deviceId;
            this.label = label;
        }

        @Override
        public String toString() {
            return label;
        }
    }

    public static UsbManager getUsbManager(Context context) {
        return (UsbManager)context.getSystemService(Context.USB_SERVICE);
    }

    public static List<UsbSerialDriver> findAllDrivers(Context context) {
        UsbManager usbManager = getUsbManager(context);
        if (usbManager == null) return new ArrayList<>();
        try {
            return UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);
        }
        catch (Exception e) {
            return new ArrayList<>();
        }
    }

    /** Lists every serial port of every connected USB serial adapter. */
    public static List<PortInfo> listPorts(Context context) {
        ArrayList<PortInfo> result = new ArrayList<>();
        for (UsbSerialDriver driver : findAllDrivers(context)) {
            UsbDevice device = driver.getDevice();
            List<UsbSerialPort> ports = driver.getPorts();
            for (int i = 0; i < ports.size(); i++) {
                String deviceId = ComPortMapping.makeDeviceId(device.getVendorId(), device.getProductId(), i);
                result.add(new PortInfo(deviceId, getDeviceLabel(driver, i, ports.size())));
            }
        }
        return result;
    }

    public static String getDeviceLabel(UsbSerialDriver driver, int portIndex, int portCount) {
        UsbDevice device = driver.getDevice();
        String name = null;
        try {
            name = device.getProductName();
        }
        catch (Exception e) {}

        if (name == null || name.trim().isEmpty()) {
            name = driver.getClass().getSimpleName().replace("SerialDriver", "");
        }

        String label = String.format(Locale.ENGLISH, "%s (%04x:%04x)", name.trim(), device.getVendorId(), device.getProductId());
        if (portCount > 1) label += " #"+(portIndex + 1);
        return label;
    }

    /** Finds the connected adapter matching the mapping, or null. */
    public static UsbSerialDriver findDriver(Context context, ComPortMapping mapping) {
        for (UsbSerialDriver driver : findAllDrivers(context)) {
            if (mapping.matches(driver.getDevice()) && mapping.portIndex < driver.getPorts().size()) return driver;
        }
        return null;
    }

    public static PendingIntent createPermissionIntent(Context context) {
        Intent intent = new Intent(ACTION_USB_PERMISSION);
        intent.setPackage(context.getPackageName());
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) flags |= PendingIntent.FLAG_MUTABLE;
        return PendingIntent.getBroadcast(context, 0, intent, flags);
    }

    /**
     * Shows the Android USB permission dialog and waits for the answer.
     * Must not be called from the UI thread.
     */
    public static boolean requestPermissionBlocking(Context context, UsbDevice device, long timeoutMs) {
        UsbManager usbManager = getUsbManager(context);
        if (usbManager == null) return false;
        if (usbManager.hasPermission(device)) return true;

        final Context appContext = context.getApplicationContext();
        final CountDownLatch latch = new CountDownLatch(1);
        final boolean[] granted = {false};

        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                if (!ACTION_USB_PERMISSION.equals(intent.getAction())) return;
                UsbDevice target = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (target != null && target.getDeviceId() != device.getDeviceId()) return;
                granted[0] = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                latch.countDown();
            }
        };

        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            appContext.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        }
        else appContext.registerReceiver(receiver, filter);

        try {
            usbManager.requestPermission(device, createPermissionIntent(appContext));
            latch.await(timeoutMs, TimeUnit.MILLISECONDS);
        }
        catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        finally {
            try {
                appContext.unregisterReceiver(receiver);
            }
            catch (Exception e) {}
        }

        return granted[0] || usbManager.hasPermission(device);
    }
}
