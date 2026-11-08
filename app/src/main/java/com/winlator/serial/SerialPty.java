package com.winlator.serial;

/** Native pseudo-terminal helpers (cpp/winlator/src/serial_pty.c). */
public abstract class SerialPty {
    static {
        System.loadLibrary("winlator");
    }

    public static final int SETTING_BAUD_RATE = 0;
    public static final int SETTING_DATA_BITS = 1;
    public static final int SETTING_STOP_BITS = 2;
    public static final int SETTING_PARITY = 3;
    public static final int SETTING_RTS_CTS = 4;
    public static final int SETTING_XON_XOFF = 5;
    public static final int SETTINGS_COUNT = 6;

    /** Opens a new pty in raw mode. Returns {masterFd, slaveFd} or null. */
    public static native int[] openPty();

    public static native String getSlavePath(int masterFd);

    /** Returns bytes read, 0 on timeout, -1 on error. */
    public static native int read(int fd, byte[] buffer, int timeoutMs);

    /** Returns bytes written, or -1 on error. */
    public static native int write(int fd, byte[] buffer, int length, int timeoutMs);

    /** Fills out (length SETTINGS_COUNT) with the termios settings applied by Wine. */
    public static native boolean getLineSettings(int slaveFd, int[] out);

    public static native void close(int fd);
}
