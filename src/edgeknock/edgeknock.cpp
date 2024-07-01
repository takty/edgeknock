/**
 * Edgeknock (CPP)
 *
 * @author Takuto Yanagida
 * @version 2024-06-30
 */

#include <chrono>
using namespace std::chrono;

#include "stdafx.h"
#include "edgeknock.h"
#include "win_util.h"
#include "path.h"
#include "knock_detector.h"
#include "corner_detector.h"
#include "shell_executer.h"

constexpr auto MAX_LINE = 1024;  // Max length of path with option string
constexpr auto MAX_HOTKEY = 16;  // Max hotkey count

const wchar_t WIN_CLS[] = L"EDGEKNOCK";
knock_detector Kd;
corner_detector Cd;
wchar_t IniPath[MAX_LINE];
bool FullScreenCheck = true;
wchar_t MsgStr[MAX_LINE];
int MsgTimeLeft = 0;

int last_x = -1;
int last_y = -1;

ATOM InitApplication(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void WmCreate(HWND);
void WmPaint(HWND);
void WmMouseMove(HWND, int, int);
void WmHotkey(HWND, int);
void WmTimer(HWND);
void WmUser(HWND);
void WmClose(HWND);

void LoadConfiguration(HWND hwnd);
void SetHotkey(HWND hwnd, const wchar_t* key, int id);

void ExecuteEdge(HWND hwnd, int area, int index);
void ExecuteCorner(HWND hwnd, int corner);
void ExecuteHotkey(HWND hwnd, int id);
void Execute(HWND hwnd, const wchar_t path[], int corner, int area);
void ExtractCommandLine(const wchar_t path[], wchar_t cmd[], wchar_t opt[]);
void ShowMessage(HWND hwnd, const wchar_t* msg, int corner = -1, int area = -1);

int APIENTRY wWinMain(_In_ HINSTANCE hinstance, _In_opt_ HINSTANCE hprev_instance, _In_ LPWSTR, _In_ int) {
	::CreateMutex(nullptr, FALSE, WIN_CLS);
	if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND handle = ::FindWindow(WIN_CLS, nullptr);
		if (handle) {
			::SendMessage(handle, WM_CLOSE, 0, 0);
		}
		return FALSE;
	}
	InitApplication(hinstance);
	HWND hWnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WIN_CLS, L"", WS_BORDER | WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hinstance, nullptr);
	if (!hWnd) return FALSE;

	MSG msg;
	while (::GetMessage(&msg, nullptr, 0, 0)) {
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

ATOM InitApplication(HINSTANCE hinstance) {
	WNDCLASSEXW wcex{};
	wcex.cbSize         = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hinstance;
	wcex.hIcon          = nullptr;
	wcex.hCursor        = nullptr;
	wcex.hbrBackground  = (HBRUSH)(COLOR_INFOBK + 1);
	wcex.lpszMenuName   = nullptr;
	wcex.lpszClassName  = WIN_CLS;
	wcex.hIconSm        = nullptr;
	return ::RegisterClassExW(&wcex);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
	case WM_CREATE:        WmCreate(hwnd); break;
	case WM_PAINT:         WmPaint(hwnd); break;
	case WM_MOUSEMOVEHOOK: WmMouseMove(hwnd, (int)wparam, (int)lparam); break;
	case WM_HOTKEY:        WmHotkey(hwnd, (int)wparam); break;
	case WM_TIMER:         WmTimer(hwnd); break;
	case WM_USER:          WmUser(hwnd); break;
	case WM_CLOSE:         WmClose(hwnd); break;
	case WM_DESTROY:       ::PostQuitMessage(0); break;
	default:               return ::DefWindowProc(hwnd, message, wparam, lparam);
	}
	return 0;
}

void WmCreate(HWND hwnd) {
	::GetModuleFileName(nullptr, IniPath, MAX_PATH);
	IniPath[wcslen(IniPath) - 3] = L'\0';
	wcscat_s(IniPath, MAX_PATH, L"ini");

	LoadConfiguration(hwnd);
	win_util::watch_module_directory(hwnd, WM_USER);
	::SetTimer(hwnd, 1, 100, nullptr);
	SetOwnerWindow(hwnd);  // mh.dll
	SetHook();  // mh.dll

	ShowMessage(hwnd, L"Edgeknock");
}

void WmPaint(HWND hwnd) {
	PAINTSTRUCT paint;
	HDC hdc = ::BeginPaint(hwnd, &paint);

	RECT r;
	::GetClientRect(hwnd, &r);
	HFONT dlgFont = win_util::get_default_font(hwnd);
	::SelectObject(hdc, dlgFont);
	::SetBkMode(hdc, TRANSPARENT);
	::SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
	::DrawText(hdc, MsgStr, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

	::EndPaint(hwnd, &paint);
	::DeleteObject(dlgFont);
}

void WmMouseMove(HWND hwnd, int x, int y) {
	last_x = x;
	last_y = y;

	const int dpi = ::GetDpiForWindow(hwnd);
	const int cx  = ::GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
	const int cy  = ::GetSystemMetricsForDpi(SM_CYSCREEN, dpi);

	auto t = system_clock::now();

	int corner = Cd.detect(t, x, y, cx, cy);
	if (corner != -1) {
		ExecuteCorner(hwnd, corner);
		return;
	}
	knock_detector::area_index r = Kd.detect(t, x, y, cx, cy);
	if (knock_detector::KNOCK <= r.area) {
		ExecuteEdge(hwnd, r.area, r.index);
	}
	else if (r.area == knock_detector::READY) {
		if (IsWindowVisible(hwnd)) {
			ShowWindow(hwnd, SW_HIDE);
		}
	}
}

void WmHotkey(HWND hwnd, int id) {
	if (MAX_HOTKEY <= id) {
		return;
	}
	ExecuteHotkey(hwnd, id);
}

void WmTimer(HWND hwnd) {
	WmMouseMove(hwnd, last_x, last_y);
	if (MsgTimeLeft > 0 && --MsgTimeLeft == 0) {
		ShowWindow(hwnd, SW_HIDE);
	}
}

void WmUser(HWND hwnd) {
	ShowMessage(hwnd, L"Edgeknock - The setting is updated.");
	LoadConfiguration(hwnd);
}

void WmClose(HWND hwnd) {
	EndHook();  // mh.dll
	::DestroyWindow(hwnd);
}


// ----------------------------------------------------------------------------


void LoadConfiguration(HWND hwnd) {
	FullScreenCheck = (bool) ::GetPrivateProfileInt(L"Setting", L"FullScreenCheck", FullScreenCheck, IniPath);

	int limitTime = ::GetPrivateProfileInt(L"Setting", L"LimitTime", 300, IniPath);
	int edgeW = ::GetPrivateProfileInt(L"Setting", L"EdgeWidth", 4, IniPath);
	int neEdgeW = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidth", 18, IniPath);
	int sepL = ::GetPrivateProfileInt(L"Setting", L"Left", 3, IniPath);
	int sepR = ::GetPrivateProfileInt(L"Setting", L"Right", 3, IniPath);
	int sepT = ::GetPrivateProfileInt(L"Setting", L"Top", 3, IniPath);
	int sepB = ::GetPrivateProfileInt(L"Setting", L"Bottom", 3, IniPath);

	int edgeWL = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthLeft", edgeW, IniPath);
	int edgeWR = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthRight", edgeW, IniPath);
	int edgeWT = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthTop", edgeW, IniPath);
	int edgeWB = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthBottom", edgeW, IniPath);
	int neEdgeWL = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthLeft", neEdgeW, IniPath);
	int neEdgeWR = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthRight", neEdgeW, IniPath);
	int neEdgeWT = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthTop", neEdgeW, IniPath);
	int neEdgeWB = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthBottom", neEdgeW, IniPath);

	Kd.set_time_limit(limitTime);
	Kd.set_edge_separations(sepL, sepR, sepT, sepB);
	Kd.set_edge_widthes(edgeWL, edgeWR, edgeWT, edgeWB);
	Kd.set_no_effect_edge_widthes(neEdgeWL, neEdgeWR, neEdgeWT, neEdgeWB);

	int cornerSize = ::GetPrivateProfileInt(L"Setting", L"CornerSize", 32, IniPath);
	int delayTime = ::GetPrivateProfileInt(L"Setting", L"DelayTime", 200, IniPath);

	Cd.set_corner_size(cornerSize);
	Cd.set_delay_time(delayTime);

	for (int i = 0; i < MAX_HOTKEY; ++i) {
		wchar_t key[8], hotkey[6];
		::UnregisterHotKey(hwnd, i);
		wsprintf(key, L"Key%d", i + 1);
		::GetPrivateProfileString(L"HotKey", key, L"", hotkey, 6, IniPath);
		::SetHotkey(hwnd, hotkey, i);
	}
}

void SetHotkey(HWND hwnd, const wchar_t* key, int id) {
	if (wcslen(key) < 5) {
		return;
	}
	UINT flag = 0;
	if (key[0] == L'1') flag |= MOD_ALT;
	if (key[1] == L'1') flag |= MOD_CONTROL;
	if (key[2] == L'1') flag |= MOD_SHIFT;
	if (key[3] == L'1') flag |= MOD_WIN;
	if (flag) {
		::RegisterHotKey(hwnd, id, flag, key[4]);
	}
}


// ----------------------------------------------------------------------------


void ExecuteEdge(HWND hwnd, int area, int index) {
	wchar_t path[MAX_LINE];
	wchar_t key[8], sec[][7] = { L"Left", L"Right", L"Top", L"Bottom" };
	wsprintf(key, L"Path%d", index + 1);
	::GetPrivateProfileString(sec[area], key, L"", path, MAX_LINE, IniPath);
	Execute(hwnd, path, -1, area);
}

void ExecuteCorner(HWND hwnd, int corner) {
	wchar_t path[MAX_LINE];
	wchar_t key[8];
	wsprintf(key, L"Path%d", corner + 1);
	::GetPrivateProfileString(L"Corner", key, L"", path, MAX_LINE, IniPath);
	Execute(hwnd, path, corner, -1);
}

void ExecuteHotkey(HWND hwnd, int id) {
	wchar_t path[MAX_LINE];
	wchar_t key[8];
	wsprintf(key, L"Path%d", id + 1);
	::GetPrivateProfileString(L"HotKey", key, L"", path, MAX_LINE, IniPath);
	Execute(hwnd, path, -1, -1);
}

void Execute(HWND hwnd, const wchar_t path[], int corner, int area) {
	if (path[0] == L'\0') return;
	if (FullScreenCheck && win_util::is_foreground_window_fullscreen()) return;
	if (shell_executer::is_executing() != 0) return;  // When starting program

	win_util::set_foreground_window(hwnd);

	wchar_t cmd[MAX_PATH], opt[MAX_PATH], fn[MAX_PATH];
	ExtractCommandLine(path, cmd, opt);
	ShowMessage(hwnd, path::name(fn, cmd), corner, area);  // Show message first

	shell_executer::execute(cmd, opt);
}

void ExtractCommandLine(const wchar_t path[], wchar_t cmd[], wchar_t opt[]) {
	cmd[0] = opt[0] = L'\0';
	for (const wchar_t* c = &path[0]; *c != L'\0'; ++c) {
		if (*c == '|') {
			LPWSTR r = lstrcpyn(cmd, path, (int)(c - &path[0] + 1));  // Increase one for \0
			wcscpy_s(opt, MAX_PATH, c + 1);
			break;
		}
	}
	if (cmd[0] == L'\0') {
		wcscpy_s(cmd, MAX_PATH, path);
	}
	if (wcscmp(cmd, L"{CONFIG}") == 0) {  // Special command {CONFIG}
		wcscpy_s(cmd, MAX_PATH, IniPath);
		opt[0] = L'\0';
	}
	path::absolute_path(cmd);
}

void ShowMessage(HWND hwnd, const wchar_t* msg, int corner, int area) {
	size_t len = wcslen(msg);
	if (len == 0) {
		return;
	}
	wcscpy_s(MsgStr, MAX_LINE, msg);
	HDC hdc = ::GetDC(hwnd);
	HFONT dlgFont = win_util::get_default_font(hwnd);
	::SelectObject(hdc, dlgFont);
	SIZE font;
	::GetTextExtentPoint32(hdc, msg, (int)len, &font);

	POINT p;
	::GetPhysicalCursorPos(&p);
	const int w = font.cx + 10, h = font.cy + 10;
	const int dpi = ::GetDpiForWindow(hwnd);
	const int dw = ::GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
	const int dh = ::GetSystemMetricsForDpi(SM_CYSCREEN, dpi);
	const int mw = ::GetSystemMetricsForDpi(SM_CXCURSOR, dpi);
	const int mh = ::GetSystemMetricsForDpi(SM_CYCURSOR, dpi);

	int l = 0, t = 0;
	if (corner != -1) {
		switch (corner) {
		case 0: l = mw;          t = mh;          break;
		case 1: l = dw - w - mw; t = mh;          break;
		case 2: l = dw - w - mw; t = dh - h - mh; break;
		case 3: l = mw;          t = dh - h - mh; break;
		}
	}
	else if (area != -1) {
		switch (area) {
		case 0: l = 0;      t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
		case 1: l = dw - w; t = (p.y > dh / 2) ? (p.y - h - mh) : (p.y + mh); break;
		case 2: l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = 0;      break;
		case 3: l = (p.x > dw / 2) ? (p.x - w - mw) : (p.x + mw); t = dh - h; break;
		}
	}
	else {
		l = (dw - w) / 2;
		t = (dh - h) / 2;
	}
	MsgTimeLeft = 10;
	::MoveWindow(hwnd, l, t, w, h, FALSE);
	::ShowWindow(hwnd, SW_SHOW);
	::DeleteObject(dlgFont);
	::ReleaseDC(hwnd, hdc);
}
