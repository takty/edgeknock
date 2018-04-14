// edgeknock.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "edgeknock.h"


#define MAX_LINE 1024  // Max length of path with option string
#define MAX_SEP 5      // Max separation count
#define MAX_HOTKEY 16  // Max hotkey count
enum { KNOCK = 1, NONE = 0, READY = -2 };
enum { LEFT, RIGHT, TOP, BOTTOM };


int LastArea, JustOut;
unsigned int LastTime, OutTime;
int LastKnockArea, KnockCount;
unsigned int LastKnockTime;
int LastCorner = -1;

wchar_t IniPath[MAX_LINE];
unsigned int LimitTime = 300;
int EdgeWidth = 1, NoEffectWidth = 16;
int EdgeWidths[4] = { 1, 1, 1, 1 }, NoEffectWidths[4] = { 16, 16, 16, 16 };
int Left = 3, Right = 3, Top = 3, Bottom = 3;
BOOL FullScreenCheck = TRUE;
int CornerSize = 32;

wchar_t Message[MAX_LINE];
wchar_t Cmd[MAX_LINE], Opt[MAX_LINE];
int MessageCount = 0;
HANDLE ThreadHandle;
CRITICAL_SECTION Cs;



#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void WmCreate(HWND hWnd);
void WmPaint(HWND hWnd);
void WmMouseMoveHook(HWND hWnd, int x, int y);
void WmHotKey(HWND hWnd, int id);
void ShowMessage(HWND hWnd, const wchar_t *msg, int edge);
void LoadIniFile(HWND hWnd);
void SetHotkey(HWND hWnd, const wchar_t *key, int id);
DWORD WINAPI CheckIniFileThreadProc(LPVOID d);
int GetCorner(int x, int y);
int DetectKnock(int x, int y, int *a);
int GetArea(int x, int y);
wchar_t* LoadPath(wchar_t *path, int area, int corner);
void Exec(HWND hWnd, LPCTSTR path, int edge);
DWORD WINAPI OpenFileThreadProc(LPVOID d);
bool IsForegroundWindowFullScreen(void);
void ForegroundWindow(HWND hWnd);
wchar_t* AbsolutePath(wchar_t *path);
int Path_findLastOf(const wchar_t *str, wchar_t c);
wchar_t* Path_name(wchar_t *ret, const wchar_t *path);
wchar_t* Path_parent(wchar_t *ret, const wchar_t *path);
wchar_t* Path_parentDest(wchar_t *path);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_EDGEKNOCK, szWindowClass, MAX_LOADSTRING);
	
	// When secondary instance is executed, close the first
	CreateMutex(NULL, FALSE, szWindowClass);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND handle = FindWindow(szWindowClass, NULL);
		if (handle) SendMessage(handle, WM_CLOSE, 0, 0);
		return FALSE;
	}

	MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

	//HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_EDGEKNOCK));

	InitializeCriticalSection(&Cs);
	
	MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        //if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        //{
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        //}
    }

	DeleteCriticalSection(&Cs);

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
	wcex.hIcon          = NULL;  // LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDGEKNOCK));
    wcex.hCursor        = NULL;  // LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_INFOBK + 1);
    wcex.lpszMenuName   = NULL;  // MAKEINTRESOURCEW(IDC_EDGEKNOCK);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;  // LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, szWindowClass, L"", WS_BORDER | WS_POPUP,
      0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
		WmCreate(hWnd); 
		break;
	case WM_PAINT:
		WmPaint(hWnd);
		break;
	case WM_MOUSEMOVEHOOK: 
		WmMouseMoveHook(hWnd, wParam, lParam); break;
	case WM_TIMER:         
		if (MessageCount > 0 && --MessageCount == 0) ShowWindow(hWnd, SW_HIDE);
		break;
	case WM_USER:
		ShowMessage(hWnd, L"Edgeknock - The setting is updated.", 0);
		LoadIniFile(hWnd);
		break;
	case WM_HOTKEY:
		WmHotKey(hWnd, wParam); break;
	case WM_CLOSE:
		EndHook();  // mh.dll
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


void WmCreate(HWND hWnd) {
	ShowMessage(hWnd, L"Edgeknock", 0);
	LoadIniFile(hWnd);
	DWORD thread_id;
	CreateThread(NULL, 0, CheckIniFileThreadProc, &hWnd, 0, &thread_id);
	SetTimer(hWnd, 1, 100, NULL);
	SetOwnerWindow(hWnd);  // mh.dll
	SetHook();  // mh.dll
}


void WmPaint(HWND hWnd) {
	PAINTSTRUCT paint;
	HDC hdc = BeginPaint(hWnd, &paint);

	RECT r;
	GetClientRect(hWnd, &r);
	SelectObject(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
	DrawText(hdc, Message, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

	EndPaint(hWnd, &paint);
}


void WmMouseMoveHook(HWND hWnd, int x, int y) {
	wchar_t path[MAX_LINE];

	// Corner action
	int corner = GetCorner(x, y);
	if (corner != -1) {
		if (corner != LastCorner) {
			LoadPath(path, -1, corner);
			Exec(hWnd, path, -(corner + 1));
			LastCorner = corner;
		}
		return;
	}
	LastCorner = -1;

	// Knock action
	int area;
	int r = DetectKnock(x, y, &area);
	if (r == KNOCK) {
		LoadPath(path, area, -1);
		Exec(hWnd, path, area / MAX_SEP + 1);
	}
	else if (r == READY) {
		if (IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_HIDE);
	}
}


void WmHotKey(HWND hWnd, int id) {
	if (MAX_HOTKEY <= id) return;
	wchar_t path[MAX_LINE], key[8];
	wsprintf(key, L"Path%d", id + 1);
	GetPrivateProfileString(L"HotKey", key, L"", path, MAX_LINE, IniPath);
	Exec(hWnd, path, 0);
}


void ShowMessage(HWND hWnd, const wchar_t *msg, int edge) {
	int len = wcslen(msg);
	if (len == 0) return;

	POINT p;
	GetPhysicalCursorPos(&p);

	wcscpy_s(Message, MAX_LINE, msg);
	HDC hdc = GetDC(hWnd);
	SelectObject(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));  // フォント設定
	SIZE font;
	GetTextExtentPoint32(hdc, msg, len, &font);

	int w = font.cx + 10, h = font.cy + 10;
	int dw = GetSystemMetrics(SM_CXSCREEN), dh = GetSystemMetrics(SM_CYSCREEN);
	int mw = GetSystemMetrics(SM_CXCURSOR), mh = GetSystemMetrics(SM_CYCURSOR);

	int l = 0, t = 0;
	switch (edge) {
	case 1:  l = 0;      t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
	case 2:  l = dw - w; t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
	case 3:  l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = 0;      break;
	case 4:  l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = dh - h; break;
	case -1: l = mw;          t = mh;          break;
	case -2: l = dw - w - mw; t = mh;          break;
	case -3: l = dw - w - mw; t = dh - h - mh; break;
	case -4: l = mw;          t = dh - h - mh; break;
	default: l = (dw - w) / 2; t = (dh - h) / 2;
	}
	MoveWindow(hWnd, l, t, w, h, FALSE);
	MessageCount = 10;
	ShowWindow(hWnd, SW_SHOW);
}


void LoadIniFile(HWND hWnd) {
	GetModuleFileName(NULL, IniPath, MAX_PATH);
	IniPath[wcslen(IniPath) - 3] = L'\0';
	wcscat_s(IniPath, MAX_PATH, L"ini");

	LimitTime = GetPrivateProfileInt(L"Setting", L"LimitTime", LimitTime, IniPath);
	EdgeWidth = GetPrivateProfileInt(L"Setting", L"EdgeWidth", EdgeWidth, IniPath);
	NoEffectWidth = GetPrivateProfileInt(L"Setting", L"NoEffectWidth", NoEffectWidth, IniPath);
	FullScreenCheck = GetPrivateProfileInt(L"Setting", L"FullScreenCheck", FullScreenCheck, IniPath);
	CornerSize = GetPrivateProfileInt(L"Setting", L"CornerSize", CornerSize, IniPath);
	Left = GetPrivateProfileInt(L"Setting", L"Left", Left, IniPath);
	Right = GetPrivateProfileInt(L"Setting", L"Right", Right, IniPath);
	Top = GetPrivateProfileInt(L"Setting", L"Top", Top, IniPath);
	Bottom = GetPrivateProfileInt(L"Setting", L"Bottom", Bottom, IniPath);

	EdgeWidths[LEFT] = GetPrivateProfileInt(L"Setting", L"LeftEdgeWidth", EdgeWidth, IniPath);
	EdgeWidths[RIGHT] = GetPrivateProfileInt(L"Setting", L"RightEdgeWidth", EdgeWidth, IniPath);
	EdgeWidths[TOP] = GetPrivateProfileInt(L"Setting", L"TopEdgeWidth", EdgeWidth, IniPath);
	EdgeWidths[BOTTOM] = GetPrivateProfileInt(L"Setting", L"BottomEdgeWidth", EdgeWidth, IniPath);
	NoEffectWidths[LEFT] = GetPrivateProfileInt(L"Setting", L"LeftNoEffectWidth", NoEffectWidth, IniPath);
	NoEffectWidths[RIGHT] = GetPrivateProfileInt(L"Setting", L"RightNoEffectWidth", NoEffectWidth, IniPath);
	NoEffectWidths[TOP] = GetPrivateProfileInt(L"Setting", L"TopNoEffectWidth", NoEffectWidth, IniPath);
	NoEffectWidths[BOTTOM] = GetPrivateProfileInt(L"Setting", L"BottomNoEffectWidth", NoEffectWidth, IniPath);

	for (int i = 0; i < MAX_HOTKEY; ++i) {
		wchar_t key[8], hotkey[6];
		UnregisterHotKey(hWnd, i);
		wsprintf(key, L"Key%d", i + 1);
		GetPrivateProfileString(L"HotKey", key, L"", hotkey, 6, IniPath);
		SetHotkey(hWnd, hotkey, i);
	}
}


void SetHotkey(HWND hWnd, const wchar_t *key, int id) {
	if (wcslen(key) < 5) return;
	UINT flag = 0;
	if (key[0] == L'1') flag |= MOD_ALT;
	if (key[1] == L'1') flag |= MOD_CONTROL;
	if (key[2] == L'1') flag |= MOD_SHIFT;
	if (key[3] == L'1') flag |= MOD_WIN;
	if (flag) RegisterHotKey(hWnd, id, flag, key[4]);
}


DWORD WINAPI CheckIniFileThreadProc(LPVOID d) {
	HWND hWnd = *(HWND*)d;
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH - 1);
	Path_parentDest(path);

	// Observe ini file writing
	HANDLE handle = FindFirstChangeNotification(path, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (handle == INVALID_HANDLE_VALUE) return (DWORD)-1;

	// Wait until changing
	while (true) {
		if (WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0) {
			SendMessage(hWnd, WM_USER, 0, 0);  // Notify changing
		}
		else {
			break;
		}
		// Wait again
		if (!FindNextChangeNotification(handle)) break;
	}
	FindCloseChangeNotification(handle);
	return 0;
}


int GetCorner(int x, int y) {
	int mx = GetSystemMetrics(SM_CXSCREEN) - 1, my = GetSystemMetrics(SM_CYSCREEN) - 1;
	if (x == 0 && (0 <= y && y < 0 + CornerSize))   return 0;
	if ((0 <= x && x < 0 + CornerSize) && y == 0)   return 0;

	if (x == mx && (0 <= y && y < 0 + CornerSize))   return 1;
	if ((mx - CornerSize < x && x <= mx) && y == 0)   return 1;

	if (x == mx && (my - CornerSize < y && y <= my)) return 2;
	if ((mx - CornerSize < x && x <= mx) && y == my) return 2;

	if (x == 0 && (my - CornerSize < y && y <= my)) return 3;
	if ((0 <= x && x < 0 + CornerSize) && y == my) return 3;
	return -1;
}


int DetectKnock(int x, int y, int *a) {
	unsigned int time = timeGetTime();

	int area = GetArea(x, y);
	if (LastArea == -2 && area > -2) {  // When exiting from Normal
		OutTime = LastTime;
		JustOut = TRUE;
	}
	bool knock = false;
	if (JustOut && LastArea < 0 && area >= 0) {  // When exiting from Normal and entering to Edge
		if (time - OutTime <= LimitTime / 2) knock = true;
		JustOut = FALSE;
	}
	LastArea = area;
	LastTime = time;

	if (knock) {
		if (KnockCount > 0 && time - LastKnockTime > LimitTime) KnockCount = 0;
		if (KnockCount == 0 || LastKnockArea == area) KnockCount++;
		LastKnockTime = time;
		LastKnockArea = area;

		if (KnockCount == 1) return READY;
		if (KnockCount == 2) {
			KnockCount = 0;
			*a = area;
			return KNOCK;
		}
	}
	return NONE;
}


int GetArea(int x, int y) {
	int mx = GetSystemMetrics(SM_CXSCREEN) - 1, my = GetSystemMetrics(SM_CYSCREEN) - 1;
	if ((x == 0 || x == mx) && (y == 0 || y == my)) return -1;

	if (x >= 0 && x <= EdgeWidths[LEFT])        return y * Left / my;
	if (x <= mx && x >= mx - EdgeWidths[RIGHT])  return y * Right / my + MAX_SEP;
	if (y >= 0 && y <= EdgeWidths[TOP])         return x * Top / mx + MAX_SEP * 2;
	if (y <= my && y >= my - EdgeWidths[BOTTOM]) return x * Bottom / mx + MAX_SEP * 3;
	if (
		EdgeWidths[LEFT] + NoEffectWidths[LEFT] < x &&
		x < mx - (EdgeWidths[RIGHT] + NoEffectWidths[RIGHT]) &&
		EdgeWidths[TOP] + NoEffectWidths[TOP] < y &&
		y < my - (EdgeWidths[BOTTOM] + NoEffectWidths[BOTTOM])
		) return -2;
	return -1;
}


wchar_t* LoadPath(wchar_t *path, int area, int corner) {
	wchar_t key[8];
	wchar_t sec[][7] = { L"Left", L"Right", L"Top", L"Bottom" };

	if (corner != -1) {
		wsprintf(key, L"Path%d", corner + 1);
		GetPrivateProfileString(L"Corner", key, L"", path, MAX_LINE, IniPath);
	}
	else {
		wsprintf(key, L"Path%d", area % MAX_SEP + 1);
		GetPrivateProfileString(sec[area / MAX_SEP], key, L"", path, MAX_LINE, IniPath);
	}
	return path;
}


void Exec(HWND hWnd, LPCTSTR path, int edge) {
	if (path[0] == L'\0') return;
	if (FullScreenCheck && IsForegroundWindowFullScreen()) return;
	if (ThreadHandle != 0) return;  // When starting program

	wchar_t cmd[MAX_PATH], opt[MAX_PATH];
	cmd[0] = opt[0] = L'\0';
	for (const wchar_t *c = &path[0]; *c != L'\0'; ++c) {
		if (*c == '|') {
			lstrcpyn(cmd, path, c - &path[0] + 1);  // Increase one for \0
			wcscpy_s(opt, MAX_PATH, c + 1);
			break;
		}
	}
	if (cmd[0] == L'\0') wcscpy_s(cmd, MAX_PATH, path);
	if (wcscmp(cmd, L"{CONFIG}") == 0) {  // Special command {CONFIG}
		wcscpy_s(cmd, MAX_PATH, IniPath);
		opt[0] = L'\0';
	}
	ForegroundWindow(hWnd);

	EnterCriticalSection(&Cs);
	wcscpy_s(Cmd, MAX_LINE, cmd);
	AbsolutePath(Cmd);
	wcscpy_s(Opt, MAX_LINE, opt);
	LeaveCriticalSection(&Cs);

	wchar_t fileName[MAX_PATH];
	ShowMessage(hWnd, Path_name(fileName, cmd), edge);  // Show message first

	DWORD thread_id;
	ThreadHandle = CreateThread(NULL, 0, OpenFileThreadProc, NULL, 0, &thread_id);
	if (ThreadHandle) {
		CloseHandle(ThreadHandle);  // Release handle
		ThreadHandle = 0;
	}
}


DWORD WINAPI OpenFileThreadProc(LPVOID d) {
	wchar_t oldCurrent[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, oldCurrent);

	EnterCriticalSection(&Cs);
	wchar_t parent[MAX_PATH];
	SetCurrentDirectory(Path_parent(parent, Cmd));
	ShellExecute(NULL, NULL, Cmd, Opt, L"", SW_SHOW);
	LeaveCriticalSection(&Cs);

	SetCurrentDirectory(oldCurrent);  // for making removable disk ejectable
	ExitThread(0);
}


bool IsForegroundWindowFullScreen(void) {
	RECT s;
	HWND fw = GetForegroundWindow();
	GetWindowRect(fw, &s);

	int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
	if (s.left <= 0 && s.top <= 0 && s.right >= sw && s.bottom >= sh) {
		wchar_t cls[256];
		GetClassName(fw, cls, 256);
		if (wcscmp(cls, L"Progman") != 0 && wcscmp(cls, L"WorkerW") != 0) return true;
	}
	return false;
}


void ForegroundWindow(HWND hWnd) {
	int foregID = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
	int targetID = GetWindowThreadProcessId(hWnd, NULL);
	DWORD t;

	AttachThreadInput(targetID, foregID, TRUE);
	SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &t, 0);
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, NULL, 0);
	SetForegroundWindow(hWnd);
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, t, NULL, 0);
	AttachThreadInput(targetID, foregID, FALSE);
}


wchar_t* AbsolutePath(wchar_t *path) {
	wchar_t tmp[MAX_LINE];

	if (path[0] == L'.' && path[1] == L'\\') {
		GetModuleFileName(NULL, tmp, MAX_LINE - 1);
		Path_parentDest(tmp);
		wcscat_s(tmp, MAX_LINE, path + 1);
		wcscpy_s(path, MAX_LINE, tmp);
	}
	return path;
}


int Path_findLastOf(const wchar_t *str, wchar_t c) {
	int ret = -1;
	for (int i = 0; str[i] != L'\0'; ++i) {
		if (str[i] == c) ret = i;
	}
	return ret;
}


wchar_t* Path_name(wchar_t *ret, const wchar_t *path) {
	int pos = Path_findLastOf(path, L'\\');

	if (pos == -1) {  // When path is only file name
		wcscpy_s(ret, MAX_PATH, path);
	}
	else if (pos == 2 && path[3] == L'\0') {  // When path is root
		lstrcpyn(ret, path, 3);  // return "C:"
	}
	else {
		wcscpy_s(ret, MAX_PATH, path + pos + 1);
	}
	return ret;
}


wchar_t* Path_parent(wchar_t *ret, const wchar_t *path) {
	int pos = Path_findLastOf(path, L'\\');

	if (pos == -1) {
		ret[0] = L'\0';
	}
	else if (pos == 2 && path[3] == L'\0') {  // When path is root
		ret[0] = L'\0';
	}
	else {
		if (pos == 2) ++pos;
		lstrcpyn(ret, path, pos + 1);
	}
	return ret;
}


wchar_t* Path_parentDest(wchar_t *path) {
	int pos = Path_findLastOf(path, L'\\');

	if (pos == -1) {
		path[0] = L'\0';
	}
	else if (pos == 2 && path[3] == L'\0') {  // When path is root
		path[0] = L'\0';
	}
	else {
		if (pos == 2) ++pos;
		path[pos] = L'\0';
	}
	return path;
}
