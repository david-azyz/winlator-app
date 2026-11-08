package com.winlator.xenvironment.components;

import android.content.Context;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.winlator.R;
import com.winlator.core.AppUtils;
import com.winlator.core.WineRegistryEditor;
import com.winlator.serial.ComPortMapping;
import com.winlator.serial.SerialBridge;
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

    private final File wineprefixDir;
    private final List<ComPortMapping> mappings;
    private final ArrayList<SerialBridge> bridges = new ArrayList<>();

    public SerialPortBridgeComponent(File wineprefixDir, List<ComPortMapping> mappings) {
        this.wineprefixDir = wineprefixDir;
        this.mappings = mappings;
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

        for (SerialBridge bridge : bridges) bridge.start();
    }

    @Override
    public void stop() {
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
