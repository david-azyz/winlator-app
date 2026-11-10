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
#define SYS_fcntl 72
#define SYS_readlink 89
#define SYS_openat 257

#define AT_FDCWD (-100)
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 04000
#define O_CREAT 0100
#define O_TMPFILE 020200000

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
#define TCOFLUSH 1
#define TCIOFLUSH 2
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

static long sys4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "rcx", "r11", "memory");
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

static int env_loaded = 0;
static int env_force = 0;
static char env_prefix[512];

static void load_env(void) {
    if (env_loaded) return;
    env_loaded = 1;
    static char buf[32768];
    long fd = sys3(SYS_open, (long)"/proc/self/environ", 0, 0);
    if (fd < 0) return;
    long n = sys3(SYS_read, fd, (long)buf, sizeof(buf) - 1);
    sys3(SYS_close, fd, 0, 0);
    if (n <= 0) return;
    buf[n] = 0;
    for (long i = 0; i < n; i++) {
        if (i > 0 && buf[i - 1] != 0) continue;
        if (starts_with(buf + i, "WINLATOR_SERIALSHIM_FORCE=1")) env_force = 1;
        if (starts_with(buf + i, "WINEPREFIX=")) {
            const char *v = buf + i + 11;
            int k = 0;
            while (v[k] && k < (int)sizeof(env_prefix) - 1) { env_prefix[k] = v[k]; k++; }
            env_prefix[k] = 0;
        }
    }
}

static int force_emulation(void) {
    load_env();
    return env_force;
}

/* ---- diagnostics: C:\windows\winlator-serialshim.log, shown by the COM port manager ---- */
#define SYS_write 1
#define SYS_getpid 39
#define O_WRONLY 01
#define O_APPEND 02000

static int log_lines = 0;

static int s_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void s_cat(char *dst, int max, const char *src) {
    int n = s_len(dst);
    while (*src && n < max - 1) dst[n++] = *src++;
    dst[n] = 0;
}

static void s_num(char *dst, int max, unsigned long v, int base) {
    char tmp[24];
    int i = 0;
    const char *digits = "0123456789ABCDEF";
    do { tmp[i++] = digits[v % base]; v /= base; } while (v && i < 23);
    char out[24];
    int k = 0;
    while (i) out[k++] = tmp[--i];
    out[k] = 0;
    s_cat(dst, max, out);
}

static void shim_log(const char *msg) {
    load_env();
    if (!env_prefix[0] || log_lines >= 200) return;
    log_lines++;

    char path[600];
    path[0] = 0;
    s_cat(path, sizeof(path), env_prefix);
    s_cat(path, sizeof(path), "/drive_c/windows/winlator-serialshim.log");

    char comm[32];
    comm[0] = 0;
    long cfd = sys3(SYS_open, (long)"/proc/self/comm", 0, 0);
    if (cfd >= 0) {
        long n = sys3(SYS_read, cfd, (long)comm, sizeof(comm) - 1);
        sys3(SYS_close, cfd, 0, 0);
        if (n < 0) n = 0;
        comm[n] = 0;
        for (int i = 0; comm[i]; i++) if (comm[i] == '\n') comm[i] = 0;
    }

    char line[400];
    line[0] = 0;
    s_cat(line, sizeof(line), comm[0] ? comm : "?");
    s_cat(line, sizeof(line), "[");
    s_num(line, sizeof(line), (unsigned long)sys3(SYS_getpid, 0, 0, 0), 10);
    s_cat(line, sizeof(line), "] ");
    s_cat(line, sizeof(line), msg);
    s_cat(line, sizeof(line), "\n");

    long fd = sys3(SYS_open, (long)path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    sys3(SYS_write, fd, (long)line, s_len(line));
    sys3(SYS_close, fd, 0, 0);
}

/* Logs the first time each tty ioctl is seen on a COM pty in this process, with its result. */
static unsigned long seen_reqs[32];
static int seen_count = 0;
static int announced = 0;

static void log_ioctl_once(unsigned long req, long native, long final_result, int emulated) {
    for (int i = 0; i < seen_count; i++) if (seen_reqs[i] == req) return;
    if (seen_count < 32) seen_reqs[seen_count++] = req;

    char msg[160];
    msg[0] = 0;
    s_cat(msg, sizeof(msg), "ioctl 0x");
    s_num(msg, sizeof(msg), req, 16);
    if (native >= 0) s_cat(msg, sizeof(msg), " ok");
    else {
        s_cat(msg, sizeof(msg), " bloqueado (erro ");
        s_num(msg, sizeof(msg), (unsigned long)(-native), 10);
        s_cat(msg, sizeof(msg), ")");
        if (emulated) s_cat(msg, sizeof(msg), final_result >= 0 ? " -> emulado ok" : " -> emulacao falhou");
        else s_cat(msg, sizeof(msg), " -> sem emulacao");
    }
    shim_log(msg);
}

/* Discards the data waiting to be read on fd (bytes received while no program used the port). */
static long drain_input(int fd) {
    static char junk[4096];
    long total = 0;
    long flags = sys3(SYS_fcntl, fd, F_GETFL, 0);
    if (flags < 0) return 0;
    if (!(flags & O_NONBLOCK)) sys3(SYS_fcntl, fd, F_SETFL, flags | O_NONBLOCK);
    for (int i = 0; i < 64; i++) {
        long n = sys3(SYS_read, fd, (long)junk, sizeof(junk));
        if (n <= 0) break;
        total += n;
    }
    if (!(flags & O_NONBLOCK)) sys3(SYS_fcntl, fd, F_SETFL, flags);
    return total;
}

static void log_drain(const char *what, long n) {
    char msg[120];
    msg[0] = 0;
    s_cat(msg, sizeof(msg), what);
    s_cat(msg, sizeof(msg), ": ");
    s_num(msg, sizeof(msg), (unsigned long)n, 10);
    s_cat(msg, sizeof(msg), " bytes descartados");
    shim_log(msg);
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
        case TCFLSH: {
            /* PurgeComm: TCFLSH may be blocked; an input flush is done by reading out the queue. */
            int what = (int)(long)arg;
            if (what == TCIFLUSH || what == TCIOFLUSH) {
                long n = drain_input(fd);
                if (n > 0) log_drain("PurgeComm", n);
            }
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
    int tty_req = ((req >> 8) & 0xFF) == 0x54;
    if (force_emulation() && is_pty(fd)) {
        r = emulate(fd, req, arg);
        if (r == -ENOTTY) r = sys3(SYS_ioctl, fd, (long)req, (long)arg);
    }
    else {
        r = sys3(SYS_ioctl, fd, (long)req, (long)arg);
        int pty = (tty_req && !announced) || r < 0 ? is_pty(fd) : 0;
        if (pty && !announced) {
            announced = 1;
            shim_log("compatibilidade da porta COM ativa");
        }
        if (r < 0 && (r == -EACCES || r == -EPERM || r == -ENOTTY || r == -EINVAL) && pty) {
            long native = r;
            long e = emulate(fd, req, arg);
            if (e != -ENOTTY) r = e;
            log_ioctl_once(req, native, r, e != -ENOTTY);
        }
        else if (pty && r < 0) log_ioctl_once(req, r, r, 0);
    }

    if (r < 0) return fail(-r);
    return (int)r;
}

/*
 * open() hooks. On Windows a COM port that no program has open drops what it
 * receives, so a program always starts with an empty receive buffer. Here the
 * bridge keeps the pty open, so bytes received earlier (line noise when the
 * cable is plugged, data sent by the device, leftovers of a previous session)
 * would wait in the pty and confuse the program's first exchange. When a pty
 * is opened, its pending input is discarded.
 */
static int looks_like_com_path(const char *path) {
    if (!path) return 0;
    if (starts_with(path, "/dev/pts/")) return 1;
    const char *base = path;
    for (const char *p = path; *p; p++) if (*p == '/') base = p + 1;
    return (base[0] == 'c' || base[0] == 'C') && (base[1] == 'o' || base[1] == 'O') &&
           (base[2] == 'm' || base[2] == 'M') && base[3] >= '0' && base[3] <= '9';
}

static long do_openat(int dirfd, const char *path, int flags, long mode) {
    long fd = sys4(SYS_openat, dirfd, (long)path, flags, mode);
    if (fd >= 0 && looks_like_com_path(path) && is_pty((int)fd)) {
        long n = drain_input((int)fd);
        char msg[160];
        msg[0] = 0;
        s_cat(msg, sizeof(msg), "porta aberta: ");
        s_cat(msg, sizeof(msg), path);
        if (n > 0) {
            s_cat(msg, sizeof(msg), ", ");
            s_num(msg, sizeof(msg), (unsigned long)n, 10);
            s_cat(msg, sizeof(msg), " bytes antigos descartados");
        }
        shim_log(msg);
    }
    return fd;
}

static int open_result(long r) {
    if (r < 0) return fail(-r);
    return (int)r;
}

static long mode_arg(int flags, __builtin_va_list ap) {
    if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) return __builtin_va_arg(ap, int);
    return 0;
}

int open(const char *path, int flags, ...) {
    __builtin_va_list ap; __builtin_va_start(ap, flags); long mode = mode_arg(flags, ap); __builtin_va_end(ap);
    return open_result(do_openat(AT_FDCWD, path, flags, mode));
}

int open64(const char *path, int flags, ...) {
    __builtin_va_list ap; __builtin_va_start(ap, flags); long mode = mode_arg(flags, ap); __builtin_va_end(ap);
    return open_result(do_openat(AT_FDCWD, path, flags, mode));
}

int openat(int dirfd, const char *path, int flags, ...) {
    __builtin_va_list ap; __builtin_va_start(ap, flags); long mode = mode_arg(flags, ap); __builtin_va_end(ap);
    return open_result(do_openat(dirfd, path, flags, mode));
}

int openat64(int dirfd, const char *path, int flags, ...) {
    __builtin_va_list ap; __builtin_va_start(ap, flags); long mode = mode_arg(flags, ap); __builtin_va_end(ap);
    return open_result(do_openat(dirfd, path, flags, mode));
}

int __open_2(const char *path, int flags) {
    return open_result(do_openat(AT_FDCWD, path, flags, 0));
}

int __open64_2(const char *path, int flags) {
    return open_result(do_openat(AT_FDCWD, path, flags, 0));
}

/*
 * termios helpers called by Wine's serial code (PurgeComm -> tcflush,
 * FlushFileBuffers -> tcdrain, flow control -> tcflow). They are libc
 * functions, so they do not go through the ioctl() hook above.
 */
#define TCXONC 0x540A

static int tc_call(int fd, unsigned long req, long arg, const char *name) {
    long r = force_emulation() && is_pty(fd) ? -EACCES : sys3(SYS_ioctl, fd, (long)req, arg);
    if (r >= 0) return 0;
    if ((r == -EACCES || r == -EPERM || r == -ENOTTY || r == -EINVAL) && is_pty(fd)) {
        long e = emulate(fd, req, (void *)arg);
        if (e == -ENOTTY) e = 0; /* nothing to do on a pty bridge */
        log_ioctl_once(req, r, e, 1);
        (void)name;
        if (e >= 0) return 0;
        r = e;
    }
    return fail(-r);
}

int tcflush(int fd, int queue) {
    return tc_call(fd, TCFLSH, queue, "tcflush");
}

int tcdrain(int fd) {
    return tc_call(fd, TCSBRK, 1, "tcdrain");
}

int tcflow(int fd, int action) {
    return tc_call(fd, TCXONC, action, "tcflow");
}
