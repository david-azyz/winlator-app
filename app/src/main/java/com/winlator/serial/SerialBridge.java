package com.winlator.serial;

import android.content.Context;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.io.IOException;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Bridges one Wine COM port (the slave side of a pty) to one USB serial port.
 *
 * The pty exists for the whole session, so Wine always sees the COM port.
 * The USB adapter is (re)opened whenever it is plugged in, and the line
 * settings the Windows program sets with SetCommState (baud rate, data bits,
 * parity, stop bits, RTS/CTS) are mirrored to the adapter.
 */
public class SerialBridge implements SerialInputOutputManager.Listener {
    private static final String TAG = "SerialBridge";
    private static final int PTY_READ_TIMEOUT_MS = 50;
    private static final int PTY_WRITE_TIMEOUT_MS = 1000;
    private static final int USB_WRITE_TIMEOUT_MS = 2000;
    private static final int RECONNECT_INTERVAL_MS = 1500;
    private static final int LINE_SETTINGS_INTERVAL_MS = 50;

    public interface Callback {
        void onAdapterConnected(SerialBridge bridge, String label);

        void onAdapterDisconnected(SerialBridge bridge);

        /** Returns true if permission was granted. Called from the bridge thread. */
        boolean onPermissionRequired(SerialBridge bridge, UsbSerialDriver driver);
    }

    public final ComPortMapping mapping;
    private final Context context;
    private final Callback callback;
    private int masterFd = -1;
    private int slaveFd = -1;
    private String slavePath;
    private volatile boolean running = false;
    private Thread thread;
    private final Object portLock = new Object();
    private UsbSerialPort port;
    private UsbDeviceConnection connection;
    private SerialInputOutputManager ioManager;
    private final int[] lineSettings = new int[SerialPty.SETTINGS_COUNT];
    private final int[] appliedLineSettings = new int[SerialPty.SETTINGS_COUNT];
    private boolean permissionDenied = false;

    public enum State { WAITING_ADAPTER, PERMISSION_DENIED, CONNECTED, LOOPBACK, EMPTY }

    private volatile State state = State.WAITING_ADAPTER;
    private volatile String adapterLabel = "";
    private final AtomicLong bytesSent = new AtomicLong();
    private final AtomicLong bytesReceived = new AtomicLong();

    public State getState() {
        return state;
    }

    public String getAdapterLabel() {
        return adapterLabel;
    }

    /** Bytes written by the Windows program (sent to the device). */
    public long getBytesSent() {
        return bytesSent.get();
    }

    /** Bytes delivered to the Windows program (received from the device). */
    public long getBytesReceived() {
        return bytesReceived.get();
    }

    /** {baudRate, stopBits} currently set by the Windows program, or null. */
    public int[] getCurrentLineSettings() {
        int[] settings = new int[SerialPty.SETTINGS_COUNT];
        if (slaveFd == -1 || !SerialPty.getLineSettings(slaveFd, settings)) return null;
        return new int[]{settings[SerialPty.SETTING_BAUD_RATE], settings[SerialPty.SETTING_STOP_BITS]};
    }

    public SerialBridge(Context context, ComPortMapping mapping, Callback callback) {
        this.context = context;
        this.mapping = mapping;
        this.callback = callback;
    }

    /** Creates the pty. Returns false if it could not be created. */
    public boolean openPty() {
        int[] fds = SerialPty.openPty();
        if (fds == null) return false;
        masterFd = fds[0];
        slaveFd = fds[1];
        slavePath = SerialPty.getSlavePath(masterFd);
        if (slavePath == null) {
            closePty();
            return false;
        }
        return true;
    }

    /** Unix path Wine must open for this COM port (e.g. /dev/pts/3). */
    public String getSlavePath() {
        return slavePath;
    }

    public boolean isAdapterConnected() {
        synchronized (portLock) {
            return port != null;
        }
    }

    public void start() {
        if (running || masterFd == -1) return;
        running = true;
        thread = new Thread(this::run, "SerialBridge-"+mapping.getComName());
        thread.start();
    }

    public void stop() {
        running = false;
        if (thread != null) {
            thread.interrupt();
            try {
                thread.join(1000);
            }
            catch (InterruptedException e) {}
            thread = null;
        }
        closeAdapter(false);
        closePty();
    }

    private void closePty() {
        if (masterFd != -1) SerialPty.close(masterFd);
        if (slaveFd != -1) SerialPty.close(slaveFd);
        masterFd = -1;
        slaveFd = -1;
    }

    private void run() {
        if (mapping.isLoopback()) {
            runLoopback();
            return;
        }
        if (mapping.isEmpty()) {
            runEmpty();
            return;
        }

        byte[] buffer = new byte[4096];
        long lastConnectAttempt = 0;
        long lastLineSettingsCheck = 0;

        while (running) {
            long now = System.currentTimeMillis();

            if (!isAdapterConnected() && now - lastConnectAttempt >= RECONNECT_INTERVAL_MS) {
                lastConnectAttempt = now;
                tryOpenAdapter();
            }

            if (now - lastLineSettingsCheck >= LINE_SETTINGS_INTERVAL_MS) {
                lastLineSettingsCheck = now;
                applyLineSettingsIfChanged(false);
            }

            int count = SerialPty.read(masterFd, buffer, PTY_READ_TIMEOUT_MS);
            if (count < 0) {
                Log.e(TAG, mapping.getComName()+": pty read failed");
                sleep(100);
                continue;
            }

            if (count > 0) {
                UsbSerialPort currentPort;
                synchronized (portLock) {
                    currentPort = port;
                }
                // Without an adapter the data is dropped, like a real port with nothing attached.
                if (currentPort != null) {
                    applyLineSettingsIfChanged(false);
                    try {
                        currentPort.write(buffer, count, USB_WRITE_TIMEOUT_MS);
                        bytesSent.addAndGet(count);
                    }
                    catch (IOException e) {
                        Log.w(TAG, mapping.getComName()+": USB write failed: "+e.getMessage());
                        closeAdapter(true);
                    }
                }
            }
        }
    }

    /** Test device: everything the Windows program writes comes back to it. */
    private void runLoopback() {
        byte[] buffer = new byte[4096];
        state = State.LOOPBACK;
        adapterLabel = "Loopback";
        callback.onAdapterConnected(this, "Loopback");
        while (running) {
            int count = SerialPty.read(masterFd, buffer, PTY_READ_TIMEOUT_MS);
            if (count > 0) {
                bytesSent.addAndGet(count);
                if (SerialPty.write(masterFd, buffer, count, PTY_WRITE_TIMEOUT_MS) > 0) bytesReceived.addAndGet(count);
            }
            else if (count < 0) sleep(100);
        }
    }

    /** Test port with nothing attached: data written by the program is counted and dropped. */
    private void runEmpty() {
        byte[] buffer = new byte[4096];
        state = State.EMPTY;
        adapterLabel = "Porta vazia";
        callback.onAdapterConnected(this, "Porta vazia (teste)");
        while (running) {
            int count = SerialPty.read(masterFd, buffer, PTY_READ_TIMEOUT_MS);
            if (count > 0) bytesSent.addAndGet(count);
            else if (count < 0) sleep(100);
        }
    }

    private void tryOpenAdapter() {
        UsbSerialDriver driver = UsbSerialHelper.findDriver(context, mapping);
        if (driver == null) {
            permissionDenied = false;
            return;
        }

        UsbManager usbManager = UsbSerialHelper.getUsbManager(context);
        if (!usbManager.hasPermission(driver.getDevice())) {
            if (permissionDenied) return;
            if (!callback.onPermissionRequired(this, driver)) {
                permissionDenied = true;
                state = State.PERMISSION_DENIED;
                return;
            }
        }

        UsbDeviceConnection newConnection = usbManager.openDevice(driver.getDevice());
        if (newConnection == null) return;

        UsbSerialPort newPort = driver.getPorts().get(mapping.portIndex);
        try {
            newPort.open(newConnection);
        }
        catch (IOException e) {
            Log.w(TAG, mapping.getComName()+": failed to open USB port: "+e.getMessage());
            try {
                newPort.close();
            }
            catch (IOException ignored) {}
            newConnection.close();
            return;
        }

        synchronized (portLock) {
            port = newPort;
            connection = newConnection;
        }

        applyLineSettingsIfChanged(true);

        // Wine cannot drive modem lines on a pty, so assert DTR/RTS like most
        // Windows programs do when they open a port.
        try {
            newPort.setDTR(true);
        }
        catch (Exception e) {}
        try {
            newPort.setRTS(true);
        }
        catch (Exception e) {}

        SerialInputOutputManager manager = new SerialInputOutputManager(newPort, this);
        synchronized (portLock) {
            ioManager = manager;
        }
        manager.start();

        adapterLabel = UsbSerialHelper.getDeviceLabel(driver, mapping.portIndex, driver.getPorts().size());
        state = State.CONNECTED;
        callback.onAdapterConnected(this, adapterLabel);
    }

    private void closeAdapter(boolean notify) {
        UsbSerialPort oldPort;
        UsbDeviceConnection oldConnection;
        SerialInputOutputManager oldManager;
        synchronized (portLock) {
            oldPort = port;
            oldConnection = connection;
            oldManager = ioManager;
            port = null;
            connection = null;
            ioManager = null;
        }

        if (oldPort == null) return;

        if (oldManager != null) {
            oldManager.setListener(null);
            oldManager.stop();
        }

        try {
            oldPort.close();
        }
        catch (Exception e) {}
        if (oldConnection != null) oldConnection.close();

        synchronized (appliedLineSettings) {
            Arrays.fill(appliedLineSettings, 0);
        }
        state = State.WAITING_ADAPTER;
        if (notify && running) callback.onAdapterDisconnected(this);
    }

    private void applyLineSettingsIfChanged(boolean force) {
        synchronized (appliedLineSettings) {
            applyLineSettingsLocked(force);
        }
    }

    private void applyLineSettingsLocked(boolean force) {
        if (slaveFd == -1 || !SerialPty.getLineSettings(slaveFd, lineSettings)) return;

        UsbSerialPort currentPort;
        synchronized (portLock) {
            currentPort = port;
        }
        if (currentPort == null) return;
        if (!force && Arrays.equals(lineSettings, appliedLineSettings)) return;

        int stopBits = lineSettings[SerialPty.SETTING_STOP_BITS] == 2 ? UsbSerialPort.STOPBITS_2 : UsbSerialPort.STOPBITS_1;
        try {
            // Data bits and parity come from the user setting: the pty driver forces CS8/no parity.
            currentPort.setParameters(lineSettings[SerialPty.SETTING_BAUD_RATE], mapping.getDataBits(), stopBits, mapping.getParity());
        }
        catch (Exception e) {
            Log.w(TAG, mapping.getComName()+": setParameters failed: "+e.getMessage());
        }

        try {
            boolean rtsCts = lineSettings[SerialPty.SETTING_RTS_CTS] == 1;
            UsbSerialPort.FlowControl flowControl = rtsCts ? UsbSerialPort.FlowControl.RTS_CTS : UsbSerialPort.FlowControl.NONE;
            if (currentPort.getSupportedFlowControl().contains(flowControl)) currentPort.setFlowControl(flowControl);
        }
        catch (Exception e) {}

        System.arraycopy(lineSettings, 0, appliedLineSettings, 0, lineSettings.length);
    }

    @Override
    public void onNewData(byte[] data) {
        int fd = masterFd;
        if (fd == -1 || data.length == 0) return;
        int written = SerialPty.write(fd, data, data.length, PTY_WRITE_TIMEOUT_MS);
        if (written > 0) bytesReceived.addAndGet(written);
        if (written < 0) {
            Log.w(TAG, mapping.getComName()+": pty write failed");
        }
    }

    @Override
    public void onRunError(Exception e) {
        Log.w(TAG, mapping.getComName()+": USB connection lost: "+e.getMessage());
        // Called from the IO manager thread; close from another thread to avoid joining itself.
        new Thread(() -> closeAdapter(true)).start();
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        }
        catch (InterruptedException e) {}
    }
}
