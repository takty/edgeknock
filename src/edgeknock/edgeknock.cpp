/**
 * Edgeknock (CPP)
 *
 * @author Takuto Yanagida
 * @version 2024-07-07
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

void RecognizeGesture(HMONITOR hmon, HWND hwnd, const int x, const int y, const int cx, const int cy);
void ExecuteEdge(HMONITOR hmon, HWND hwnd, int area, int index);
void ExecuteCorner(HMONITOR hmon, HWND hwnd, int corner);
void ExecuteHotkey(HMONITOR hmon, HWND hwnd, int id);
void Execute(HMONITOR hmon, HWND hwnd, const wchar_t path[], int corner, int area);
void ExtractCommandLine(const wchar_t path[], wchar_t cmd[], wchar_t opt[]);
void ShowMessage(HMONITOR hmon, HWND hwnd, const wchar_t* msg, int corner = -1, int area = -1);

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
	if (!hWnd) {
		return FALSE;
	}
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

	HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ShowMessage(hmon, hwnd, L"Edgeknock");
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

typedef struct tagWPT {
	HWND hwnd;
	LONG x;
	LONG y;
} WPT;

void WmMouseMove(HWND hwnd, int x, int y) {
	last_x = x;
	last_y = y;

	WPT logical{ hwnd, x, y };

	::EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lParam) -> BOOL {
		WPT* wpt  = reinterpret_cast<WPT*>(lParam);
		HWND hwnd = wpt->hwnd;
		POINT pt  = { wpt->x, wpt->y };

		if (win_util::is_point_in_monitor(hmon, pt)) {
			MONITORINFOEX mi{};
			mi.cbSize = sizeof(MONITORINFOEX);
			if (::GetMonitorInfo(hmon, &mi)) {
				int x  = pt.x - mi.rcMonitor.left;
				int y  = pt.y - mi.rcMonitor.top;
				int cx = mi.rcMonitor.right - mi.rcMonitor.left;
				int cy = mi.rcMonitor.bottom - mi.rcMonitor.top;
				RecognizeGesture(hmon, hwnd, x, y, cx, cy);
			}
			return FALSE;
		}
		return TRUE;
		}, reinterpret_cast<LPARAM>(&logical));
}

void WmHotkey(HWND hwnd, int id) {
	if (MAX_HOTKEY <= id) {
		return;
	}
	HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ExecuteHotkey(hmon, hwnd, id);
}

void WmTimer(HWND hwnd) {
	WmMouseMove(hwnd, last_x, last_y);
	if (MsgTimeLeft > 0 && --MsgTimeLeft == 0) {
		ShowWindow(hwnd, SW_HIDE);
	}
}

void WmUser(HWND hwnd) {
	HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ShowMessage(hmon, hwnd, L"Edgeknock - The setting is updated.");
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
	int edgeW     = ::GetPrivateProfileInt(L"Setting", L"EdgeWidth", 4, IniPath);
	int neEdgeW   = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidth", 18, IniPath);
	int sepL      = ::GetPrivateProfileInt(L"Setting", L"Left", 3, IniPath);
	int sepR      = ::GetPrivateProfileInt(L"Setting", L"Right", 3, IniPath);
	int sepT      = ::GetPrivateProfileInt(L"Setting", L"Top", 3, IniPath);
	int sepB      = ::GetPrivateProfileInt(L"Setting", L"Bottom", 3, IniPath);

	int edgeWL   = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthLeft", edgeW, IniPath);
	int edgeWR   = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthRight", edgeW, IniPath);
	int edgeWT   = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthTop", edgeW, IniPath);
	int edgeWB   = ::GetPrivateProfileInt(L"Setting", L"EdgeWidthBottom", edgeW, IniPath);
	int neEdgeWL = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthLeft", neEdgeW, IniPath);
	int neEdgeWR = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthRight", neEdgeW, IniPath);
	int neEdgeWT = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthTop", neEdgeW, IniPath);
	int neEdgeWB = ::GetPrivateProfileInt(L"Setting", L"NoEffectWidthBottom", neEdgeW, IniPath);

	Kd.set_time_limit(limitTime);
	Kd.set_edge_separations(sepL, sepR, sepT, sepB);
	Kd.set_edge_widths(edgeWL, edgeWR, edgeWT, edgeWB);
	Kd.set_no_effect_edge_widths(neEdgeWL, neEdgeWR, neEdgeWT, neEdgeWB);

	int cornerSize = ::GetPrivateProfileInt(L"Setting", L"CornerSize", 8, IniPath);
	int delayTime  = ::GetPrivateProfileInt(L"Setting", L"DelayTime", 200, IniPath);

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


void RecognizeGesture(HMONITOR hmon, HWND hwnd, const int x, const int y, const int scr_w, const int scr_h) {
	auto t = system_clock::now();

	int corner = Cd.detect(t, x, y, scr_w, scr_h);
	if (corner != -1) {
		ExecuteCorner(hmon, hwnd, corner);
		return;
	}
	knock_detector::area_index r = Kd.detect(t, x, y, scr_w, scr_h);
	if (knock_detector::KNOCK <= r.area) {
		ExecuteEdge(hmon, hwnd, r.area, r.index);
	}
	else if (r.area == knock_detector::READY) {
		if (IsWindowVisible(hwnd)) {
			ShowWindow(hwnd, SW_HIDE);
		}
	}
}

void ExecuteEdge(HMONITOR hmon, HWND hwnd, int area, int index) {
	wchar_t path[MAX_LINE];
	wchar_t key[8], sec[][7] = { L"Left", L"Right", L"Top", L"Bottom" };
	wsprintf(key, L"Path%d", index + 1);
	::GetPrivateProfileString(sec[area], key, L"", path, MAX_LINE, IniPath);
	Execute(hmon, hwnd, path, -1, area);
}

void ExecuteCorner(HMONITOR hmon, HWND hwnd, int corner) {
	wchar_t path[MAX_LINE];
	wchar_t key[8];
	wsprintf(key, L"Path%d", corner + 1);
	::GetPrivateProfileString(L"Corner", key, L"", path, MAX_LINE, IniPath);
	Execute(hmon, hwnd, path, corner, -1);
}

void ExecuteHotkey(HMONITOR hmon, HWND hwnd, int id) {
	wchar_t path[MAX_LINE];
	wchar_t key[8];
	wsprintf(key, L"Path%d", id + 1);
	::GetPrivateProfileString(L"HotKey", key, L"", path, MAX_LINE, IniPath);
	Execute(hmon, hwnd, path, -1, -1);
}

void Execute(HMONITOR hmon, HWND hwnd, const wchar_t path[], int corner, int area) {
	if (path[0] == L'\0') return;
	if (FullScreenCheck && win_util::is_foreground_window_fullscreen()) return;
	if (shell_executer::is_executing() != 0) return;  // When starting program

	win_util::set_foreground_window(hwnd);

	wchar_t cmd[MAX_PATH], opt[MAX_PATH], fn[MAX_PATH];
	ExtractCommandLine(path, cmd, opt);
	ShowMessage(hmon, hwnd, path::name(fn, cmd), corner, area);  // Show message first

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

void ShowMessage(HMONITOR hmon, HWND hwnd, const wchar_t* msg, int corner, int area) {
	size_t len = wcslen(msg);
	if (len == 0) {
		return;
	}
	RECT mr = win_util::get_monitor_rect(hmon);
	::MoveWindow(hwnd, mr.left, mr.top, 0, 0, FALSE);

	SIZE font = win_util::get_text_size(hwnd, msg, len);
	const int w = font.cx + 10, h = font.cy + 10;

	POINT p;
	::GetPhysicalCursorPos(&p);
	p.x = p.x - mr.left;
	p.y = p.y - mr.top;
	int scr_w = mr.right - mr.left;
	int scr_h = mr.bottom - mr.top;

	auto [dpi_x, dpi_y] = win_util::get_monitor_dpi(hmon);
	const int off_x = ::GetSystemMetricsForDpi(SM_CXCURSOR, dpi_x);
	const int off_y = ::GetSystemMetricsForDpi(SM_CYCURSOR, dpi_y);

	int x = 0, y = 0;
	if (corner != -1) {
		switch (corner) {
		case 0: x = off_x;             y = off_y;          break;
		case 1: x = scr_w - w - off_x; y = off_y;          break;
		case 2: x = scr_w - w - off_x; y = scr_h - h - off_y; break;
		case 3: x = off_x;             y = scr_h - h - off_y; break;
		}
	}
	else if (area != -1) {
		switch (area) {
		case 0: x = 0;         y = (p.y > scr_h / 2) ? (p.y - h - off_y) : (p.y + off_y); break;
		case 1: x = scr_w - w; y = (p.y > scr_h / 2) ? (p.y - h - off_y) : (p.y + off_y); break;
		case 2: x = (p.x > scr_w / 2) ? (p.x - w - off_x) : (p.x + off_x); y = 0;         break;
		case 3: x = (p.x > scr_w / 2) ? (p.x - w - off_x) : (p.x + off_x); y = scr_h - h; break;
		}
	}
	else {
		x = (scr_w - w) / 2;
		y = (scr_h - h) / 2;
	}
	wcscpy_s(MsgStr, MAX_LINE, msg);
	MsgTimeLeft = 10;
	::MoveWindow(hwnd, x + mr.left, y + mr.top, w, h, FALSE);
	::ShowWindow(hwnd, SW_SHOW);
}
