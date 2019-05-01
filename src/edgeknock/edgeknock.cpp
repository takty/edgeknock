/**
 *
 * Edgeknock (CPP)
 *
 * @author Takuto Yanagida
 * @version 2019-05-01
 *
 */


#include "stdafx.h"
#include "edgeknock.h"
#include "win_util.h"
#include "path.h"
#include "knock_detector.h"
#include "corner_detector.h"

#define MAX_LINE 1024  // Max length of path with option string
#define MAX_HOTKEY 16  // Max hotkey count


const wchar_t WIN_CLS[] = L"EDGEKNOCK";
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
ATOM InitApplication(HINSTANCE);

knock_detector kd;
corner_detector cd;

CRITICAL_SECTION Cs;
HANDLE ThreadHandle;

wchar_t IniPath[MAX_LINE];

BOOL FullScreenCheck = TRUE;
wchar_t Cmd[MAX_LINE], Opt[MAX_LINE];

wchar_t MsgStr[MAX_LINE];
int MsgTimeLeft = 0;

void WmCreate(HWND hWnd);
void WmPaint(HWND hWnd);
void WmMouseMoveHook(HWND hWnd, int x, int y);
void WmHotKey(HWND hWnd, int id);

void LoadIniFile(HWND hWnd);
void SetHotkey(HWND hWnd, const wchar_t *key, int id);
DWORD WINAPI CheckIniFileThreadProc(LPVOID d);

wchar_t* LoadPath(wchar_t *path, int corner, int area, int index);
void Exec(HWND hWnd, LPCTSTR path, int corner, int area);
DWORD WINAPI OpenFileThreadProc(LPVOID d);
void ShowMessage(HWND hWnd, const wchar_t *msg, int corner = -1, int area = -1);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
	::CreateMutex(NULL, FALSE, WIN_CLS);
	if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND handle = ::FindWindow(WIN_CLS, NULL);
		if (handle) ::SendMessage(handle, WM_CLOSE, 0, 0);
		return FALSE;
	}
	InitApplication(hInstance);
	HWND hWnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WIN_CLS, L"", WS_BORDER | WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd) return FALSE;

	::InitializeCriticalSection(&Cs);

	MSG msg;
    while (::GetMessage(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
	::DeleteCriticalSection(&Cs);
    return (int) msg.wParam;
}

ATOM InitApplication(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;
	wcex.cbSize         = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = nullptr;
	wcex.hCursor        = nullptr;
	wcex.hbrBackground  = (HBRUSH)(COLOR_INFOBK + 1);
	wcex.lpszMenuName   = nullptr;
	wcex.lpszClassName  = WIN_CLS;
	wcex.hIconSm        = nullptr;
	return ::RegisterClassExW(&wcex);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:        WmCreate(hWnd); break;
	case WM_PAINT:         WmPaint(hWnd); break;
	case WM_MOUSEMOVEHOOK: WmMouseMoveHook(hWnd, (int) wParam, (int) lParam); break;
	case WM_HOTKEY:        WmHotKey(hWnd, (int) wParam); break;
	case WM_TIMER:
		if (MsgTimeLeft > 0 && --MsgTimeLeft == 0) ShowWindow(hWnd, SW_HIDE);
		break;
	case WM_USER:
		ShowMessage(hWnd, L"Edgeknock - The setting is updated.");
		LoadIniFile(hWnd);
		break;
	case WM_CLOSE:
		EndHook();  // mh.dll
		::DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void WmCreate(HWND hWnd) {
	::GetModuleFileName(NULL, IniPath, MAX_PATH);
	IniPath[wcslen(IniPath) - 3] = L'\0';
	wcscat_s(IniPath, MAX_PATH, L"ini");

	LoadIniFile(hWnd);
	DWORD thread_id;
	::CreateThread(NULL, 0, CheckIniFileThreadProc, &hWnd, 0, &thread_id);
	::SetTimer(hWnd, 1, 100, NULL);
	SetOwnerWindow(hWnd);  // mh.dll
	SetHook();  // mh.dll

	ShowMessage(hWnd, L"Edgeknock");
}

void WmPaint(HWND hWnd) {
	PAINTSTRUCT paint;
	HDC hdc = ::BeginPaint(hWnd, &paint);

	RECT r;
	::GetClientRect(hWnd, &r);
	HFONT dlgFont = win_util::get_default_font();
	::SelectObject(hdc, dlgFont);
	::SetBkMode(hdc, TRANSPARENT);
	::SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
	::DrawText(hdc, MsgStr, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

	::EndPaint(hWnd, &paint);
	::DeleteObject(dlgFont);
}

void WmMouseMoveHook(HWND hWnd, int x, int y) {
	wchar_t path[MAX_LINE];
	const int cx = ::GetSystemMetrics(SM_CXSCREEN);
	const int cy = ::GetSystemMetrics(SM_CYSCREEN);

	// Corner action
	int corner = cd.detect(x, y, cx, cy);
	if (corner != -1) {
		LoadPath(path, corner, -1, -1);
		Exec(hWnd, path, corner, -1);
		return;
	}
	// Knock action
	knock_detector::area_index r = kd.detect(x, y, cx, cy);
	if (knock_detector::KNOCK <= r.area) {
		LoadPath(path, -1, r.area, r.index);
		Exec(hWnd, path, -1, r.area);
	} else if (r.area == knock_detector::READY) {
		if (IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_HIDE);
	}
}

void WmHotKey(HWND hWnd, int id) {
	if (MAX_HOTKEY <= id) return;
	wchar_t path[MAX_LINE], key[8];
	wsprintf(key, L"Path%d", id + 1);
	::GetPrivateProfileString(L"HotKey", key, L"", path, MAX_LINE, IniPath);
	Exec(hWnd, path, -1, -1);
}




// ----------------------------------------------------------------------------




void LoadIniFile(HWND hWnd) {
	FullScreenCheck = ::GetPrivateProfileInt(L"Setting", L"FullScreenCheck", FullScreenCheck, IniPath);

	int limitTime = ::GetPrivateProfileInt(L"Setting", L"LimitTime", 300, IniPath);
	int edgeW = ::GetPrivateProfileInt(L"Setting", L"EdgeWidth", 1, IniPath);
	int neEdgeW = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidth", 16, IniPath);
	int sepL = ::GetPrivateProfileInt(L"Setting", L"Left", 3, IniPath);
	int sepR = ::GetPrivateProfileInt(L"Setting", L"Right", 3, IniPath);
	int sepT = ::GetPrivateProfileInt(L"Setting", L"Top", 3, IniPath);
	int sepB = ::GetPrivateProfileInt(L"Setting", L"Bottom", 3, IniPath);

	int edgeWL = ::GetPrivateProfileInt(L"Setting", L"LeftEdgeWidth", edgeW, IniPath);
	int edgeWR = ::GetPrivateProfileInt(L"Setting", L"RightEdgeWidth", edgeW, IniPath);
	int edgeWT = ::GetPrivateProfileInt(L"Setting", L"TopEdgeWidth", edgeW, IniPath);
	int edgeWB = ::GetPrivateProfileInt(L"Setting", L"BottomEdgeWidth", edgeW, IniPath);
	int neEdgeWL = ::GetPrivateProfileInt(L"Setting", L"LeftNoEffectWidth", neEdgeW, IniPath);
	int neEdgeWR = ::GetPrivateProfileInt(L"Setting", L"RightNoEffectWidth", neEdgeW, IniPath);
	int neEdgeWT = ::GetPrivateProfileInt(L"Setting", L"TopNoEffectWidth", neEdgeW, IniPath);
	int neEdgeWB = ::GetPrivateProfileInt(L"Setting", L"BottomNoEffectWidth", neEdgeW, IniPath);

	kd.set_time_limit(limitTime);
	kd.set_edge_separations(sepL, sepR, sepT, sepB);
	kd.set_edge_widthes(edgeWL, edgeWR, edgeWT, edgeWB);
	kd.set_no_effect_edge_widthes(neEdgeWL, neEdgeWR, neEdgeWT, neEdgeWB);

	int cornerSize = ::GetPrivateProfileInt(L"Setting", L"CornerSize", 32, IniPath);

	cd.set_corner_size(cornerSize);

	for (int i = 0; i < MAX_HOTKEY; ++i) {
		wchar_t key[8], hotkey[6];
		::UnregisterHotKey(hWnd, i);
		wsprintf(key, L"Key%d", i + 1);
		::GetPrivateProfileString(L"HotKey", key, L"", hotkey, 6, IniPath);
		::SetHotkey(hWnd, hotkey, i);
	}
}

void SetHotkey(HWND hWnd, const wchar_t *key, int id) {
	if (wcslen(key) < 5) return;
	UINT flag = 0;
	if (key[0] == L'1') flag |= MOD_ALT;
	if (key[1] == L'1') flag |= MOD_CONTROL;
	if (key[2] == L'1') flag |= MOD_SHIFT;
	if (key[3] == L'1') flag |= MOD_WIN;
	if (flag) ::RegisterHotKey(hWnd, id, flag, key[4]);
}

DWORD WINAPI CheckIniFileThreadProc(LPVOID d) {
	HWND hWnd = *(HWND*)d;
	wchar_t path[MAX_PATH];
	::GetModuleFileName(NULL, path, MAX_PATH - 1);
	path::parent_dest(path);

	// Observe ini file writing
	HANDLE handle = ::FindFirstChangeNotification(path, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (handle == INVALID_HANDLE_VALUE) return (DWORD)-1;

	// Wait until changing
	while (true) {
		if (::WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0) {
			::SendMessage(hWnd, WM_USER, 0, 0);  // Notify changing
		} else {
			break;
		}
		// Wait again
		if (!::FindNextChangeNotification(handle)) break;
	}
	::FindCloseChangeNotification(handle);
	return 0;
}




// ----------------------------------------------------------------------------




wchar_t* LoadPath(wchar_t *path, int corner, int area, int index) {
	wchar_t key[8];
	wchar_t sec[][7] = { L"Left", L"Right", L"Top", L"Bottom" };

	if (corner != -1) {
		wsprintf(key, L"Path%d", corner + 1);
		::GetPrivateProfileString(L"Corner", key, L"", path, MAX_LINE, IniPath);
	} else {
		wsprintf(key, L"Path%d", index + 1);
		::GetPrivateProfileString(sec[area], key, L"", path, MAX_LINE, IniPath);
	}
	return path;
}

void Exec(HWND hWnd, LPCTSTR path, int corner, int area) {
	if (path[0] == L'\0') return;
	if (FullScreenCheck && win_util::is_foreground_window_fullscreen()) return;
	if (ThreadHandle != 0) return;  // When starting program

	wchar_t cmd[MAX_PATH], opt[MAX_PATH];
	cmd[0] = opt[0] = L'\0';
	for (const wchar_t *c = &path[0]; *c != L'\0'; ++c) {
		if (*c == '|') {
			LPWSTR r = lstrcpyn(cmd, path, (int) (c - &path[0] + 1));  // Increase one for \0
			wcscpy_s(opt, MAX_PATH, c + 1);
			break;
		}
	}
	if (cmd[0] == L'\0') wcscpy_s(cmd, MAX_PATH, path);
	if (wcscmp(cmd, L"{CONFIG}") == 0) {  // Special command {CONFIG}
		wcscpy_s(cmd, MAX_PATH, IniPath);
		opt[0] = L'\0';
	}
	win_util::set_foreground_window(hWnd);

	::EnterCriticalSection(&Cs);
	wcscpy_s(Cmd, MAX_LINE, cmd);
	path::absolute_path(Cmd);
	wcscpy_s(Opt, MAX_LINE, opt);
	::LeaveCriticalSection(&Cs);

	wchar_t fileName[MAX_PATH];
	ShowMessage(hWnd, path::name(fileName, cmd), corner, area);  // Show message first

	DWORD thread_id;
	ThreadHandle = ::CreateThread(NULL, 0, OpenFileThreadProc, NULL, 0, &thread_id);
	if (ThreadHandle) {
		::CloseHandle(ThreadHandle);  // Release handle
		ThreadHandle = 0;
	}
}

DWORD WINAPI OpenFileThreadProc(LPVOID d) {
	wchar_t oldCurrent[MAX_PATH];
	::GetCurrentDirectory(MAX_PATH, oldCurrent);

	::EnterCriticalSection(&Cs);
	wchar_t parent[MAX_PATH];
	::SetCurrentDirectory(path::parent(parent, Cmd));
	::ShellExecute(NULL, NULL, Cmd, Opt, L"", SW_SHOW);
	::LeaveCriticalSection(&Cs);

	::SetCurrentDirectory(oldCurrent);  // for making removable disk ejectable
	::ExitThread(0);
}




// ----------------------------------------------------------------------------




void ShowMessage(HWND hWnd, const wchar_t *msg, int corner, int area) {
	size_t len = wcslen(msg);
	if (len == 0) return;

	wcscpy_s(MsgStr, MAX_LINE, msg);
	HDC hdc = ::GetDC(hWnd);
	HFONT dlgFont = win_util::get_default_font();
	::SelectObject(hdc, dlgFont);
	SIZE font;
	::GetTextExtentPoint32(hdc, msg, (int) len, &font);

	POINT p;
	::GetPhysicalCursorPos(&p);
	int w = font.cx + 10, h = font.cy + 10;
	int dw = ::GetSystemMetrics(SM_CXSCREEN), dh = ::GetSystemMetrics(SM_CYSCREEN);
	int mw = ::GetSystemMetrics(SM_CXCURSOR), mh = ::GetSystemMetrics(SM_CYCURSOR);

	int l = 0, t = 0;
	if (corner != -1) {
		switch (corner) {
		case 0: l = mw;          t = mh;          break;
		case 1: l = dw - w - mw; t = mh;          break;
		case 2: l = dw - w - mw; t = dh - h - mh; break;
		case 3: l = mw;          t = dh - h - mh; break;
		}
	} else if (area != -1) {
		switch (area) {
		case 0: l = 0;      t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
		case 1: l = dw - w; t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
		case 2: l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = 0;      break;
		case 3: l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = dh - h; break;
		}
	} else {
		l = (dw - w) / 2;
		t = (dh - h) / 2;
	}
	::MoveWindow(hWnd, l, t, w, h, FALSE);
	MsgTimeLeft = 10;
	::ShowWindow(hWnd, SW_SHOW);
	::DeleteObject(dlgFont);
}
