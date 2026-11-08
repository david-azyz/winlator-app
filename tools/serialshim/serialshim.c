/*
 * libwinlator_serialshim.so - x86_64 library preloaded into Wine through
 * BOX64_LD_PRELOAD when COM ports are configured.
 *
 * Wine talks to COM ports with tty ioctls. Inside Android, the COM port is
 * a pty (/dev/pts/N) and SELinux only lets apps use a few tty ioctls on it,
 * so calls like TCGETS2 (used by Wine to read the baud rate), TCSETS2,
 * TIOCMGET or TIOCINQ fail with EACCES and Windows programs get
 * ERROR_ACCESS_DENIED from GetCommState/SetCommState/ClearCommError.
 *
 * This library overrides ioctl(): every call is first sent to the kernel
 * unchanged; only when it fails with a permission/unsupported error on a pty
 * is it emulated with the ioctls Android allows (TCGETS/TCSETS/TCFLSH/poll).
 *
 * Freestanding: no libc headers, raw x86_64 syscalls (Box64 translates them).
 * Set WINLATOR_SERIALSHIM_FORCE=1 to always emulate (for testing on a PC).
 */

typedef unsigned long size_t;
typedef long ssize_t;

#define SYS_read 0
#define SYS_open 2
#define SYS_close 3
#define SYS_poll 7
#define SYS_ioctl 16
#define SYS_readlink 89

#define EPERM 1
#define EINVAL 22
#define ENOTTY 25
#define EACCES 13

/* tty ioctls (same values on x86_64 and arm64) */
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCSBRK 0x5409
#define TCFLSH 0x540B
#define TIOCOUTQ 0x5411
#define TIOCMGET 0x5415
#define TIOCMBIS 0x5416
#define TIOCMBIC 0x5417
#define TIOCMSET 0x5418
#define FIONREAD 0x541B
#define TIOCSBRK 0x5427
#define TIOCCBRK 0x5428
#define TCGETS2 0x802C542AUL
#define TCSETS2 0x402C542BUL
#define TCSETSW2 0x402C542CUL
#define TCSETSF2 0x402C542DUL

#define TIOCM_DTR 0x002
#define TIOCM_RTS 0x004
#define TIOCM_CTS 0x020
#define TIOCM_CAR 0x040
#define TIOCM_DSR 0x100

#define TCIFLUSH 0
#define POLLIN 1

#define CBAUD 0x100F
#define BOTHER 0x1000

struct k_termios {
    unsigned int c_iflag, c_oflag, c_cflag, c_lflag;
    unsigned char c_line;
    unsigned char c_cc[19];
};

struct k_termios2 {
    unsigned int c_iflag, c_oflag, c_cflag, c_lflag;
    unsigned char c_line;
    unsigned char c_cc[19];
    unsigned int c_ispeed, c_ospeed;
};

struct pollfd_s {
    int fd;
    short events, revents;
};

extern int *__errno_location(void);

static long sys3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return ret;
}

static int fail(long err) {
    *__errno_location() = (int)err;
    return -1;
}

static const struct { unsigned int code; unsigned int baud; } speeds[] = {
    {0000001, 50}, {0000002, 75}, {0000003, 110}, {0000004, 134}, {0000005, 150},
    {0000006, 200}, {0000007, 300}, {0000010, 600}, {0000011, 1200}, {0000012, 1800},
    {0000013, 2400}, {0000014, 4800}, {0000015, 9600}, {0000016, 19200}, {0000017, 38400},
    {0010001, 57600}, {0010002, 115200}, {0010003, 230400}, {0010004, 460800},
    {0010005, 500000}, {0010006, 576000}, {0010007, 921600}, {0010010, 1000000},
    {0010011, 1152000}, {0010012, 1500000}, {0010013, 2000000}, {0010014, 2500000},
    {0010015, 3000000}, {0010016, 3500000}, {0010017, 4000000}
};
#define SPEED_COUNT ((int)(sizeof(speeds) / sizeof(speeds[0])))

static unsigned int code_to_baud(unsigned int code) {
    for (int i = 0; i < SPEED_COUNT; i++) if (speeds[i].code == code) return speeds[i].baud;
    return 0;
}

/* Nearest standard speed: a pty only stores the Bxxx code. */
static unsigned int baud_to_code(unsigned int baud) {
    unsigned int best = 0000015, bestDiff = 0xFFFFFFFF;
    for (int i = 0; i < SPEED_COUNT; i++) {
        unsigned int b = speeds[i].baud;
        unsigned int diff = b > baud ? b - baud : baud - b;
        if (diff < bestDiff) {
            bestDiff = diff;
            best = speeds[i].code;
        }
    }
    return best;
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) if (*s++ != *prefix++) return 0;
    return 1;
}

static int is_pty(int fd) {
    char path[32] = "/proc/self/fd/";
    char target[64];
    int n = 14, v = fd, digits = 0;
    char tmp[12];

    if (fd < 0) return 0;
    do {
        tmp[digits++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && digits < 11);
    while (digits) path[n++] = tmp[--digits];
    path[n] = 0;

    long len = sys3(SYS_readlink, (long)path, (long)target, sizeof(target) - 1);
    if (len <= 0) return 0;
    target[len] = 0;
    return starts_with(target, "/dev/pts/");
}

static int force_emulation(void) {
    static int cached = -1;
    if (cached == -1) {
        static char buf[16384];
        const char *key = "WINLATOR_SERIALSHIM_FORCE=1";
        cached = 0;
        long fd = sys3(SYS_open, (long)"/proc/self/environ", 0, 0);
        if (fd >= 0) {
            long n = sys3(SYS_read, fd, (long)buf, sizeof(buf) - 1);
            sys3(SYS_close, fd, 0, 0);
            for (long i = 0; n > 0 && i < n; i++) {
                if ((i == 0 || buf[i - 1] == 0) && starts_with(buf + i, key)) cached = 1;
            }
        }
    }
    return cached;
}

static long emulate(int fd, unsigned long req, void *arg) {
    switch (req) {
        case TCGETS2: {
            struct k_termios t;
            struct k_termios2 *t2 = (struct k_termios2 *)arg;
            long r = sys3(SYS_ioctl, fd, TCGETS, (long)&t);
            if (r < 0) return r;
            t2->c_iflag = t.c_iflag;
            t2->c_oflag = t.c_oflag;
            t2->c_cflag = t.c_cflag;
            t2->c_lflag = t.c_lflag;
            t2->c_line = t.c_line;
            for (int i = 0; i < 19; i++) t2->c_cc[i] = t.c_cc[i];
            unsigned int baud = code_to_baud(t.c_cflag & CBAUD);
            t2->c_ispeed = t2->c_ospeed = baud ? baud : 9600;
            return 0;
        }
        case TCSETS2:
        case TCSETSW2:
        case TCSETSF2: {
            struct k_termios t;
            const struct k_termios2 *t2 = (const struct k_termios2 *)arg;
            t.c_iflag = t2->c_iflag;
            t.c_oflag = t2->c_oflag;
            t.c_cflag = t2->c_cflag;
            t.c_lflag = t2->c_lflag;
            t.c_line = t2->c_line;
            for (int i = 0; i < 19; i++) t.c_cc[i] = t2->c_cc[i];
            if ((t.c_cflag & CBAUD) == BOTHER) {
                t.c_cflag = (t.c_cflag & ~CBAUD) | baud_to_code(t2->c_ospeed);
            }
            if (req == TCSETSF2) sys3(SYS_ioctl, fd, TCFLSH, TCIFLUSH);
            return sys3(SYS_ioctl, fd, TCSETS, (long)&t);
        }
        case TCSETSW:
        case TCSETSF:
            if (req == TCSETSF) sys3(SYS_ioctl, fd, TCFLSH, TCIFLUSH);
            return sys3(SYS_ioctl, fd, TCSETS, (long)arg);
        case TIOCMGET:
            /* The USB bridge keeps DTR/RTS asserted; report the lines as active. */
            *(int *)arg = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
            return 0;
        case TIOCMSET:
        case TIOCMBIS:
        case TIOCMBIC:
        case TIOCSBRK:
        case TIOCCBRK:
        case TCSBRK:
            return 0;
        case FIONREAD: {
            /* Exact count unavailable: report whether at least one byte is waiting. */
            struct pollfd_s p = {fd, POLLIN, 0};
            long r = sys3(SYS_poll, (long)&p, 1, 0);
            *(int *)arg = (r > 0 && (p.revents & POLLIN)) ? 1 : 0;
            return 0;
        }
        case TIOCOUTQ:
            *(int *)arg = 0;
            return 0;
        default:
            return -ENOTTY;
    }
}

int ioctl(int fd, unsigned long req, ...) {
    void *arg;
    __builtin_va_list ap;
    __builtin_va_start(ap, req);
    arg = __builtin_va_arg(ap, void *);
    __builtin_va_end(ap);

    req &= 0xFFFFFFFFUL;

    long r;
    if (force_emulation() && is_pty(fd)) {
        r = emulate(fd, req, arg);
        if (r == -ENOTTY) r = sys3(SYS_ioctl, fd, (long)req, (long)arg);
    }
    else {
        r = sys3(SYS_ioctl, fd, (long)req, (long)arg);
        if (r < 0 && (r == -EACCES || r == -EPERM || r == -ENOTTY || r == -EINVAL) && is_pty(fd)) {
            long e = emulate(fd, req, arg);
            if (e != -ENOTTY) r = e;
        }
    }

    if (r < 0) return fail(-r);
    return (int)r;
}
