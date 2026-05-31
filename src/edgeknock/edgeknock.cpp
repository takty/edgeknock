/**
 * Edgeknock (CPP)
 *
 * @author Takuto Yanagida
 * @version 2026-05-31
 */

#include "stdafx.h"

#include <chrono>
#include <string_view>
using namespace std::chrono;

#include "gsl/gsl"
#include "lib/file_system.hpp"
#include "lib/path.hpp"
#include "lib/dc.hpp"
#include "lib/mod_dir_watch.hpp"
#include "lib/shell_exec.hpp"
#include "lib/pref.hpp"
#include "lib/win_util.hpp"
#include "lib/mh.hpp"
#include "lib/pt_mon.hpp"

#include "edgeknock.h"
#include "knock_detector.h"
#include "corner_detector.h"

constexpr auto MAX_LINE = 1024;  // Max length of path with option string
constexpr auto MAX_HOTKEY = 16;  // Max hotkey count

const wchar_t MUTEX[]       = _T("EDGEKNOCK_20260524");
const wchar_t CLASS_NAME[]  = _T("Edgeknock");
const wchar_t WINDOW_NAME[] = _T("Edgeknock");

knock_detector Kd;
corner_detector Cd;
std::wstring IniPath;
Pref pref;
bool FullScreenCheck = true;
std::wstring MsgStr;
int MsgTimeLeft = 0;

int last_x = -1;
int last_y = -1;

ATOM InitApplication(HINSTANCE, const wchar_t* class_name) noexcept;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void WmCreate(HWND);
void WmPaint(HWND) noexcept;
void WmMouseMove(HWND, int, int);
void WmHotkey(HWND, WPARAM);
void WmTimer(HWND);
void WmUser(HWND);
void WmClose(HWND) noexcept;

void LoadConfiguration(HWND hwnd);
void SetHotkey(HWND hwnd, std::wstring_view key, int id) noexcept;

void RecognizeGesture(HMONITOR hmon, HWND hwnd, const POINT pt, const SIZE scr);
void ExecuteEdge(HMONITOR hmon, HWND hwnd, int area, int index);
void ExecuteCorner(HMONITOR hmon, HWND hwnd, int corner);
void ExecuteHotkey(HMONITOR hmon, HWND hwnd, int id);
void Execute(HMONITOR hmon, HWND hwnd, std::wstring_view path, int corner, int area);
void ExtractCommandLine(std::wstring_view path, std::wstring& cmd, std::wstring& opt) noexcept;
void ShowMessage(HMONITOR hmon, HWND hwnd, std::wstring_view msg, int corner = -1, int area = -1);

int APIENTRY wWinMain(_In_ HINSTANCE hinstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	::CreateMutex(nullptr, FALSE, &MUTEX[0]);
	if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		const HWND handle = ::FindWindow(&CLASS_NAME[0], nullptr);
		if (handle) {
			::SendMessage(handle, WM_CLOSE, 0, 0);
		}
		return 0;
	}
	if (!InitApplication(hinstance, &CLASS_NAME[0])) {
		return 0;
	}
	[[gsl::suppress("con.4")]]
	const HWND hWnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, &CLASS_NAME[0], &WINDOW_NAME[0], WS_BORDER | WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hinstance, nullptr);
	if (!hWnd) {
		return 0;
	}
	MSG msg{};
	while (::GetMessage(&msg, nullptr, 0, 0)) {
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
	return gsl::narrow<int>(msg.wParam);
}

ATOM InitApplication(HINSTANCE hinstance, const wchar_t* class_name) noexcept {
	WNDCLASSEXW wc{};
	wc.cbSize         = sizeof(WNDCLASSEX);
	wc.style          = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc    = WndProc;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = 0;
	wc.hInstance      = hinstance;
	wc.hIcon          = nullptr;
	wc.hCursor        = nullptr;
	[[gsl::suppress("type.1")]]
	wc.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_INFOBK + 1);
	wc.lpszMenuName   = nullptr;
	wc.lpszClassName  = class_name;
	return ::RegisterClassExW(&wc);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
	case WM_CREATE:        WmCreate(hwnd); break;
	case WM_PAINT:         WmPaint(hwnd); break;
	case WM_MOUSEMOVEHOOK: WmMouseMove(hwnd, gsl::narrow_cast<int>(wparam), gsl::narrow_cast<int>(lparam)); break;
	case WM_HOTKEY:        WmHotkey(hwnd, wparam); break;
	case WM_TIMER:         WmTimer(hwnd); break;
	case WM_USER:          WmUser(hwnd); break;
	case WM_CLOSE:         WmClose(hwnd); break;
	case WM_DESTROY:       ::PostQuitMessage(0); break;
	default:               return ::DefWindowProc(hwnd, message, wparam, lparam);
	}
	return 0;
}

void WmCreate(HWND hwnd) {
	LoadConfiguration(hwnd);
	module_directory_watcher::watch(hwnd, WM_USER);
	::SetTimer(hwnd, 1, 100, nullptr);
	mh::set_hook(hwnd);

	HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ShowMessage(hmon, hwnd, L"Edgeknock");
}

void WmPaint(HWND hwnd) noexcept {
	PAINTSTRUCT paint;
	HDC hdc = ::BeginPaint(hwnd, &paint);

	RECT r;
	::GetClientRect(hwnd, &r);
	HFONT dlgFont = dc::get_default_font(hwnd);
	::SelectObject(hdc, dlgFont);
	::SetBkMode(hdc, TRANSPARENT);
	::SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
	::DrawText(hdc, MsgStr.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

	::EndPaint(hwnd, &paint);
	::DeleteObject(dlgFont);
}

void WmMouseMove(HWND hwnd, int x, int y) {
	last_x = x;
	last_y = y;

	HMONITOR hmon{};
	POINT pt{};
	SIZE scr{};

	if (point_in_monitor::point_in_monitor(x, y, hmon, pt, scr)) {
		RecognizeGesture(hmon, hwnd, pt, scr);
	}
}

void WmHotkey(HWND hwnd, WPARAM id) {
	if (MAX_HOTKEY <= id) {
		return;
	}
	HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	ExecuteHotkey(hmon, hwnd, gsl::narrow_cast<int>(id));
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

void WmClose(HWND hwnd) noexcept {
	mh::end_hook();
	::DestroyWindow(hwnd);
}


// ----------------------------------------------------------------------------


void LoadConfiguration(HWND hwnd) {
	pref.set_current_section(L"Setting");

	FullScreenCheck = pref.item_int(L"FullScreenCheck", FullScreenCheck);

	const int limitTime = pref.item_int(L"LimitTime", 300);
	const int edgeW     = pref.item_int(L"EdgeWidth", 4);
	const int neEdgeW   = pref.item_int(L"NoEffectWidth", 18);
	const int sepL      = pref.item_int(L"Left", 3);
	const int sepR      = pref.item_int(L"Right", 3);
	const int sepT      = pref.item_int(L"Top", 3);
	const int sepB      = pref.item_int(L"Bottom", 3);

	const int edgeWL   = pref.item_int(L"EdgeWidthLeft", edgeW);
	const int edgeWR   = pref.item_int(L"EdgeWidthRight", edgeW);
	const int edgeWT   = pref.item_int(L"EdgeWidthTop", edgeW);
	const int edgeWB   = pref.item_int(L"EdgeWidthBottom", edgeW);
	const int neEdgeWL = pref.item_int(L"NoEffectWidthLeft", neEdgeW);
	const int neEdgeWR = pref.item_int(L"NoEffectWidthRight", neEdgeW);
	const int neEdgeWT = pref.item_int(L"NoEffectWidthTop", neEdgeW);
	const int neEdgeWB = pref.item_int(L"NoEffectWidthBottom", neEdgeW);

	Kd.set_time_limit(limitTime);
	Kd.set_edge_separations(sepL, sepR, sepT, sepB);
	Kd.set_edge_widths(edgeWL, edgeWR, edgeWT, edgeWB);
	Kd.set_no_effect_edge_widths(neEdgeWL, neEdgeWR, neEdgeWT, neEdgeWB);

	const int cornerSize = pref.item_int(L"CornerSize", 8);
	const int delayTime  = pref.item_int(L"DelayTime", 200);

	Cd.set_corner_size(cornerSize);
	Cd.set_delay_time(delayTime);

	pref.set_current_section(L"HotKey");
	for (int i = 0; i < MAX_HOTKEY; ++i) {
		::UnregisterHotKey(hwnd, i);
		const std::wstring hotkey = pref.item(L"Key" + std::to_wstring(i + 1), L"");
		::SetHotkey(hwnd, hotkey, i);
	}
}

void SetHotkey(HWND hwnd, std::wstring_view key, int id) noexcept {
	if (key.size() < 5) return;

	UINT flag = 0;
	if (key.at(0) == L'1') flag |= MOD_ALT;
	if (key.at(1) == L'1') flag |= MOD_CONTROL;
	if (key.at(2) == L'1') flag |= MOD_SHIFT;
	if (key.at(3) == L'1') flag |= MOD_WIN;
	if (flag) ::RegisterHotKey(hwnd, id, flag, key.at(4));
}


// ----------------------------------------------------------------------------


void RecognizeGesture(HMONITOR hmon, HWND hwnd, const POINT pt, const SIZE scr) {
	const auto t = system_clock::now();

	const int corner = Cd.detect(t, pt.x, pt.y, scr.cx, scr.cy);
	if (corner != -1) {
		ExecuteCorner(hmon, hwnd, corner);
		return;
	}
	const knock_detector::area_index r = Kd.detect(t, pt.x, pt.y, scr.cx, scr.cy);
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
	static const std::array<std::wstring, 4> sec = { L"Left", L"Right", L"Top", L"Bottom" };
	pref.set_current_section(sec.at(area));
	const std::wstring path = pref.item(L"Path" + std::to_wstring(index + 1), L"");
	Execute(hmon, hwnd, path, -1, area);
}

void ExecuteCorner(HMONITOR hmon, HWND hwnd, int corner) {
	pref.set_current_section(L"Corner");
	const std::wstring path = pref.item(L"Path" + std::to_wstring(corner + 1), L"");
	Execute(hmon, hwnd, path, corner, -1);
}

void ExecuteHotkey(HMONITOR hmon, HWND hwnd, int id) {
	pref.set_current_section(L"HotKey");
	const std::wstring path = pref.item(L"Path" + std::to_wstring(id + 1), L"");
	Execute(hmon, hwnd, path, -1, -1);
}

void Execute(HMONITOR hmon, HWND hwnd, std::wstring_view path, int corner, int area) {
	if (path.empty()) {
		return;
	}
	if (FullScreenCheck && window_utilities::is_foreground_window_fullscreen()) return;
	if (shell_executer::is_executing() != 0) return;  // When starting program

	window_utilities::set_foreground_window(hwnd);

	std::wstring cmd, opt;
	ExtractCommandLine(path, cmd, opt);
	auto fn = path::name(cmd);
	ShowMessage(hmon, hwnd, fn, corner, area);  // Show message first

	shell_executer::execute(cmd, opt, hmon);
}

void ExtractCommandLine(std::wstring_view path, std::wstring& cmd, std::wstring& opt) noexcept {
	cmd.clear();
	opt.clear();
	if (path.empty()) {
		return;
	}
	const auto pos = path.find(L'|');
	if (pos == std::wstring_view::npos) {
		cmd = path;
	}
	else {
		cmd = std::wstring{ path.substr(0, pos) };
		opt = std::wstring{ path.substr(pos + 1) };
	}
	if (cmd == L"{CONFIG}") {  // Special command {CONFIG}
		cmd = IniPath;
		opt.clear();
	}
	cmd = path::absolute_path(cmd, file_system::module_file_path());
}

void ShowMessage(HMONITOR hmon, HWND hwnd, std::wstring_view msg, int corner, int area) {
	if (msg.empty()) {
		return;
	}
	RECT mr{};
	point_in_monitor::get_monitor_rect(hmon, mr);
	::MoveWindow(hwnd, mr.left, mr.top, 0, 0, FALSE);

	const SIZE font = dc::get_text_size(hwnd, msg);
	const int w = font.cx + 10, h = font.cy + 10;

	POINT p;
	::GetPhysicalCursorPos(&p);
	p.x = p.x - mr.left;
	p.y = p.y - mr.top;
	const int scr_w = mr.right - mr.left;
	const int scr_h = mr.bottom - mr.top;

	auto [dpi_x, dpi_y] = window_utilities::get_monitor_dpi(hmon);
	const int off_x = ::GetSystemMetricsForDpi(SM_CXCURSOR, dpi_x);
	const int off_y = ::GetSystemMetricsForDpi(SM_CYCURSOR, dpi_y);

	int x = 0, y = 0;
	if (corner != -1) {
		switch (corner) {
		case 0: x = off_x;             y = off_y;             break;
		case 1: x = scr_w - w - off_x; y = off_y;             break;
		case 2: x = scr_w - w - off_x; y = scr_h - h - off_y; break;
		case 3: x = off_x;             y = scr_h - h - off_y; break;
		default:  // do nothing
		}
	}
	else if (area != -1) {
		switch (area) {
		case 0: x = 0;         y = (p.y > scr_h / 2) ? (p.y - h - off_y) : (p.y + off_y); break;
		case 1: x = scr_w - w; y = (p.y > scr_h / 2) ? (p.y - h - off_y) : (p.y + off_y); break;
		case 2: x = (p.x > scr_w / 2) ? (p.x - w - off_x) : (p.x + off_x); y = 0;         break;
		case 3: x = (p.x > scr_w / 2) ? (p.x - w - off_x) : (p.x + off_x); y = scr_h - h; break;
		default:  // do nothing
		}
	}
	else {
		x = (scr_w - w) / 2;
		y = (scr_h - h) / 2;
	}
	MsgStr.assign(msg);
	MsgTimeLeft = 10;
	::MoveWindow(hwnd, x + mr.left, y + mr.top, w, h, FALSE);
	::ShowWindow(hwnd, SW_SHOW);
}
