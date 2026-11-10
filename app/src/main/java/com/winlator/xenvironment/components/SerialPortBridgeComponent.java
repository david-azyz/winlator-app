package com.winlator.xenvironment.components;

import android.content.Context;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.winlator.R;
import com.winlator.core.AppUtils;
import com.winlator.core.WineRegistryEditor;
import com.winlator.serial.ComPortMapping;
import com.winlator.serial.SerialBridge;
import com.winlator.serial.TrafficLog;
import com.winlator.serial.UsbSerialHelper;
import com.winlator.xenvironment.EnvironmentComponent;

import java.io.File;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Exposes USB serial adapters to Wine as COM ports.
 *
 * For each mapping a pty is created and registered in
 * HKLM\Software\Wine\Ports (COMn = /dev/pts/N). Wine's mountmgr reads that key
 * at startup and creates the COMn device, so this component must start before
 * GuestProgramLauncherComponent.
 */
public class SerialPortBridgeComponent extends EnvironmentComponent implements SerialBridge.Callback {
    private static final String TAG = "SerialPortBridge";
    private static final String PORTS_KEY = "Software\\Wine\\Ports";
    private static final String PTY_PREFIX = "/dev/pts/";
    private static final long PERMISSION_TIMEOUT_MS = 30000;

    private static final String SHIM_ASSET = "serial/libwinlator_serialshim.so";

    private final File wineprefixDir;
    private final List<ComPortMapping> mappings;
    private final ArrayList<SerialBridge> bridges = new ArrayList<>();
    private Thread statusThread;
    private volatile boolean running = false;
    private String lastStatus = null;
    private long lastTrafficSequence = -1;

    public SerialPortBridgeComponent(File wineprefixDir, List<ComPortMapping> mappings) {
        this.wineprefixDir = wineprefixDir;
        this.mappings = mappings;
    }

    /**
     * Extracts the x86_64 ioctl shim (tools/serialshim) that must be preloaded
     * into Wine with BOX64_LD_PRELOAD. Android's SELinux policy blocks several
     * tty ioctls on ptys (TCGETS2, TCSETS2, TIOCINQ...), which Wine needs for
     * GetCommState/SetCommState/ClearCommError; the shim emulates them.
     * Returns the library path, or null if it could not be extracted.
     */
    public static String installIoctlShim(Context context) {
        File dir = new File(context.getFilesDir(), "serial");
        File shim = new File(dir, "libwinlator_serialshim.so");
        if (!dir.isDirectory()) dir.mkdirs();
        if (!copyAsset(context, SHIM_ASSET, shim)) return null;
        shim.setReadable(true, false);
        return shim.getPath();
    }

    private static boolean copyAsset(Context context, String asset, File target) {
        try (java.io.InputStream in = context.getAssets().open(asset);
             java.io.OutputStream out = new java.io.FileOutputStream(target)) {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
            return true;
        }
        catch (java.io.IOException e) {
            Log.e(TAG, "Failed to extract "+asset, e);
            return false;
        }
    }

    /** Puts the COM port tools (start menu > System Tools) into C:\windows. */
    private void installWindowsTools(Context context) {
        File windowsDir = new File(wineprefixDir, "drive_c/windows");
        if (!windowsDir.isDirectory()) return;
        copyAsset(context, "serial/comports.exe", new File(windowsDir, "comports.exe"));
    }

    private static String stateLabel(SerialBridge bridge) {
        switch (bridge.getState()) {
            case CONNECTED: return "Conectado";
            case LOOPBACK: return "Teste (loopback)";
            case EMPTY: return "Teste (sem dispositivo)";
            case PERMISSION_DENIED: return "Permiss\u00e3o USB negada";
            default: return "Desconectado";
        }
    }

    /**
     * Writes C:\windows\winlator-comports.ini, read by comports.exe
     * ("Gerenciador de Portas COM"). ANSI (windows-1252) so the Windows ANSI
     * profile functions show accented characters.
     */
    private void writeStatus() {
        StringBuilder sb = new StringBuilder();
        sb.append("[General]\r\nCount=").append(bridges.size()).append("\r\n");
        for (int i = 0; i < bridges.size(); i++) {
            SerialBridge bridge = bridges.get(i);
            ComPortMapping mapping = bridge.mapping;
            int[] line = bridge.getCurrentLineSettings();
            String device = mapping.isLoopback() ? "Loopback (teste)" : mapping.isEmpty() ? "Porta vazia (teste)" : bridge.getAdapterLabel();
            if (device.isEmpty()) device = mapping.isAuto() ? "Autom\u00e1tico (aguardando adaptador)" : "Adaptador USB "+mapping.getDeviceId();
            String format = mapping.getDataBits()+String.valueOf(mapping.format.charAt(1))+(line != null ? line[1] : 1);

            sb.append("[Port").append(i + 1).append("]\r\n");
            sb.append("Name=").append(mapping.getComName()).append("\r\n");
            sb.append("Device=").append(device.replace("\r", " ").replace("\n", " ")).append("\r\n");
            sb.append("DeviceId=").append(mapping.getDeviceId()).append("\r\n");
            sb.append("State=").append(stateLabel(bridge)).append("\r\n");
            sb.append("Baud=").append(line != null ? line[0] : 0).append("\r\n");
            sb.append("Format=").append(format).append("\r\n");
            sb.append("Sent=").append(bridge.getBytesSent()).append("\r\n");
            sb.append("Received=").append(bridge.getBytesReceived()).append("\r\n");
        }

        String status = sb.toString();
        if (status.equals(lastStatus)) return;
        lastStatus = status;

        File windowsDir = new File(wineprefixDir, "drive_c/windows");
        if (!windowsDir.isDirectory()) return;
        File target = new File(windowsDir, "winlator-comports.ini");
        File temp = new File(windowsDir, "winlator-comports.ini.tmp");
        try (java.io.FileOutputStream out = new java.io.FileOutputStream(temp)) {
            out.write(status.getBytes(java.nio.charset.Charset.forName("windows-1252")));
        }
        catch (Exception e) {
            return;
        }
        if (!temp.renameTo(target)) temp.delete();
    }

    /** Writes C:\\windows\\winlator-comtraffic.txt, shown by the Trafego button of comports.exe. */
    private void writeTraffic() {
        long sequence = TrafficLog.getSequence();
        if (sequence == lastTrafficSequence) return;
        lastTrafficSequence = sequence;

        File windowsDir = new File(wineprefixDir, "drive_c/windows");
        if (!windowsDir.isDirectory()) return;
        File target = new File(windowsDir, "winlator-comtraffic.txt");
        File temp = new File(windowsDir, "winlator-comtraffic.txt.tmp");
        try (java.io.FileOutputStream out = new java.io.FileOutputStream(temp)) {
            out.write(TrafficLog.getText().getBytes(java.nio.charset.Charset.forName("windows-1252")));
        }
        catch (Exception e) {
            return;
        }
        if (!temp.renameTo(target)) temp.delete();
    }

    private void runStatusWriter() {
        while (running) {
            writeStatus();
            writeTraffic();
            try {
                Thread.sleep(500);
            }
            catch (InterruptedException e) {
                return;
            }
        }
    }

    @Override
    public void start() {
        Context context = environment.getContext();

        for (ComPortMapping mapping : mappings) {
            SerialBridge bridge = new SerialBridge(context, mapping, this);
            if (bridge.openPty()) {
                bridges.add(bridge);
                Log.i(TAG, mapping.getComName()+" -> "+bridge.getSlavePath()+" ("+mapping.getDeviceId()+")");
            }
            else Log.e(TAG, "Failed to create pty for "+mapping.getComName());
        }

        updateWineRegistry();
        removeStaleDosdevicesLinks();
        installWindowsTools(context);

        for (SerialBridge bridge : bridges) bridge.start();

        TrafficLog.clear();
        for (SerialBridge bridge : bridges) TrafficLog.event(bridge.mapping.getComName(), "porta criada ("+bridge.getSlavePath()+"), modo "+bridge.mapping.getDeviceId());

        running = true;
        writeStatus();
        statusThread = new Thread(this::runStatusWriter, "SerialPortStatus");
        statusThread.start();
    }

    @Override
    public void stop() {
        running = false;
        if (statusThread != null) {
            statusThread.interrupt();
            statusThread = null;
        }
        for (SerialBridge bridge : bridges) bridge.stop();
        bridges.clear();
    }

    /** Points COMn to the pty of each bridge and removes COM ports left by previous sessions. */
    private void updateWineRegistry() {
        File systemRegFile = new File(wineprefixDir, "system.reg");
        if (!systemRegFile.isFile()) return;

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(systemRegFile)) {
            for (int i = 1; i <= 32; i++) {
                String name = "COM"+i;
                String value = registryEditor.getStringValue(PORTS_KEY, name);
                if (value != null && value.startsWith(PTY_PREFIX)) registryEditor.removeValue(PORTS_KEY, name);
            }

            for (SerialBridge bridge : bridges) {
                registryEditor.setStringValue(PORTS_KEY, bridge.mapping.getComName(), bridge.getSlavePath());
            }
        }
    }

    /** dosdevices/comN links pointing to ptys of a previous session would otherwise survive. */
    private void removeStaleDosdevicesLinks() {
        File[] files = (new File(wineprefixDir, "dosdevices")).listFiles();
        if (files == null) return;

        for (File file : files) {
            if (!file.getName().toLowerCase(Locale.ENGLISH).matches("com[0-9]+")) continue;
            try {
                if (!Files.isSymbolicLink(file.toPath())) continue;
                String target = Files.readSymbolicLink(file.toPath()).toString();
                if (target.startsWith(PTY_PREFIX)) file.delete();
            }
            catch (Exception e) {}
        }
    }

    @Override
    public void onAdapterConnected(SerialBridge bridge, String label) {
        Context context = environment.getContext();
        AppUtils.showToast(context, context.getString(R.string.com_port_connected, bridge.mapping.getComName(), label));
    }

    @Override
    public void onAdapterDisconnected(SerialBridge bridge) {
        Context context = environment.getContext();
        AppUtils.showToast(context, context.getString(R.string.com_port_disconnected, bridge.mapping.getComName()));
    }

    @Override
    public boolean onPermissionRequired(SerialBridge bridge, UsbSerialDriver driver) {
        Context context = environment.getContext();
        boolean granted = UsbSerialHelper.requestPermissionBlocking(context, driver.getDevice(), PERMISSION_TIMEOUT_MS);
        if (!granted) AppUtils.showToast(context, context.getString(R.string.com_port_permission_denied, bridge.mapping.getComName()));
        return granted;
    }
}
