/**
 * Window Utilities
 *
 * @author Takuto Yanagida
 * @version 2024-07-07
 */

#pragma once
#include <utility>
#include <windows.h>
#include "path.h"

class win_util {

	static DWORD WINAPI watch_module_directory_thread(LPVOID ps) {
		HWND hwnd = (HWND)((long long int*) ps)[0];
		UINT msg = (UINT)((long long int*)ps)[1];

		wchar_t path[MAX_PATH];
		::GetModuleFileName(NULL, path, MAX_PATH - 1);
		path::parent_dest(path);

		// Observe ini file writing
		HANDLE h = ::FindFirstChangeNotification(path, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		if (h == INVALID_HANDLE_VALUE) return (DWORD)-1;

		// Wait until changing
		while (true) {
			if (::WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0) {
				::SendMessage(hwnd, msg, 0, 0);  // Notify changing
			}
			else {
				break;
			}
			// Wait again
			if (!::FindNextChangeNotification(h)) break;
		}
		::FindCloseChangeNotification(h);
		return 0;
	}

public:

	static void watch_module_directory(HWND hwnd, UINT msg) {
		DWORD t_id;
		static long long int ps[] = { (long long int) hwnd, msg };
		::CreateThread(NULL, 0, win_util::watch_module_directory_thread, ps, 0, &t_id);
	}

	static void set_foreground_window(HWND hWnd) {
		int win_id = ::GetWindowThreadProcessId(GetForegroundWindow(), NULL);
		int tar_id = ::GetWindowThreadProcessId(hWnd, NULL);
		DWORD t = 0;

		::AttachThreadInput(tar_id, win_id, TRUE);
		::SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &t, 0);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, NULL, 0);
		::SetForegroundWindow(hWnd);
		::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, t, NULL, 0);
		::AttachThreadInput(tar_id, win_id, FALSE);
	}

	static bool is_foreground_window_fullscreen(void) {
		RECT s;
		HWND fw = ::GetForegroundWindow();
		::GetWindowRect(fw, &s);

		const int dpi = ::GetDpiForWindow(fw);
		const int sw = ::GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
		const int sh = ::GetSystemMetricsForDpi(SM_CYSCREEN, dpi);

		if (s.left <= 0 && s.top <= 0 && s.right >= sw && s.bottom >= sh) {
			wchar_t cls[256];
			::GetClassName(fw, cls, 256);
			if (wcscmp(cls, L"Progman") != 0 && wcscmp(cls, L"WorkerW") != 0) {
				return true;
			}
		}
		return false;
	}

	static HFONT get_default_font(HWND hwnd) {
		NONCLIENTMETRICS ncm{};
		ncm.cbSize = sizeof(ncm);
		const int dpi = ::GetDpiForWindow(hwnd);
		::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, FALSE, dpi);
		return ::CreateFontIndirect(&(ncm.lfMessageFont));
	}

	static SIZE get_text_size(HWND hwnd, const wchar_t* msg, size_t len) {
		HDC hdc = ::GetDC(hwnd);
		HFONT dlgFont = win_util::get_default_font(hwnd);
		::SelectObject(hdc, dlgFont);
		SIZE font;
		::GetTextExtentPoint32(hdc, msg, (int)len, &font);
		::DeleteObject(dlgFont);
		::ReleaseDC(hwnd, hdc);
		return font;
	}

	static bool is_point_in_monitor(HMONITOR hmon, POINT pt) {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (GetMonitorInfo(hmon, &mi)) {
			return (pt.x >= mi.rcMonitor.left && pt.x < mi.rcMonitor.right &&
				pt.y >= mi.rcMonitor.top && pt.y < mi.rcMonitor.bottom);
		}
		return false;
	}

	static POINT get_physical_point_from_logical(HMONITOR hmon, POINT logical) {
		POINT physical = logical;
		UINT dpi_x, dpi_y;
		if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
			physical.x = MulDiv(logical.x, dpi_x, 96);
			physical.y = MulDiv(logical.y, dpi_y, 96);
		}
		return physical;
	}

	static RECT get_monitor_rect(HMONITOR hmon) {
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		if (!::GetMonitorInfo(hmon, &mi)) {
			return RECT{};
		}
		return mi.rcMonitor;
	}

	static std::pair<UINT, UINT> get_monitor_dpi(HMONITOR hmon) {
		UINT dpi_x, dpi_y;
		if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
			return { dpi_x, dpi_y };
		}
		return { 96, 96 };
	}

};
