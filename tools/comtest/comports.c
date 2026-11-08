/*
 * comports.exe - "Gerenciador de Portas COM" for Winlator.
 *
 * Device-Manager-like window listing the COM ports Winlator bridges to USB
 * serial adapters: adapter, state, current baud rate/format and bytes
 * sent/received, refreshed every second.
 *
 * Data comes from C:\windows\winlator-comports.ini, written by Winlator's
 * SerialPortBridgeComponent while the container runs.
 *
 * Built without the CRT or Windows headers (see build.sh).
 */

typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef void *HANDLE;
typedef HANDLE HWND, HINSTANCE, HICON, HCURSOR, HBRUSH, HMENU, HFONT, HGDIOBJ;
typedef UINT WPARAM;
typedef long LPARAM;
typedef long LRESULT;
typedef const char *LPCSTR;
typedef char *LPSTR;

#define WINAPI __stdcall
#define CALLBACK __stdcall

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct { UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; HICON hIconSm; } WNDCLASSEXA;
typedef struct { long x, y; } POINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef struct { long left, top, right, bottom; } RECT;
typedef struct { DWORD dwSize, dwICC; } INITCOMMONCONTROLSEX;
typedef struct { HWND hwndFrom; UINT idFrom; UINT code; } NMHDR;
typedef struct { UINT mask; int fmt; int cx; LPSTR pszText; int cchTextMax; int iSubItem; int iImage; int iOrder; } LVCOLUMNA;
typedef struct { UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask; LPSTR pszText; int cchTextMax; int iImage; LPARAM lParam; int iIndent; } LVITEMA;

__declspec(dllimport) HINSTANCE WINAPI GetModuleHandleA(LPCSTR);
__declspec(dllimport) DWORD WINAPI GetPrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
__declspec(dllimport) UINT WINAPI GetPrivateProfileIntA(LPCSTR, LPCSTR, int, LPCSTR);
__declspec(dllimport) UINT WINAPI GetWindowsDirectoryA(LPSTR, UINT);
__declspec(dllimport) UINT WINAPI WinExec(LPCSTR, UINT);
__declspec(dllimport) void WINAPI ExitProcess(UINT);

__declspec(dllimport) unsigned short WINAPI RegisterClassExA(const WNDCLASSEXA *);
__declspec(dllimport) HWND WINAPI CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void *);
__declspec(dllimport) LRESULT WINAPI DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL WINAPI GetMessageA(MSG *, HWND, UINT, UINT);
__declspec(dllimport) BOOL WINAPI TranslateMessage(const MSG *);
__declspec(dllimport) LRESULT WINAPI DispatchMessageA(const MSG *);
__declspec(dllimport) void WINAPI PostQuitMessage(int);
__declspec(dllimport) LRESULT WINAPI SendMessageA(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) UINT WINAPI SetTimer(HWND, UINT, UINT, void *);
__declspec(dllimport) BOOL WINAPI MoveWindow(HWND, int, int, int, int, BOOL);
__declspec(dllimport) HCURSOR WINAPI LoadCursorA(HINSTANCE, LPCSTR);
__declspec(dllimport) HICON WINAPI LoadIconA(HINSTANCE, LPCSTR);
__declspec(dllimport) int WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
__declspec(dllimport) BOOL WINAPI ShowWindow(HWND, int);
__declspec(dllimport) BOOL WINAPI UpdateWindow(HWND);
__declspec(dllimport) BOOL WINAPI GetClientRect(HWND, RECT *);
__declspec(dllimport) BOOL WINAPI SetWindowTextA(HWND, LPCSTR);
__declspec(dllimport) HGDIOBJ WINAPI GetStockObject(int);
__declspec(dllimport) BOOL WINAPI InitCommonControlsEx(const INITCOMMONCONTROLSEX *);

#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_VISIBLE 0x10000000
#define WS_CHILD 0x40000000
#define WS_TABSTOP 0x00010000
#define WS_EX_CLIENTEDGE 0x00000200
#define LVS_REPORT 0x0001
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVS_EX_GRIDLINES 0x00000001
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVM_GETITEMCOUNT 0x1004
#define LVM_INSERTITEMA 0x1007
#define LVM_DELETEALLITEMS 0x1009
#define LVM_GETNEXTITEM 0x100C
#define LVM_INSERTCOLUMNA 0x101B
#define LVM_SETITEMTEXTA 0x102E
#define LVM_SETEXTENDEDLISTVIEWSTYLE 0x1036
#define LVCF_FMT 1
#define LVCF_WIDTH 2
#define LVCF_TEXT 4
#define LVCF_SUBITEM 8
#define LVIF_TEXT 1
#define LVNI_SELECTED 2
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_SETFONT 0x0030
#define WM_NOTIFY 0x004E
#define WM_COMMAND 0x0111
#define WM_TIMER 0x0113
#define NM_DBLCLK ((UINT)-3)
#define BN_CLICKED 0
#define MB_OK 0
#define MB_ICONINFORMATION 0x40
#define MB_ICONWARNING 0x30
#define SW_SHOWNORMAL 1
#define DEFAULT_GUI_FONT 17
#define COLOR_BTNFACE 15
#define CW_USEDEFAULT ((int)0x80000000)

#define ID_LIST 100
#define ID_PROPERTIES 101
#define ID_TEST 102
#define ID_REFRESH 103
#define TIMER_REFRESH 1

#define MAX_PORTS 16
#define COLUMNS 7
#define FIELD_LEN 96

static HWND mainWindow, header, list, statusText, btnProperties, btnTest, btnRefresh;
static char iniPath[300];
static char cache[MAX_PORTS][COLUMNS][FIELD_LEN];
static char deviceIds[MAX_PORTS][FIELD_LEN];
static int rowCount = -1;

static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void str_cat(char *dst, const char *src, int max) {
    int n = str_len(dst);
    str_copy(dst + n, src, max - n);
}

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void num_to_str(unsigned long v, char *out) {
    char tmp[16];
    int i = 0, n = 0;
    do { tmp[i++] = (char)('0' + v % 10); v /= 10; } while (v && i < 15);
    while (i) out[n++] = tmp[--i];
    out[n] = 0;
}

/* "1234567" -> "1.234.567" */
static void group_thousands(const char *digits, char *out) {
    int len = str_len(digits), n = 0;
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[n++] = '.';
        out[n++] = digits[i];
    }
    out[n] = 0;
}

static void read_field(int port, const char *key, char *out, int max) {
    char section[16] = "Port";
    char num[8];
    num_to_str((unsigned long)port, num);
    str_cat(section, num, sizeof(section));
    GetPrivateProfileStringA(section, key, "", out, (DWORD)max, iniPath);
}

static void add_column(int index, const char *title, int width) {
    LVCOLUMNA col;
    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    col.fmt = 0;
    col.cx = width;
    col.pszText = (LPSTR)title;
    col.cchTextMax = 0;
    col.iSubItem = index;
    col.iImage = 0;
    col.iOrder = 0;
    SendMessageA(list, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&col);
}

static void set_cell(int row, int column, const char *text) {
    if (str_eq(cache[row][column], text)) return;
    str_copy(cache[row][column], text, FIELD_LEN);

    LVITEMA item;
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = column;
    item.state = item.stateMask = 0;
    item.pszText = cache[row][column];
    item.cchTextMax = 0;
    item.iImage = 0;
    item.lParam = 0;
    item.iIndent = 0;
    SendMessageA(list, LVM_SETITEMTEXTA, (WPARAM)row, (LPARAM)&item);
}

static void insert_row(int row) {
    LVITEMA item;
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = 0;
    item.state = item.stateMask = 0;
    item.pszText = (LPSTR)"";
    item.cchTextMax = 0;
    item.iImage = 0;
    item.lParam = 0;
    item.iIndent = 0;
    SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);
}

static void refresh(void) {
    int count = (int)GetPrivateProfileIntA("General", "Count", 0, iniPath);
    if (count < 0) count = 0;
    if (count > MAX_PORTS) count = MAX_PORTS;

    if (count != rowCount) {
        SendMessageA(list, LVM_DELETEALLITEMS, 0, 0);
        for (int i = 0; i < MAX_PORTS; i++) for (int c = 0; c < COLUMNS; c++) cache[i][c][0] = 0;
        for (int i = 0; i < count; i++) insert_row(i);
        rowCount = count;
    }

    static const char *keys[COLUMNS] = {"Name", "Device", "State", "Baud", "Format", "Sent", "Received"};
    char value[FIELD_LEN], grouped[FIELD_LEN];
    for (int i = 0; i < count; i++) {
        for (int c = 0; c < COLUMNS; c++) {
            read_field(i + 1, keys[c], value, sizeof(value));
            if (c >= 5) {
                group_thousands(value, grouped);
                set_cell(i, c, grouped);
            }
            else set_cell(i, c, value);
        }
        read_field(i + 1, "DeviceId", deviceIds[i], FIELD_LEN);
    }

    if (count == 0) {
        SetWindowTextA(statusText, "Nenhuma porta COM configurada. No Winlator: Editar container > Avancado > Portas COM.");
    }
    else {
        char status[128], num[8];
        num_to_str((unsigned long)count, num);
        str_copy(status, num, sizeof(status));
        str_cat(status, count == 1 ? " porta configurada - atualiza a cada 1 segundo" : " portas configuradas - atualiza a cada 1 segundo", sizeof(status));
        SetWindowTextA(statusText, status);
    }
}

static int selected_row(void) {
    int row = (int)SendMessageA(list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    return row >= 0 && row < rowCount ? row : -1;
}

static void show_properties(void) {
    int row = selected_row();
    if (row < 0) {
        MessageBoxA(mainWindow, "Selecione uma porta na lista.", "Propriedades", MB_OK | MB_ICONINFORMATION);
        return;
    }

    char text[1024], title[64];
    static const char *labels[COLUMNS] = {"Porta", "Dispositivo", "Estado", "Velocidade (baud)", "Formato", "Bytes enviados", "Bytes recebidos"};
    text[0] = 0;
    for (int c = 0; c < COLUMNS; c++) {
        str_cat(text, labels[c], sizeof(text));
        str_cat(text, ":  ", sizeof(text));
        str_cat(text, cache[row][c], sizeof(text));
        str_cat(text, "\r\n", sizeof(text));
    }
    if (deviceIds[row][0]) {
        str_cat(text, "ID USB (VID:PID:porta):  ", sizeof(text));
        str_cat(text, deviceIds[row], sizeof(text));
        str_cat(text, "\r\n", sizeof(text));
    }
    str_cat(text, "\r\nO formato (bits de dados e paridade) e definido nas configuracoes do container. A velocidade e os stop bits seguem o programa Windows.", sizeof(text));

    str_copy(title, "Propriedades - ", sizeof(title));
    str_cat(title, cache[row][0], sizeof(title));
    MessageBoxA(mainWindow, text, title, MB_OK | MB_ICONINFORMATION);
}

static void test_port(void) {
    int row = selected_row();
    if (row < 0) {
        MessageBoxA(mainWindow, "Selecione uma porta na lista.", "Testar porta", MB_OK | MB_ICONINFORMATION);
        return;
    }

    char cmd[400];
    GetWindowsDirectoryA(cmd, 260);
    str_cat(cmd, "\\comtest.exe ", sizeof(cmd));
    str_cat(cmd, cache[row][0], sizeof(cmd));
    if (WinExec(cmd, SW_SHOWNORMAL) <= 31) {
        MessageBoxA(mainWindow, "Nao foi possivel abrir o comtest.exe.", "Testar porta", MB_OK | MB_ICONWARNING);
    }
}

static void layout(void) {
    RECT rc;
    GetClientRect(mainWindow, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int buttonsY = h - 58;
    MoveWindow(header, 10, 8, w - 20, 18, 1);
    MoveWindow(list, 10, 28, w - 20, buttonsY - 36, 1);
    MoveWindow(btnRefresh, w - 100, buttonsY, 90, 26, 1);
    MoveWindow(btnTest, w - 200, buttonsY, 92, 26, 1);
    MoveWindow(btnProperties, w - 300, buttonsY, 92, 26, 1);
    MoveWindow(statusText, 10, h - 24, w - 20, 18, 1);
}

static HWND create_child(DWORD exStyle, LPCSTR cls, LPCSTR text, DWORD style, int id, HINSTANCE inst) {
    HWND hwnd = CreateWindowExA(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, mainWindow, (HMENU)(long)id, inst, 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 1);
    return hwnd;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE inst = GetModuleHandleA(0);
            mainWindow = hwnd;
            header = create_child(0, "STATIC", "Portas (COM e LPT)", 0, 0, inst);
            list = create_child(WS_EX_CLIENTEDGE, "SysListView32", "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, ID_LIST, inst);
            SendMessageA(list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            add_column(0, "Porta", 52);
            add_column(1, "Dispositivo", 200);
            add_column(2, "Estado", 110);
            add_column(3, "Velocidade", 72);
            add_column(4, "Formato", 58);
            add_column(5, "Enviados", 72);
            add_column(6, "Recebidos", 76);
            btnProperties = create_child(0, "BUTTON", "Propriedades", WS_TABSTOP, ID_PROPERTIES, inst);
            btnTest = create_child(0, "BUTTON", "Testar porta", WS_TABSTOP, ID_TEST, inst);
            btnRefresh = create_child(0, "BUTTON", "Atualizar", WS_TABSTOP, ID_REFRESH, inst);
            statusText = create_child(0, "STATIC", "", 0, 0, inst);
            layout();
            refresh();
            SetTimer(hwnd, TIMER_REFRESH, 1000, 0);
            return 0;
        }
        case WM_SIZE:
            if (list) layout();
            return 0;
        case WM_TIMER:
            refresh();
            return 0;
        case WM_COMMAND:
            if (((wParam >> 16) & 0xFFFF) == BN_CLICKED) {
                switch (wParam & 0xFFFF) {
                    case ID_PROPERTIES: show_properties(); break;
                    case ID_TEST: test_port(); break;
                    case ID_REFRESH: rowCount = -1; refresh(); break;
                }
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR *nm = (NMHDR *)lParam;
            if (nm->idFrom == ID_LIST && nm->code == NM_DBLCLK) show_properties();
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void WinMainCRTStartup(void) {
    HINSTANCE inst = GetModuleHandleA(0);
    INITCOMMONCONTROLSEX icc = {sizeof(INITCOMMONCONTROLSEX), 0x00000001 /* ICC_LISTVIEW_CLASSES */};
    InitCommonControlsEx(&icc);

    GetWindowsDirectoryA(iniPath, 260);
    str_cat(iniPath, "\\winlator-comports.ini", sizeof(iniPath));

    WNDCLASSEXA wc;
    BYTE *p = (BYTE *)&wc;
    for (unsigned i = 0; i < sizeof(wc); i++) p[i] = 0;
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(0, (LPCSTR)32512);
    wc.hCursor = LoadCursorA(0, (LPCSTR)32512);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WinlatorComPorts";
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, "WinlatorComPorts", "Gerenciador de Portas COM", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 680, 340, 0, 0, inst, 0);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    ExitProcess(0);
}
