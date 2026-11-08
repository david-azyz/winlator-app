/*
 * Pseudo-terminal helpers used by the USB serial -> Wine COM port bridge.
 *
 * Wine maps COMx to a Unix tty path (HKLM\Software\Wine\Ports). Android apps
 * cannot open /dev/ttyUSB*, so the app opens the USB adapter through the
 * Android USB Host API and exposes it to Wine through a pty: Wine opens the
 * slave side (/dev/pts/N) as if it were a real serial port and the app copies
 * bytes between the master side and the USB adapter.
 */
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <android/log.h>

/* Kernel struct termios2 (asm-generic layout, used by arm64/x86_64), declared
 * here so the code does not depend on which libc headers expose it. */
struct winlator_termios2 {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_line;
    unsigned char c_cc[19];
    unsigned int c_ispeed;
    unsigned int c_ospeed;
};
#define WINLATOR_TCGETS2 _IOR('T', 0x2A, struct winlator_termios2)

#define LOG_TAG "SerialPty"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int speedToBaud(speed_t speed) {
    switch (speed) {
        case B50: return 50;
        case B75: return 75;
        case B110: return 110;
        case B134: return 134;
        case B150: return 150;
        case B200: return 200;
        case B300: return 300;
        case B600: return 600;
        case B1200: return 1200;
        case B1800: return 1800;
        case B2400: return 2400;
        case B4800: return 4800;
        case B9600: return 9600;
        case B19200: return 19200;
        case B38400: return 38400;
        case B57600: return 57600;
        case B115200: return 115200;
        case B230400: return 230400;
        case B460800: return 460800;
        case B500000: return 500000;
        case B576000: return 576000;
        case B921600: return 921600;
        case B1000000: return 1000000;
        case B1152000: return 1152000;
        case B1500000: return 1500000;
        case B2000000: return 2000000;
        case B2500000: return 2500000;
        case B3000000: return 3000000;
        case B3500000: return 3500000;
        case B4000000: return 4000000;
        default: return 0;
    }
}

JNIEXPORT jintArray JNICALL
Java_com_winlator_serial_SerialPty_openPty(JNIEnv *env, jclass clazz) {
    int masterFd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (masterFd < 0) {
        LOGE("posix_openpt failed: %s", strerror(errno));
        return NULL;
    }

    if (grantpt(masterFd) != 0 || unlockpt(masterFd) != 0) {
        LOGE("grantpt/unlockpt failed: %s", strerror(errno));
        close(masterFd);
        return NULL;
    }

    char slavePath[128];
    if (ptsname_r(masterFd, slavePath, sizeof(slavePath)) != 0) {
        LOGE("ptsname_r failed: %s", strerror(errno));
        close(masterFd);
        return NULL;
    }

    /* The bridge keeps the slave open for the whole session: without an open
     * slave, reads on the master return EIO and the line settings set by Wine
     * would be lost between opens. */
    int slaveFd = open(slavePath, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slaveFd < 0) {
        LOGE("open(%s) failed: %s", slavePath, strerror(errno));
        close(masterFd);
        return NULL;
    }

    /* Raw mode by default, so nothing is echoed or translated even if the
     * Windows program never calls SetCommState. 9600 8N1 like a fresh port. */
    struct termios tio;
    if (tcgetattr(slaveFd, &tio) == 0) {
        cfmakeraw(&tio);
        tio.c_cflag |= CLOCAL | CREAD;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        cfsetispeed(&tio, B9600);
        cfsetospeed(&tio, B9600);
        tcsetattr(slaveFd, TCSANOW, &tio);
    }

    int flags = fcntl(masterFd, F_GETFL);
    if (flags != -1) fcntl(masterFd, F_SETFL, flags | O_NONBLOCK);

    jintArray result = (*env)->NewIntArray(env, 2);
    jint fds[2] = {masterFd, slaveFd};
    (*env)->SetIntArrayRegion(env, result, 0, 2, fds);
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_winlator_serial_SerialPty_getSlavePath(JNIEnv *env, jclass clazz, jint masterFd) {
    char slavePath[128];
    if (ptsname_r(masterFd, slavePath, sizeof(slavePath)) != 0) return NULL;
    return (*env)->NewStringUTF(env, slavePath);
}

/* Returns the number of bytes read, 0 on timeout, -1 on error. */
JNIEXPORT jint JNICALL
Java_com_winlator_serial_SerialPty_read(JNIEnv *env, jclass clazz, jint fd, jbyteArray buffer, jint timeoutMs) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int res = poll(&pfd, 1, timeoutMs);
    if (res == 0) return 0;
    if (res < 0) return errno == EINTR ? 0 : -1;
    if (!(pfd.revents & POLLIN)) return (pfd.revents & (POLLERR | POLLNVAL)) ? -1 : 0;

    jsize capacity = (*env)->GetArrayLength(env, buffer);
    jbyte *data = (*env)->GetByteArrayElements(env, buffer, NULL);
    ssize_t count = read(fd, data, capacity);
    (*env)->ReleaseByteArrayElements(env, buffer, data, count > 0 ? 0 : JNI_ABORT);

    if (count < 0) return (errno == EAGAIN || errno == EINTR || errno == EIO) ? 0 : -1;
    return (jint)count;
}

/* Writes the whole buffer (waiting up to timeoutMs while the pty is full).
 * Returns the number of bytes written, or -1 on error. */
JNIEXPORT jint JNICALL
Java_com_winlator_serial_SerialPty_write(JNIEnv *env, jclass clazz, jint fd, jbyteArray buffer, jint length, jint timeoutMs) {
    jbyte *data = (*env)->GetByteArrayElements(env, buffer, NULL);
    int written = 0;

    while (written < length) {
        ssize_t count = write(fd, data + written, length - written);
        if (count > 0) {
            written += count;
            continue;
        }

        if (count < 0 && (errno == EAGAIN || errno == EINTR)) {
            struct pollfd pfd = {fd, POLLOUT, 0};
            if (poll(&pfd, 1, timeoutMs) <= 0) break;
            continue;
        }

        if (written == 0) written = -1;
        break;
    }

    (*env)->ReleaseByteArrayElements(env, buffer, data, JNI_ABORT);
    return written;
}

/* Fills out[] with the line settings the Windows program applied through
 * SetCommState: {baudRate, dataBits, stopBits, parity, rtsCtsFlow, xonXoffFlow}.
 * parity uses usb-serial-for-android values: 0 none, 1 odd, 2 even, 3 mark, 4 space. */
JNIEXPORT jboolean JNICALL
Java_com_winlator_serial_SerialPty_getLineSettings(JNIEnv *env, jclass clazz, jint slaveFd, jintArray out) {
    struct termios tio;
    if (tcgetattr(slaveFd, &tio) != 0) return JNI_FALSE;

    int baudRate = 0;
    struct winlator_termios2 tio2;
    if (ioctl(slaveFd, WINLATOR_TCGETS2, &tio2) == 0) baudRate = (int)tio2.c_ospeed;
    if (baudRate <= 0) baudRate = speedToBaud(cfgetospeed(&tio));
    if (baudRate <= 0) baudRate = 9600;

    int dataBits;
    switch (tio.c_cflag & CSIZE) {
        case CS5: dataBits = 5; break;
        case CS6: dataBits = 6; break;
        case CS7: dataBits = 7; break;
        default: dataBits = 8; break;
    }

    int stopBits = (tio.c_cflag & CSTOPB) ? 2 : 1;

    int parity = 0;
    if (tio.c_cflag & PARENB) {
#ifdef CMSPAR
        if (tio.c_cflag & CMSPAR) parity = (tio.c_cflag & PARODD) ? 3 : 4;
        else
#endif
        parity = (tio.c_cflag & PARODD) ? 1 : 2;
    }

    jint values[6] = {
        baudRate,
        dataBits,
        stopBits,
        parity,
        (tio.c_cflag & CRTSCTS) ? 1 : 0,
        (tio.c_iflag & (IXON | IXOFF)) ? 1 : 0
    };
    (*env)->SetIntArrayRegion(env, out, 0, 6, values);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_winlator_serial_SerialPty_close(JNIEnv *env, jclass clazz, jint fd) {
    if (fd >= 0) close(fd);
}
