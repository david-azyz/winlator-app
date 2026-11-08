/*
 * comtest.exe - serial port test for Winlator (and real Windows).
 *
 *   comtest            test COM1 (loopback test at 9600, 19200 and 38400)
 *   comtest COM3       test another port
 *   comtest COM1 monitor 9600
 *                      show everything that arrives on the port (Ctrl+C to quit)
 *
 * The loopback test sends data and expects to receive the same bytes back.
 * It passes with:
 *   - the "Loopback (test)" device of the Winlator COM port settings, or
 *   - a real USB-RS232 adapter with pins 2 (RX) and 3 (TX) of the DB9 shorted.
 *
 * It uses the same Win32 calls as GE wPTU (BuildCommDCB, SetCommState,
 * SetCommTimeouts, SetCommMask, PurgeComm, ClearCommError), including
 * changing the baud rate on an open port.
 *
 * Built without the CRT or Windows headers (see build.sh).
 */

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef void *HANDLE;
typedef const char *LPCSTR;

#define WINAPI __stdcall
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define OPEN_EXISTING 3
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_INPUT_HANDLE ((DWORD)-10)
#define PURGE_TXCLEAR 0x0004
#define PURGE_RXCLEAR 0x0008
#define EV_RXCHAR 0x0001

typedef struct {
    DWORD DCBlength;
    DWORD BaudRate;
    DWORD flags;
    WORD wReserved;
    WORD XonLim;
    WORD XoffLim;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    char XonChar;
    char XoffChar;
    char ErrorChar;
    char EofChar;
    char EvtChar;
    WORD wReserved1;
} DCB;

typedef struct {
    DWORD ReadIntervalTimeout;
    DWORD ReadTotalTimeoutMultiplier;
    DWORD ReadTotalTimeoutConstant;
    DWORD WriteTotalTimeoutMultiplier;
    DWORD WriteTotalTimeoutConstant;
} COMMTIMEOUTS;

typedef struct {
    DWORD Flags;
    DWORD cbInQue;
    DWORD cbOutQue;
} COMSTAT;

__declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
__declspec(dllimport) BOOL WINAPI GetCommState(HANDLE, DCB *);
__declspec(dllimport) BOOL WINAPI SetCommState(HANDLE, DCB *);
__declspec(dllimport) BOOL WINAPI BuildCommDCBA(LPCSTR, DCB *);
__declspec(dllimport) BOOL WINAPI SetCommTimeouts(HANDLE, COMMTIMEOUTS *);
__declspec(dllimport) BOOL WINAPI SetCommMask(HANDLE, DWORD);
__declspec(dllimport) BOOL WINAPI SetupComm(HANDLE, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI PurgeComm(HANDLE, DWORD);
__declspec(dllimport) BOOL WINAPI ClearCommError(HANDLE, DWORD *, COMSTAT *);
__declspec(dllimport) HANDLE WINAPI GetStdHandle(DWORD);
__declspec(dllimport) DWORD WINAPI GetLastError(void);
__declspec(dllimport) DWORD WINAPI GetTickCount(void);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) void WINAPI ExitProcess(unsigned int);
__declspec(dllimport) LPCSTR WINAPI GetCommandLineA(void);

static HANDLE out;

/* Keeps the window open when the program was started with a double click. */
static void finish(unsigned int code) {
    char buf[16];
    DWORD n;
    print("\r\nPressione Enter para fechar...");
    ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, sizeof(buf), &n, 0);
    ExitProcess(code);
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    DWORD written;
    WriteFile(out, s, str_len(s), &written, 0);
}

static void print_num(unsigned long v) {
    char buf[16];
    int i = 15;
    buf[i] = 0;
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i > 0);
    print(buf + i);
}

static void print_hex_byte(BYTE b) {
    const char *digits = "0123456789ABCDEF";
    char buf[4] = {digits[b >> 4], digits[b & 15], ' ', 0};
    print(buf);
}

static int str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
        char cb = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static unsigned long parse_num(const char *s) {
    unsigned long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0');
    return v;
}

/* Splits the command line into up to max arguments (argv[0] is skipped). */
static int parse_args(char *cmd, char **args, int max) {
    int count = 0;
    char *p = cmd;

    /* skip program name */
    if (*p == '"') {
        p++;
        while (*p && *p != '"') p++;
        if (*p) p++;
    }
    else while (*p && *p != ' ' && *p != '\t') p++;

    while (*p && count < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        args[count++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    return count;
}

static void copy_str(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void append_str(char *dst, const char *src, int max) {
    int n = str_len(dst);
    copy_str(dst + n, src, max - n);
}

static void append_num(char *dst, unsigned long v, int max) {
    char buf[16];
    int i = 15;
    buf[i] = 0;
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i > 0);
    append_str(dst, buf + i, max);
}

static void print_error(const char *what) {
    print("  ERRO: ");
    print(what);
    print(" (codigo ");
    print_num(GetLastError());
    print(")\r\n");
}

static BOOL configure(HANDLE port, unsigned long baud) {
    char mode[64];
    DCB dcb;

    mode[0] = 0;
    append_str(mode, "baud=", sizeof(mode));
    append_num(mode, baud, sizeof(mode));
    append_str(mode, " parity=N data=8 stop=1", sizeof(mode));

    for (int i = 0; i < (int)sizeof(dcb); i++) ((BYTE *)&dcb)[i] = 0;
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(port, &dcb)) {
        print_error("GetCommState falhou");
        return 0;
    }
    if (!BuildCommDCBA(mode, &dcb)) {
        print_error("BuildCommDCB falhou");
        return 0;
    }
    if (!SetCommState(port, &dcb)) {
        print_error("SetCommState falhou");
        return 0;
    }

    COMMTIMEOUTS timeouts = {50, 0, 300, 0, 1000};
    if (!SetCommTimeouts(port, &timeouts)) {
        print_error("SetCommTimeouts falhou");
        return 0;
    }
    return 1;
}

static BOOL loopback_test(HANDLE port, unsigned long baud) {
    static const char pattern[] = "WINLATOR-COM-TEST 0123456789 abcdefghijklmnopqrstuvwxyz\r\n";
    BYTE received[256];
    DWORD total = 0, n, errors;
    COMSTAT stat;
    int len = str_len(pattern);

    print("Teste a ");
    print_num(baud);
    print(" baud... ");

    if (!configure(port, baud)) return 0;

    /* Let the bridge pick up the new baud rate. */
    Sleep(150);
    PurgeComm(port, PURGE_RXCLEAR | PURGE_TXCLEAR);
    ClearCommError(port, &errors, &stat);

    if (!WriteFile(port, pattern, len, &n, 0) || n != (DWORD)len) {
        print_error("WriteFile falhou");
        return 0;
    }

    DWORD start = GetTickCount();
    while (total < (DWORD)len && GetTickCount() - start < 3000) {
        if (!ReadFile(port, received + total, sizeof(received) - total, &n, 0)) {
            print_error("ReadFile falhou");
            return 0;
        }
        total += n;
    }

    if (total == 0) {
        print("FALHOU: nada voltou.\r\n");
        return 0;
    }

    int match = total == (DWORD)len;
    for (DWORD i = 0; match && i < total; i++) if (received[i] != (BYTE)pattern[i]) match = 0;

    if (match) {
        print("OK (");
        print_num(total);
        print(" bytes enviados e recebidos)\r\n");
        return 1;
    }

    print("FALHOU: recebidos ");
    print_num(total);
    print(" de ");
    print_num(len);
    print(" bytes, diferentes do enviado.\r\n  Primeiros bytes: ");
    for (DWORD i = 0; i < total && i < 16; i++) print_hex_byte(received[i]);
    print("\r\n");
    return 0;
}

static void monitor(HANDLE port, unsigned long baud) {
    BYTE buf[256];
    DWORD n;

    if (!configure(port, baud)) return;
    print("Monitorando a ");
    print_num(baud);
    print(" baud (Ctrl+C para sair). Bytes em hexadecimal:\r\n");

    for (;;) {
        if (!ReadFile(port, buf, sizeof(buf), &n, 0)) {
            print_error("ReadFile falhou");
            return;
        }
        for (DWORD i = 0; i < n; i++) print_hex_byte(buf[i]);
        if (n) print("\r\n");
    }
}

void mainCRTStartup(void) {
    static char cmd[512];
    char *args[4];
    char portName[32], path[48];
    int failures = 0;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    copy_str(cmd, GetCommandLineA(), sizeof(cmd));
    int argc = parse_args(cmd, args, 4);

    copy_str(portName, argc >= 1 ? args[0] : "COM1", sizeof(portName));
    int monitorMode = argc >= 2 && str_ieq(args[1], "monitor");
    unsigned long monitorBaud = argc >= 3 ? parse_num(args[2]) : 9600;
    if (monitorBaud == 0) monitorBaud = 9600;

    print("comtest - teste de porta serial para Winlator\r\n");
    print("Abrindo ");
    print(portName);
    print("...\r\n");

    path[0] = 0;
    append_str(path, "\\\\.\\", sizeof(path));
    append_str(path, portName, sizeof(path));

    HANDLE port = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (port == INVALID_HANDLE_VALUE) {
        print_error("nao foi possivel abrir a porta");
        print("A porta nao existe para o Windows/Wine. Confira a configuracao\r\n");
        print("das Portas COM do container e reinicie o container.\r\n");
        finish(2);
    }
    print("Porta aberta.\r\n");

    SetupComm(port, 4096, 4096);
    SetCommMask(port, EV_RXCHAR);

    if (monitorMode) {
        monitor(port, monitorBaud);
        CloseHandle(port);
        finish(0);
    }

    /* Same speeds wPTU uses, switching on the open port. */
    static const unsigned long speeds[] = {9600, 19200, 38400};
    for (int i = 0; i < 3; i++) if (!loopback_test(port, speeds[i])) failures++;

    CloseHandle(port);

    if (failures == 0) {
        print("\r\nRESULTADO: TUDO OK. A porta envia e recebe dados nas 3 velocidades.\r\n");
    }
    else {
        print("\r\nRESULTADO: ");
        print_num((unsigned long)failures);
        print(" teste(s) falharam.\r\n");
        print("Com adaptador real, os pinos 2 e 3 do DB9 precisam estar ligados entre si.\r\n");
    }
    finish(failures == 0 ? 0 : 1);
}
